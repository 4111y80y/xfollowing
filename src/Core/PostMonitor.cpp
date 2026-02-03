#include "PostMonitor.h"
#include <QJsonArray>
#include <QJsonDocument>

PostMonitor::PostMonitor(QObject* parent)
    : QObject(parent) {
}

QString PostMonitor::buildKeywordsArray(const QList<Keyword>& keywords) {
    QJsonArray arr;
    for (const auto& kw : keywords) {
        if (kw.isEnabled) {
            arr.append(kw.text);
        }
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString PostMonitor::getMonitorScript(const QList<Keyword>& keywords) {
    QString keywordsJson = buildKeywordsArray(keywords);

    QString script = R"(
(function() {
    // 如果已经注入过，先移除旧的
    if (window.xfollowingObserver) {
        window.xfollowingObserver.disconnect();
    }
    if (window.xfollowingProcessedIds) {
        // 保留已处理的ID，避免重复
    } else {
        window.xfollowingProcessedIds = new Set();
    }

    const keywords = %KEYWORDS%;

    function parsePost(article) {
        try {
            // 获取作者信息
            const authorLinks = article.querySelectorAll('a[href^="/"]');
            let authorHandle = '';
            let authorName = '';

            for (const link of authorLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    authorHandle = href.substring(1);
                    authorName = link.innerText || authorHandle;
                    break;
                }
            }

            if (!authorHandle) return null;

            // 检查是否是蓝V用户（验证徽章）
            // 蓝V徽章通常在用户名旁边，有data-testid="icon-verified"或包含验证SVG
            const verifiedBadge = article.querySelector('[data-testid="icon-verified"]') ||
                                  article.querySelector('svg[aria-label="Verified account"]') ||
                                  article.querySelector('svg[aria-label="已认证帐号"]') ||
                                  article.querySelector('[aria-label="Verified account"]') ||
                                  article.querySelector('[aria-label="已认证帐号"]');

            if (!verifiedBadge) {
                // 不是蓝V用户，跳过
                return null;
            }

            // 获取帖子内容
            const contentDiv = article.querySelector('[data-testid="tweetText"]');
            const content = contentDiv ? contentDiv.innerText : '';

            if (!content) return null;

            // 检查是否匹配关键词
            let matchedKeyword = null;
            for (const kw of keywords) {
                if (content.toLowerCase().includes(kw.toLowerCase())) {
                    matchedKeyword = kw;
                    break;
                }
            }

            if (!matchedKeyword) return null;

            // 获取帖子URL和时间
            const statusLink = article.querySelector('a[href*="/status/"]');
            const postUrl = statusLink ? 'https://x.com' + statusLink.getAttribute('href') : '';
            const postId = postUrl.split('/status/')[1]?.split('?')[0] || '';

            if (!postId) return null;

            // 获取帖子发布时间
            const timeElement = article.querySelector('time');
            const postTime = timeElement ? timeElement.getAttribute('datetime') : '';

            // 提取帖子中@提及的用户
            const mentionedUsers = [];
            const mentionLinks = contentDiv ? contentDiv.querySelectorAll('a[href^="/"]') : [];
            for (const link of mentionLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    const mentionHandle = href.substring(1);
                    // 排除作者自己和重复的用户
                    if (mentionHandle !== authorHandle && !mentionedUsers.includes(mentionHandle)) {
                        mentionedUsers.push(mentionHandle);
                    }
                }
            }

            return {
                postId: postId,
                authorHandle: authorHandle,
                authorName: authorName,
                authorUrl: 'https://x.com/' + authorHandle,
                content: content,
                postUrl: postUrl,
                postTime: postTime,
                matchedKeyword: matchedKeyword,
                mentionedUsers: mentionedUsers
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse error:', e);
            return null;
        }
    }

    function scanPosts() {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        const newPosts = [];

        articles.forEach(article => {
            const statusLink = article.querySelector('a[href*="/status/"]');
            const postId = statusLink?.getAttribute('href')?.split('/status/')[1]?.split('?')[0];

            if (postId && !window.xfollowingProcessedIds.has(postId)) {
                window.xfollowingProcessedIds.add(postId);
                const post = parsePost(article);
                if (post) {
                    // 添加原帖子作者
                    newPosts.push(post);

                    // 为每个@提及的用户创建一条记录
                    if (post.mentionedUsers && post.mentionedUsers.length > 0) {
                        for (const mentionHandle of post.mentionedUsers) {
                            // 检查是否已处理过这个用户
                            const mentionKey = 'mention_' + mentionHandle;
                            if (!window.xfollowingProcessedIds.has(mentionKey)) {
                                window.xfollowingProcessedIds.add(mentionKey);
                                newPosts.push({
                                    postId: post.postId + '_mention_' + mentionHandle,
                                    authorHandle: mentionHandle,
                                    authorName: '@' + mentionHandle,
                                    authorUrl: 'https://x.com/' + mentionHandle,
                                    content: '[被@] 来自 @' + post.authorHandle + ' 的帖子',
                                    postUrl: post.postUrl,
                                    postTime: post.postTime,
                                    matchedKeyword: post.matchedKeyword + ' (被@)'
                                });
                            }
                        }
                    }
                }
            }
        });

        if (newPosts.length > 0) {
            console.log('XFOLLOWING_NEW_POSTS:' + JSON.stringify(newPosts));
        }
    }

    // 初始扫描 - 等待页面加载完成后多次扫描确保采集第一页
    function initialScan() {
        let scanCount = 0;
        const maxScans = 5;

        function doScan() {
            scanCount++;
            console.log('[XFOLLOW] Initial scan attempt ' + scanCount + '/' + maxScans);
            scanPosts();

            if (scanCount < maxScans) {
                setTimeout(doScan, 1500);
            }
        }

        // 等待2秒后开始第一次扫描
        setTimeout(doScan, 2000);
    }

    // 定时扫描 - 每5秒扫描一次，捕获页面刷新的新帖子
    if (window.xfollowingInterval) {
        clearInterval(window.xfollowingInterval);
    }
    window.xfollowingInterval = setInterval(() => {
        scanPosts();
    }, 5000);

    // 使用MutationObserver监听DOM变化
    window.xfollowingObserver = new MutationObserver(() => {
        scanPosts();
    });

    window.xfollowingObserver.observe(document.body, {
        childList: true,
        subtree: true
    });

    // 执行初始扫描
    initialScan();

    // 检测登录状态 - 如果页面上有帖子，说明用户已登录
    function checkLoginStatus() {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        if (articles.length > 0) {
            console.log('XFOLLOWING_USER_LOGGED_IN');
        } else {
            // 3秒后再检查一次
            setTimeout(checkLoginStatus, 3000);
        }
    }
    setTimeout(checkLoginStatus, 2000);

    console.log('[XFOLLOW] Monitor script injected, keywords:', keywords);
})();
)";

    script = script.replace("%KEYWORDS%", keywordsJson);
    return script;
}

QString PostMonitor::getFollowersMonitorScript() {
    QString script = R"(
(function() {
    // 避免重复注入
    if (window.xfollowingFollowersProcessedIds) {
        // 保留已处理的ID
    } else {
        window.xfollowingFollowersProcessedIds = new Set();
    }

    function parseFollower(userCell) {
        try {
            // 获取用户信息
            const userLinks = userCell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            let userName = '';

            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    // 获取用户名（通常在第一个span中）
                    const nameSpan = link.querySelector('span');
                    userName = nameSpan ? nameSpan.innerText : userHandle;
                    break;
                }
            }

            if (!userHandle) return null;

            // 检查是否是蓝V用户
            const verifiedBadge = userCell.querySelector('[data-testid="icon-verified"]') ||
                                  userCell.querySelector('svg[aria-label="Verified account"]') ||
                                  userCell.querySelector('svg[aria-label="已认证帐号"]') ||
                                  userCell.querySelector('[aria-label="Verified account"]') ||
                                  userCell.querySelector('[aria-label="已认证帐号"]');
            if (!verifiedBadge) return null;

            // 检查是否有"Follows you"标签（如果有则跳过）
            const allSpans = userCell.querySelectorAll('span');
            for (const span of allSpans) {
                const text = span.innerText.toLowerCase().trim();
                if (text === 'follows you' || text.includes('follows you') ||
                    text === '关注了你' || text.includes('关注了你')) {
                    return null;  // 已关注我，跳过
                }
            }

            return {
                authorHandle: userHandle,
                authorName: userName,
                authorUrl: 'https://x.com/' + userHandle
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse follower error:', e);
            return null;
        }
    }

    function scanFollowers() {
        // 查找用户列表中的所有用户单元格
        const userCells = document.querySelectorAll('[data-testid="UserCell"]');
        const newFollowers = [];

        userCells.forEach(cell => {
            // 获取用户handle作为唯一标识
            const userLinks = cell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    break;
                }
            }

            if (userHandle && !window.xfollowingFollowersProcessedIds.has(userHandle)) {
                window.xfollowingFollowersProcessedIds.add(userHandle);
                const follower = parseFollower(cell);
                if (follower) {
                    newFollowers.push(follower);
                }
            }
        });

        if (newFollowers.length > 0) {
            console.log('XFOLLOWING_NEW_FOLLOWERS:' + JSON.stringify(newFollowers));
        }
    }

    // 定时扫描
    if (window.xfollowingFollowersInterval) {
        clearInterval(window.xfollowingFollowersInterval);
    }
    window.xfollowingFollowersInterval = setInterval(scanFollowers, 3000);

    // 初始扫描
    setTimeout(scanFollowers, 2000);

    console.log('[XFOLLOW] Followers monitor script injected');
})();
)";

    return script;
}

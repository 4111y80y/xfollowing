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

            // 获取帖子URL
            const statusLink = article.querySelector('a[href*="/status/"]');
            const postUrl = statusLink ? 'https://x.com' + statusLink.getAttribute('href') : '';
            const postId = postUrl.split('/status/')[1]?.split('?')[0] || '';

            if (!postId) return null;

            return {
                postId: postId,
                authorHandle: authorHandle,
                authorName: authorName,
                authorUrl: 'https://x.com/' + authorHandle,
                content: content,
                postUrl: postUrl,
                matchedKeyword: matchedKeyword
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
                    newPosts.push(post);
                }
            }
        });

        if (newPosts.length > 0) {
            console.log('XFOLLOWING_NEW_POSTS:' + JSON.stringify(newPosts));
        }
    }

    // 使用MutationObserver监听DOM变化
    window.xfollowingObserver = new MutationObserver(() => {
        scanPosts();
    });

    window.xfollowingObserver.observe(document.body, {
        childList: true,
        subtree: true
    });

    // 初始扫描
    setTimeout(scanPosts, 1000);

    console.log('[XFOLLOW] Monitor script injected, keywords:', keywords);
})();
)";

    script.replace("%KEYWORDS%", keywordsJson);
    return script;
}

#include "AutoFollower.h"

AutoFollower::AutoFollower(QObject* parent)
    : QObject(parent) {
}

QString AutoFollower::getFollowScript() {
    QString script = R"(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';
    let retryCount = 0;
    const maxRetries = 2;

    function checkIfOwnProfile() {
        // 检查是否是自己的页面（有编辑资料按钮）
        const editProfileLink = document.querySelector('a[href="/settings/profile"]') ||
                                document.querySelector('a[href*="/settings/profile"]');
        if (editProfileLink) {
            return true;
        }

        // 检查是否有"Edit profile"或"编辑个人资料"按钮
        const buttons = document.querySelectorAll('a, button, [role="button"]');
        for (const btn of buttons) {
            const text = btn.innerText.toLowerCase();
            if (text === 'edit profile' || text === '编辑个人资料' || text === 'set up profile') {
                return true;
            }
        }
        return false;
    }

    function checkIfFollowing() {
        // 检查是否已经变成Following状态
        const buttons = document.querySelectorAll('[role="button"]');

        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();

            // 检查是否已经关注（Following状态）
            if (testId && testId.includes('-unfollow')) {
                return true;
            }
            if (btnText === 'following' || btnText === '正在关注') {
                return true;
            }
        }
        return false;
    }

    function findFollowButton() {
        // 查找关注按钮
        const buttons = document.querySelectorAll('[role="button"]');

        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');

            // 检查是否是关注按钮（未关注状态）
            if (testId && testId.includes('-follow') && !testId.includes('-unfollow')) {
                const btnText = btn.innerText.toLowerCase();
                if (btnText === 'follow' || btnText === '关注') {
                    return btn;
                }
            }
        }
        return null;
    }

    function findAndClickFollow() {
        // 先检查是否是自己的页面
        if (checkIfOwnProfile()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }

        // 检查是否已经是Following状态
        if (checkIfFollowing()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }

        const btn = findFollowButton();
        if (btn) {
            console.log('[XFOLLOW] Found follow button, clicking... (attempt ' + (retryCount + 1) + ')');
            btn.click();

            // 等待后检查是否成功
            setTimeout(verifyFollowSuccess, 2000);
        } else {
            // 没找到按钮，可能是自己的页面或已关注
            if (checkIfOwnProfile()) {
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            } else {
                console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
            }
        }
    }

    function verifyFollowSuccess() {
        if (checkIfFollowing()) {
            // 成功关注
            console.log('XFOLLOWING_FOLLOW_SUCCESS:' + userHandle);
        } else {
            // 关注失败，尝试重试
            retryCount++;
            if (retryCount <= maxRetries) {
                console.log('[XFOLLOW] Follow not confirmed, retrying... (' + retryCount + '/' + maxRetries + ')');
                setTimeout(findAndClickFollow, 1500);
            } else {
                // 重试次数用完，报告失败
                console.log('[XFOLLOW] Follow failed after ' + maxRetries + ' retries');
                console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
            }
        }
    }

    // 等待页面完全加载后再执行
    function waitForPageReady() {
        let checkCount = 0;
        const maxChecks = 30;  // 最多等待15秒

        const interval = setInterval(() => {
            checkCount++;

            // 检查页面是否加载完成
            const isDocumentReady = document.readyState === 'complete';

            // 检查是否是自己的页面
            const isOwnProfile = checkIfOwnProfile();
            if (isOwnProfile && isDocumentReady) {
                clearInterval(interval);
                console.log('[XFOLLOW] This is own profile, skipping...');
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
                return;
            }

            // 检查是否有用户头像（表示用户信息已加载）
            const hasAvatar = document.querySelector('[data-testid="UserAvatar-Container-unknown"]') ||
                              document.querySelector('a[href="/' + userHandle + '/photo"]') ||
                              document.querySelector('[data-testid="UserName"]');

            // 检查是否有关注按钮
            const hasFollowButton = findFollowButton() !== null || checkIfFollowing();

            // 检查是否有用户简介区域
            const hasUserProfile = document.querySelector('[data-testid="UserDescription"]') ||
                                   document.querySelector('[data-testid="UserProfileHeader_Items"]');

            console.log('[XFOLLOW] Waiting for page... check ' + checkCount + '/' + maxChecks +
                        ' ready=' + isDocumentReady +
                        ' avatar=' + !!hasAvatar +
                        ' followBtn=' + hasFollowButton +
                        ' profile=' + !!hasUserProfile);

            // 页面完全加载的条件：文档ready + (有头像或简介) + 有关注按钮
            const isPageReady = isDocumentReady && (hasAvatar || hasUserProfile) && hasFollowButton;

            if (isPageReady) {
                clearInterval(interval);
                console.log('[XFOLLOW] Page ready, waiting 2 seconds before clicking...');
                // 页面准备好后再等2秒确保稳定
                setTimeout(findAndClickFollow, 2000);
            } else if (checkCount >= maxChecks) {
                clearInterval(interval);
                console.log('[XFOLLOW] Page load timeout, trying anyway...');
                setTimeout(findAndClickFollow, 1500);
            }
        }, 500);
    }

    // 开始等待页面加载
    console.log('[XFOLLOW] Auto-follow script injected, waiting for page to load...');
    waitForPageReady();
})();
)";

    return script;
}

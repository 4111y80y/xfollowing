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

    function checkIfAccountSuspended() {
        // 检查账号是否被封禁或受限
        const bodyText = document.body.innerText.toLowerCase();

        // 检查各种限制状态（使用精确的完整短语）
        const restrictionKeywords = [
            'account suspended',
            'this account has been suspended',
            'caution: this account is temporarily restricted',
            'this account is temporarily restricted',
            'account has been withheld',
            'this account is suspended',
            'account withheld in'
        ];

        for (const keyword of restrictionKeywords) {
            if (bodyText.includes(keyword)) {
                console.log('[XFOLLOW] Detected restriction: ' + keyword);
                return true;
            }
        }

        // 专门检测"账号不存在"页面（更精确的检测）
        // 这个页面通常同时包含这两个短语
        if (bodyText.includes('this account doesn') && bodyText.includes('exist')) {
            console.log('[XFOLLOW] Detected: account does not exist');
            return true;
        }
        if (bodyText.includes('try searching for another')) {
            console.log('[XFOLLOW] Detected: try searching for another');
            return true;
        }

        return false;
    }

    function checkIfUserFollowsMe() {
        // 检查对方是否已经关注我（显示 "Follows you"）
        const spans = document.querySelectorAll('span');
        for (const span of spans) {
            const text = span.innerText.toLowerCase();
            if (text === 'follows you' || text === '正在关注你' || text === '关注了你') {
                return true;
            }
        }
        return false;
    }

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
                // 兼容 "Follow" 和 "Follow back" 按钮
                if (btnText === 'follow' || btnText === '关注' || btnText === 'follow back' || btnText === '回关') {
                    return btn;
                }
            }
        }
        return null;
    }

    function findAndClickFollow() {
        // 先检查账号是否被封禁
        if (checkIfAccountSuspended()) {
            console.log('[XFOLLOW] Account is suspended, skipping...');
            console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
            return;
        }

        // 检查是否是自己的页面
        if (checkIfOwnProfile()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }

        // 检查是否已经是Following状态
        if (checkIfFollowing()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }

        // 检查对方是否已经关注我（先关注我的用户，直接关注回去）
        if (checkIfUserFollowsMe()) {
            console.log('[XFOLLOW] User already follows me, will follow back');
            // 继续执行关注操作，不跳过
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

            // 先检查账号是否被封禁或不存在（尽早检测）
            if (isDocumentReady && checkIfAccountSuspended()) {
                clearInterval(interval);
                console.log('[XFOLLOW] Account suspended/not exist, skipping...');
                console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
                return;
            }

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
                // 超时时再次检查账号状态
                if (checkIfAccountSuspended()) {
                    console.log('[XFOLLOW] Account suspended/not exist (timeout), skipping...');
                    console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
                } else {
                    console.log('[XFOLLOW] Page load timeout, trying anyway...');
                    setTimeout(findAndClickFollow, 1500);
                }
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

QString AutoFollower::getCheckFollowBackScript() {
    QString script = R"(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';
    let checkCount = 0;
    const maxChecks = 20;  // 最多等待10秒

    function checkIfAccountSuspended() {
        const bodyText = document.body.innerText.toLowerCase();
        if (bodyText.includes('account suspended') || bodyText.includes('this account has been suspended')) {
            return true;
        }
        return false;
    }

    function checkIfFollowsMe() {
        // 检查是否有"Follows you"标签
        const spans = document.querySelectorAll('span');
        for (const span of spans) {
            const text = span.innerText.toLowerCase();
            if (text === 'follows you' || text === '正在关注你' || text === '关注了你') {
                return true;
            }
        }
        return false;
    }

    function checkIfIAmFollowing() {
        // 检查我是否正在关注对方（有Following/正在关注按钮）
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId && testId.includes('-unfollow')) {
                return true;
            }
            if (btnText === 'following' || btnText === '正在关注') {
                return true;
            }
        }
        return false;
    }

    function checkPage() {
        checkCount++;

        // 检查账号是否被封禁
        if (checkIfAccountSuspended()) {
            console.log('[XFOLLOW] Account is suspended');
            console.log('XFOLLOWING_CHECK_SUSPENDED:' + userHandle);
            return;
        }

        // 检查页面是否加载完成
        const hasUserProfile = document.querySelector('[data-testid="UserDescription"]') ||
                               document.querySelector('[data-testid="UserProfileHeader_Items"]') ||
                               document.querySelector('[data-testid="UserName"]');

        if (!hasUserProfile && checkCount < maxChecks) {
            setTimeout(checkPage, 500);
            return;
        }

        // 检查我是否关注了对方
        if (!checkIfIAmFollowing()) {
            console.log('[XFOLLOW] Not following this user, skip check');
            console.log('XFOLLOWING_CHECK_NOT_FOLLOWING:' + userHandle);
            return;
        }

        // 检查对方是否回关我
        if (checkIfFollowsMe()) {
            console.log('[XFOLLOW] User follows me back');
            console.log('XFOLLOWING_CHECK_FOLLOWS_BACK:' + userHandle);
        } else {
            console.log('[XFOLLOW] User does NOT follow me back');
            console.log('XFOLLOWING_CHECK_NOT_FOLLOW_BACK:' + userHandle);
        }
    }

    // 等待页面加载后检查
    console.log('[XFOLLOW] Check follow-back script injected');
    setTimeout(checkPage, 2000);
})();
)";

    return script;
}

QString AutoFollower::getUnfollowScript() {
    QString script = R"(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';

    function findUnfollowButton() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId && testId.includes('-unfollow')) {
                return btn;
            }
            if (btnText === 'following' || btnText === '正在关注') {
                return btn;
            }
        }
        return null;
    }

    function findConfirmUnfollowButton() {
        // 查找确认取消关注的按钮
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId === 'confirmationSheetConfirm') {
                return btn;
            }
            if (btnText === 'unfollow' || btnText === '取消关注') {
                return btn;
            }
        }
        return null;
    }

    function verifyUnfollowSuccess() {
        // 检查是否变成了Follow状态
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId && testId.includes('-follow') && !testId.includes('-unfollow')) {
                if (btnText === 'follow' || btnText === '关注') {
                    return true;
                }
            }
        }
        return false;
    }

    function doUnfollow() {
        const btn = findUnfollowButton();
        if (btn) {
            console.log('[XFOLLOW] Found Following button, clicking to unfollow...');
            btn.click();

            // 等待确认对话框出现
            setTimeout(() => {
                const confirmBtn = findConfirmUnfollowButton();
                if (confirmBtn) {
                    console.log('[XFOLLOW] Found confirm button, clicking...');
                    confirmBtn.click();

                    // 验证取消关注是否成功
                    setTimeout(() => {
                        if (verifyUnfollowSuccess()) {
                            console.log('[XFOLLOW] Unfollow success');
                            console.log('XFOLLOWING_UNFOLLOW_SUCCESS:' + userHandle);
                        } else {
                            console.log('[XFOLLOW] Unfollow may have failed');
                            console.log('XFOLLOWING_UNFOLLOW_FAILED:' + userHandle);
                        }
                    }, 2000);
                } else {
                    console.log('[XFOLLOW] Confirm button not found');
                    console.log('XFOLLOWING_UNFOLLOW_FAILED:' + userHandle);
                }
            }, 1000);
        } else {
            console.log('[XFOLLOW] Following button not found');
            console.log('XFOLLOWING_UNFOLLOW_FAILED:' + userHandle);
        }
    }

    // 等待页面加载后执行取消关注
    console.log('[XFOLLOW] Unfollow script injected');
    setTimeout(doUnfollow, 2000);
})();
)";

    return script;
}

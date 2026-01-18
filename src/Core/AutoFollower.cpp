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

    function findAndClickFollow() {
        // 先检查是否已经是Following状态
        if (checkIfFollowing()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }

        // 查找关注按钮
        const buttons = document.querySelectorAll('[role="button"]');

        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');

            // 检查是否是关注按钮（未关注状态）
            if (testId && testId.includes('-follow') && !testId.includes('-unfollow')) {
                const btnText = btn.innerText.toLowerCase();

                // 确认是"关注"按钮
                if (btnText === 'follow' || btnText === '关注') {
                    console.log('[XFOLLOW] Found follow button, clicking... (attempt ' + (retryCount + 1) + ')');
                    btn.click();

                    // 等待后检查是否成功
                    setTimeout(verifyFollowSuccess, 1500);
                    return;
                }
            }
        }

        // 没找到按钮
        console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
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
                setTimeout(findAndClickFollow, 1000);
            } else {
                // 重试次数用完，报告失败
                console.log('[XFOLLOW] Follow failed after ' + maxRetries + ' retries');
                console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
            }
        }
    }

    // 等待页面加载完成后执行
    function waitAndFollow() {
        let attempts = 0;
        const maxAttempts = 10;

        const interval = setInterval(() => {
            attempts++;

            // 查找关注按钮
            const buttons = document.querySelectorAll('[role="button"]');
            let found = false;

            for (const btn of buttons) {
                const testId = btn.getAttribute('data-testid');
                const btnText = btn.innerText.toLowerCase();

                if ((testId && testId.includes('follow')) ||
                    btnText === 'follow' || btnText === '关注' ||
                    btnText === 'following' || btnText === '正在关注') {
                    found = true;
                    break;
                }
            }

            if (found || attempts >= maxAttempts) {
                clearInterval(interval);
                setTimeout(findAndClickFollow, 500);
            }
        }, 500);
    }

    // 延迟执行，等待页面渲染
    setTimeout(waitAndFollow, 1500);

    console.log('[XFOLLOW] Auto-follow script injected (with retry)');
})();
)";

    return script;
}

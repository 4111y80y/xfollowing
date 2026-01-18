#include "AutoFollower.h"

AutoFollower::AutoFollower(QObject* parent)
    : QObject(parent) {
}

QString AutoFollower::getFollowScript() {
    QString script = R"(
(function() {
    function findAndClickFollow() {
        // 获取当前用户handle
        const pathParts = window.location.pathname.split('/');
        const userHandle = pathParts[1] || '';

        // 查找关注按钮
        // X.com的关注按钮通常有data-testid属性
        const buttons = document.querySelectorAll('[role="button"]');

        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');

            // 检查是否是关注按钮（未关注状态）
            if (testId && testId.includes('-follow') && !testId.includes('-unfollow')) {
                const btnText = btn.innerText.toLowerCase();

                // 确认是"关注"按钮
                if (btnText === 'follow' || btnText === '关注') {
                    console.log('[XFOLLOW] Found follow button, clicking...');
                    btn.click();

                    // 等待一下再确认
                    setTimeout(() => {
                        console.log('XFOLLOWING_FOLLOW_SUCCESS:' + userHandle);
                    }, 500);
                    return true;
                }
            }

            // 检查是否已经关注
            if (testId && testId.includes('-unfollow')) {
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
                return false;
            }
        }

        // 也可能按钮文本直接包含"Following"
        for (const btn of buttons) {
            const btnText = btn.innerText.toLowerCase();
            if (btnText === 'following' || btnText === '正在关注') {
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
                return false;
            }
        }

        console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
        return false;
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

    console.log('[XFOLLOW] Auto-follow script injected');
})();
)";

    return script;
}

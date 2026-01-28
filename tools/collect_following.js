// 采集关注列表脚本 v2
// ================================
// 步骤1: 打开 https://x.com/4111y80y/following 运行此脚本
// 步骤2: 滚动到底后输入 saveFollowing()
// 步骤3: 打开 https://x.com/4111y80y/verified_followers 再次运行此脚本
// 步骤4: 滚动到底后输入 saveFollowers()
// 步骤5: 输入 exportFinal() 导出最终数据

(function() {
    // 初始化存储
    if (!window.xfollowData) {
        window.xfollowData = {
            following: new Map(),      // 我关注的人
            followers: new Map(),      // 关注我的人
            currentScan: new Map()     // 当前页面扫描
        };
    }

    function parseUserCell(cell) {
        try {
            const links = cell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            let userName = '';

            for (const link of links) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    const nameSpan = link.querySelector('span');
                    userName = nameSpan ? nameSpan.innerText : userHandle;
                    break;
                }
            }

            if (!userHandle) return null;

            // 检查是否是蓝V
            const verified = cell.querySelector('[data-testid="icon-verified"]') ||
                           cell.querySelector('svg[aria-label="Verified account"]');

            return {
                authorHandle: userHandle,
                authorName: userName,
                isVerified: !!verified
            };
        } catch (e) {
            return null;
        }
    }

    function scan() {
        const cells = document.querySelectorAll('[data-testid="UserCell"]');
        let newCount = 0;

        cells.forEach(cell => {
            const user = parseUserCell(cell);
            if (user && !window.xfollowData.currentScan.has(user.authorHandle)) {
                window.xfollowData.currentScan.set(user.authorHandle, user);
                newCount++;
            }
        });

        if (newCount > 0) {
            console.log('[采集] 新增 ' + newCount + ' 个，当前页总计: ' + window.xfollowData.currentScan.size);
        }
    }

    // 定时扫描
    if (window.collectInterval) clearInterval(window.collectInterval);
    window.collectInterval = setInterval(scan, 2000);

    // 保存为"我关注的人"（合并，不覆盖）
    window.saveFollowing = function() {
        let addCount = 0;
        window.xfollowData.currentScan.forEach((user, handle) => {
            if (!window.xfollowData.following.has(handle)) {
                window.xfollowData.following.set(handle, user);
                addCount++;
            }
        });
        console.log('[SUCCESS] 新增 ' + addCount + ' 个，following 总计: ' + window.xfollowData.following.size);
        window.xfollowData.currentScan.clear();
        return window.xfollowData.following.size;
    };

    // 保存为"关注我的人"（合并，不覆盖）
    window.saveFollowers = function() {
        let addCount = 0;
        window.xfollowData.currentScan.forEach((user, handle) => {
            if (!window.xfollowData.followers.has(handle)) {
                window.xfollowData.followers.set(handle, user);
                addCount++;
            }
        });
        console.log('[SUCCESS] 新增 ' + addCount + ' 个，followers 总计: ' + window.xfollowData.followers.size);
        window.xfollowData.currentScan.clear();
        return window.xfollowData.followers.size;
    };

    // 导出最终数据
    window.exportFinal = function() {
        const following = window.xfollowData.following;
        const followers = window.xfollowData.followers;
        const now = new Date().toISOString().replace('T', ' ').substring(0, 19);

        console.log('===== 数据统计 =====');
        console.log('我关注的人: ' + following.size);
        console.log('关注我的蓝V: ' + followers.size);

        // 计算互关
        let mutualCount = 0;
        following.forEach((user, handle) => {
            if (followers.has(handle)) {
                mutualCount++;
            }
        });
        console.log('互关用户: ' + mutualCount);

        // 生成 posts.json 格式
        const posts = [];

        following.forEach((user, handle) => {
            const isMutual = followers.has(handle);  // 是否互关

            posts.push({
                postId: 'recovered_' + handle,
                authorHandle: handle,
                authorName: user.authorName,
                authorUrl: 'https://x.com/' + handle,
                content: isMutual ? '[互关恢复]' : '[已关注恢复]',
                postUrl: '',
                postTime: '',
                matchedKeyword: isMutual ? '互关' : '已关注恢复',
                collectTime: now,
                followTime: now,
                isFollowed: true,
                isHidden: false,
                // 关键：互关用户设置 lastCheckedTime，避免被取关检查
                lastCheckedTime: isMutual ? now : ''
            });
        });

        console.log('');
        console.log('生成记录: ' + posts.length + ' 条');
        console.log('其中互关(不会被取关): ' + mutualCount + ' 条');

        // 下载文件
        const blob = new Blob([JSON.stringify(posts, null, 2)], {type: 'application/json'});
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'following_recovery.json';
        a.click();

        console.log('[SUCCESS] 文件已下载: following_recovery.json');
        return posts;
    };

    // 停止
    window.stopCollect = function() {
        if (window.collectInterval) {
            clearInterval(window.collectInterval);
            console.log('[STOP] 当前页采集: ' + window.xfollowData.currentScan.size);
        }
    };

    // 查看状态
    window.showStatus = function() {
        console.log('===== 当前状态 =====');
        console.log('following 已保存: ' + window.xfollowData.following.size);
        console.log('followers 已保存: ' + window.xfollowData.followers.size);
        console.log('当前页采集: ' + window.xfollowData.currentScan.size);
    };

    console.log('');
    console.log('========================================');
    console.log('  关注数据采集脚本 v2');
    console.log('========================================');
    console.log('');
    console.log('使用方法:');
    console.log('1. 在 /following 页面滚动到底，输入 saveFollowing()');
    console.log('2. 在 /verified_followers 页面滚动到底，输入 saveFollowers()');
    console.log('3. 输入 exportFinal() 导出数据');
    console.log('');
    console.log('其他命令: showStatus() stopCollect()');
    console.log('');
    showStatus();

    // 初始扫描
    scan();
})();

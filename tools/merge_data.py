# -*- coding: utf-8 -*-
"""
合并采集的关注数据到 posts.json
使用方法：
python merge_data.py following_backup.json
"""

import json
import sys
import os
import codecs

# 强制 UTF-8 输出
sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer)

def merge_data(backup_file):
    # 数据文件路径
    data_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'build', 'release', 'data')
    posts_file = os.path.join(data_dir, 'posts.json')

    # 读取现有数据
    existing_posts = []
    if os.path.exists(posts_file):
        with open(posts_file, 'r', encoding='utf-8') as f:
            existing_posts = json.load(f)
        print(f'[INFO] 现有数据: {len(existing_posts)} 条记录')

    # 读取备份数据
    with open(backup_file, 'r', encoding='utf-8') as f:
        backup_posts = json.load(f)
    print(f'[INFO] 备份数据: {len(backup_posts)} 条记录')

    # 建立现有用户索引
    existing_handles = {p['authorHandle'] for p in existing_posts}

    # 合并数据
    new_count = 0
    for post in backup_posts:
        handle = post['authorHandle']
        if handle not in existing_handles:
            existing_posts.append(post)
            existing_handles.add(handle)
            new_count += 1

    print(f'[INFO] 新增: {new_count} 条记录')
    print(f'[INFO] 合并后总计: {len(existing_posts)} 条记录')

    # 统计已关注数量
    followed_count = sum(1 for p in existing_posts if p.get('isFollowed', False))
    print(f'[INFO] 已关注用户: {followed_count} 个')

    # 保存
    with open(posts_file, 'w', encoding='utf-8') as f:
        json.dump(existing_posts, f, ensure_ascii=False, indent=4)

    print(f'[SUCCESS] 数据已保存到: {posts_file}')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('使用方法: python merge_data.py following_backup.json')
        sys.exit(1)

    merge_data(sys.argv[1])

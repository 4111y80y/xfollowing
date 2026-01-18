# X互关宝 (xfollowing)

X互关宝是一个帮助用户在X.com (Twitter)上获取互关粉丝的开源工具。

## 功能特点

- **帖子监控**: 自动监控X.com上含有"互关"、"followback"等关键词的帖子
- **自动关注**: 点击帖子时自动打开用户主页并执行关注操作
- **状态追踪**: 标记已关注的帖子，支持隐藏已关注
- **三栏布局**: 左侧搜索页、中间控制面板、右侧用户页
- **数据持久化**: 自动保存帖子和关键词配置

## 技术栈

- C++ 17
- Qt 6.10.1
- CEF (Chromium Embedded Framework)
- CMake

## 编译要求

- Windows 10/11
- Visual Studio 2022 (MSVC)
- Qt 6.10.1 MSVC 2022 64bit
- CEF库

## 编译步骤

1. 确保Qt和CEF路径正确配置在CMakeLists.txt中
2. 使用Qt Creator打开CMakeLists.txt
3. 选择MSVC 2022 64bit编译套件
4. 构建项目

```bash
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## 使用方法

1. 启动程序后，左侧浏览器会打开X.com搜索页
2. 登录你的X.com账号
3. 在中间面板设置监控关键词（默认包含"互关"）
4. 程序会自动监控匹配的帖子并显示在列表中
5. 点击帖子，右侧浏览器会打开该用户的主页并自动关注
6. 已关注的帖子会标记[v]，可以选择隐藏

## 项目结构

```
xfollowing/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── App/           # CEF集成
│   ├── UI/            # 用户界面
│   ├── Data/          # 数据结构和存储
│   ├── Core/          # 核心功能
│   └── Utils/         # 工具类
├── data/              # 数据存储目录
└── userdata/          # 浏览器配置目录
```

## 开源协议

MIT License

## 仓库地址

https://github.com/4111y80y/xfollowing.git

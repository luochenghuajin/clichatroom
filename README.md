# 命令行聊天室

项目来源于南京大学操作系统与Linux程序设计课程，简要来说是要构建一个基于命令行的聊天室项目，实现多客户端实时聊天。实现语言为C++。

项目同时配备了完整的单元测试，基于 Google Test / Google Mock 框架。

## 环境依赖

| 依赖项                              | 版本 / 要求          | 说明         |
| ----------------------------------- | -------------------- | ------------ |
| **CMake**                     | >= 3.15              | 构建系统     |
| **C++ 编译器**                | 支持 C++17           | 编译源代码   |
| **pthread**                   | Linux / WSL 默认自带 | 多线程支持   |
| **Google Test / Google Mock** | 任意较新版本         | 单元测试框架 |

如果系统中尚未安装 Google Test，可通过包管理器（如 apt ）手动安装。

## 运行项目

本项目包含 **聊天服务器** 与 **客户端** 两个可执行程序。在 `CLIChatRoom/`目录下运行下面的命令。

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

构建完成后，所有可执行文件位于 `build/` 目录下。

在 `build/`目录下运行下面的命令即可运行服务器。

```bash
./chat_server
```

在新的终端窗口中执行：

```bash
./chat_client
```

启动后，客户端会尝试连接到服务器，根据提示输入用户名即可，如果已被占用，则会要求重新输入，三次失败后自动退出。

进入聊天室后直接输入信息并发送是公聊，@用户名 消息内容 则是私聊，若用户名不存在，服务器会提示用户不存在。

输入/list 命令展示当前聊天室内客户端列表，输入/bye 命令退出客户端。

## 项目结构说明

CLIChatRoom/
├── client.cpp
├── CMakeLists.txt
├── common.h
├── console.h
├── file_io.cpp
├── file_io.h
├── network.cpp
├── network.h
├── README.md
├── server.cpp
├── services.cpp
└── services.h

## 测试说明

本项目使用 **GoogleTest + GoogleMock** 进行单元测试。

在 `build` 目录下执行：

```bash
ctest
```

所有测试均通过，项目运行正常。

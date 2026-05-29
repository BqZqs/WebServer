<h1 align="center">🚀 High-Performance C++ WebServer</h1>

<p align="center">
  <strong>基于 C++11 编写的轻量级、高性能并发 Web 服务器</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C++11-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/OS-Linux-orange.svg" alt="OS">
  <img src="https://img.shields.io/badge/Database-MySQL-lightgrey.svg" alt="Database">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License">
</p>

---

## 📖 项目简介

本项目是一个基于 Linux 环境下 C++11 编写的工业级轻量 Web 服务器。采用 **单 Reactor 模式** 与 **半同步/半异步** 的并发架构，底层基于 `Epoll` 多路复用与自建线程池。项目实现了严谨的 HTTP 协议状态机解析、小顶堆定时器连接管理、单例 MySQL 连接池以及支持自动滚动的异步日志引擎。不仅能够轻松应对万级并发，同时也具备极高的扩展性。

## ✨ 核心技术栈与架构亮点

* ⚡ **高并发网络引擎**：基于 `Epoll` 的 **ET + EPOLLONESHOT** 模式，配合非阻塞 Socket 与自建 **ThreadPool**，实现零阻塞的事件驱动分发。
* 🌐 **HTTP 解析**：基于 **FSM** 解析 HTTP 请求，支持 HTTP/1.1 Keep-Alive 以及`SO_LINGER`。
* 💾 **I/O 优化**：使用 `mmap` 内存映射技术与 `writev` 进行零拷贝传输，配合底层自适应动态扩容 `Buffer`，流畅处理大文件分块下发。
* ⏱️ **连接管理**：底层基于 **小顶堆** 数据结构设计的定时器，以 O(logN) 复杂度清理超时非活跃连接，保护服务器资源。
* 🐬 **数据库连接池**：采用单例模式与 RAII 封装 **MySQL 连接池**，消除高并发下频繁建立 TCP 握手的开销，保障 SQL 交互安全稳定。
* 📝 **异步日志系统**：基于生产者-消费者模型构建异步日志引擎。前端业务线程快速产生日志消息，后端刷盘线程按天/按行自动滚动落盘，解决了高并发下的磁盘 I/O 阻塞痛点。

---

## 🛠️ 环境依赖

* **操作系统**: Linux (推荐 Ubuntu 20.04+ / CentOS 7+)
* **编译器**: G++ 4.8 及以上版本 (需完整支持 C++11 核心特性)
* **数据库**: MySQL 5.7+ (需安装 `libmysqlclient-dev` 开发包)
* **构建工具**: Make

```bash
# Ubuntu 环境下一键安装依赖
sudo apt-get update
sudo apt-get install build-essential mysql-server libmysqlclient-dev

```

---

## 🚀 快速启动

### 1. 数据库初始化

登录本地 MySQL 并执行以下 SQL 语句，创建项目专属数据库及测试表：

```sql
CREATE DATABASE webserver;
USE webserver;

CREATE TABLE user (
    username char(50) NULL,
    password char(50) NULL
) ENGINE=InnoDB;

INSERT INTO user(username, password) VALUES('root', '123456');

```

### 2. 修改配置文件

在项目根目录打开或创建 `./conf/server.conf`（如无则在 `main.cpp` 中修改默认参数），填入你的数据库密码及所需端口：

```ini
port = 8080
trig_mode = 1          # 1: Listen LT + Conn ET (稳定模式)
timeout_ms = 60000     # Keep-Alive 连接空闲超时时间 (ms)
sql_user = root
sql_pwd = 你的数据库密码
db_name = webserver
thread_num = 8         # 线程池 Worker 数量

```

### 3. 编译与运行

在项目根目录打开终端，执行以下命令：

```bash
# 编译项目 (生成可执行文件到 ./bin 目录)
make

# 启动服务器
./bin/server

```

启动成功后，在浏览器中访问：`http://127.0.0.1:8080` 即可看到欢迎页面。

---

## 📂 核心目录结构

```text
.
WebServer/
├── bin/                    # 编译生成的可执行文件存放地
├── build/                  # CMake 构建时的临时目录
├── conf/                   # 配置文件目录
│   └── server.conf         # 服务器核心运行参数配置
├── log/                    # 运行时自动生成的日志存放地
├── resources/              # 静态资源区
│   ├── index.html
│   ├── 404.html
│   └── favicon.ico
├── CMakeLists.txt          # 构建脚本
├── README.md
├── include/                # 头文件区 (.h)
│   ├── base/               # 【基础组件模块】
│   │   ├── block_queue.h
│   │   ├── buffer.h
│   │   ├── config.h
│   │   ├── log.h
│   │   ├── sql_conn_pool.h
│   │   └── thread_pool.h
│   ├── http/               # 【HTTP 业务模块】
│   │   ├── http_conn.h
│   │   ├── http_request.h
│   │   └── http_response.h
│   ├── net/                # 【网络核心模块】
│   │   ├── epoll_poller.h
│   │   ├── timer_manager.h 
│   │   └── web_server.h
└── src/                    # 源文件区 (.cpp)
    ├── base/
    │   ├── buffer.cpp
    │   ├── config.cpp
    │   ├── log.cpp
    │   └── sql_conn_pool.cpp
    ├── http/
    │   ├── http_conn.cpp
    │   ├── http_request.cpp
    │   └── http_response.cpp
    ├── net/
    │   ├── epoll_poller.cpp
    │   ├── timer_manager.cpp
    │   └── web_server.cpp
    └── main.cpp            # 唯一的程序入口

```

---

## 📈 压力测试 (Benchmark)

使用标准压测工具 **Webbench** 对本机部署的 WebServer 进行极限并发压测：

* **测试环境**: Ubuntu 20.04 / 4核 8G / 虚拟机环境
* **压测命令**: `./webbench -c 10000 -t 60 http://127.0.0.1:8080/`
* **极限表现**:
* 并发连接数：`10,000`
* QPS (Requests/sec)：`> 15,000`
* 错误率：`0%` (在系统 FD 限制配置合理的情况下，无拒绝连接)



---

## 🗺️ 未来演进路线 (Roadmap)

作为面向未来的底层架构，本项目计划向 AI Infra 与微服务网关方向进一步扩展：

* [ ] **安全升级**：接入 OpenSSL，完成 TLS/SSL 握手支持，实现全站 HTTPS 加密传输。
* [ ] **中间件集成**：引入 **Redis** 作为热点资源缓存层，进一步降低大并发下的 MySQL I/O 穿透开销。
* [ ] **AI 原生支持**：深度改造 HTTP 协议栈，增加对 **SSE (Server-Sent Events)** 的原生支持，完美适配 LLM 大语言模型的流式（打字机）响应。
* [ ] **轻量级 API 网关**：封装 HTTP Client 组件，使其具备向外请求调度大模型 API 或向量数据库的能力，赋能 C++ RAG 应用落地。

---


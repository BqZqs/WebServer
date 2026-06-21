# WebServer

一个 Linux 下的 C++ HTTP 服务器，参考 muduo 网络库的设计思路独立实现。学习项目，非生产可用。

## 技术栈

C++14 · Epoll · 线程池 · MySQL · mmap

## 实现了什么

- `epoll` 事件分发（支持 ET/LT 切换，默认 `EPOLLONESHOT` 保并发安全）
- 固定大小的线程池（mutex + condition_variable，生产者-消费者模型）
- HTTP/1.1 请求解析（有限状态机：REQUEST_LINE → HEADERS → BODY → FINISH）
- `mmap` 文件映射 + `writev` 聚集写用于静态文件响应
- 最小堆定时器（连接超时断开，O(log n) 插入/删除）
- MySQL 连接池（单例 + RAII 借用器，`GetConn()` 阻塞等待）
- 异步日志（生产者-消费者阻塞队列，后台线程刷盘，按天/按行自动切割）
- TOML 风味的配置解析器（键值对，支持注释和默认值）

## 已知问题

### Bug

1. **`SetFdNonblock_` 用错了 flag**（`src/net/web_server.cpp`）：  
   `fcntl(fd, F_GETFD, 0)` 应改为 `fcntl(fd, F_GETFL, 0)`。当前碰巧能跑是因为 `F_GETFD` 默认返回 0，`0 | O_NONBLOCK` = `O_NONBLOCK`。但如果 fd 已有其他状态 flag（如 `O_APPEND`），会被错误覆盖。

2. **ET 模式下存在事件丢失风险**（`src/net/web_server.cpp`）：  
   当 `conn_event_` 包含 `EPOLLET` 时，`OnProcess_` 重新 `ModFd(EPOLLIN)` 之后，如果客户端的所有数据在窗口期内已经全部到达内核缓冲区，ET 模式不会再次触发事件，导致该连接被永远挂起。

3. **HTTP 解析器对不完整的首行会直接返回 400**（`src/http/http_request.cpp`）：  
   `Parse()` 里在找到 `\r\n` 之前就把不完整的行传给了 `ParseRequestLine_`。如果 TCP 拆包恰好把 `GET /index.html HTTP/1.1\r\n` 切在中间，服务器会返回 Bad Request 而不是等待后续数据。

4. **定时器无法保证连接在 worker 处理完毕前不被关闭**：  
   如果一次 HTTP 请求的处理时间超过了 `timeout_ms`，定时器回调 `CloseConn_` 会 `close(fd_)`，而 worker 线程可能仍持有该 `HttpConn*` 指针继续读写，存在 use-after-close 风险。

### 设计缺陷

5. **Lambda 捕获裸 `this` 指针抛给线程池**（`src/net/web_server.cpp`）：  
   `AddTask([this, client] { ... })` 中的 `this`（`WebServer*`）生命周期无保证。如果线程池任务队列中还有未执行的任务时 `WebServer` 被销毁，worker 线程会访问野指针。当前靠析构顺序（线程池先 join 再退）侥幸安全。

6. **日志系统 `Write()` 同时操作 Buffer 和文件指针**：  
   异步写路径里，`buff_` 格式化的内容和 `fflush(fp_)` 的调用在不同的锁上下文里——队列满时回退到同步写，与异步刷盘线程可能产生交错输出。

7. **README 原版声称的 "零拷贝" 并不准确**：  
   `mmap` + `writev` 减少了用户态缓冲区拷贝，但内核仍然需要把数据从页缓存拷到 socket 缓冲区。真正的零拷贝需要 `sendfile()` + `MSG_ZEROCOPY`。原 README 的说法存在夸大。

## 构建

```bash
# 依赖
sudo apt install build-essential libmysqlclient-dev cmake

# 编译
cmake -B build
cmake --build build

# 运行（先创建数据库和表，见 conf/server.conf）
./bin/WebServer
```

## 参考

- [muduo 网络库](https://github.com/chenshuo/muduo) — Reactor 架构和 Buffer 设计的主要参考
- 部分模块（定时器、日志队列）在理解设计思路后独立重写

## 许可

MIT

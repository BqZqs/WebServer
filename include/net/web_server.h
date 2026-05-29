#ifndef WEBSERVER_NET_WEB_SERVER_H_
#define WEBSERVER_NET_WEB_SERVER_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <memory>
#include <unordered_map>

#include "base/sql_conn_pool.h"
#include "base/thread_pool.h"
#include "http/http_conn.h"
#include "net/epoll_poller.h"
#include "net/timer_manager.h"

namespace web_server {
namespace net {

// Reactor
class WebServer {
 public:
  // 构造函数：解析来自 config 的参数，组装所有组件
  WebServer(int port, int trig_mode, int timeout_ms, bool opt_linger,
            int sql_port, const char* sql_user, const char* sql_pwd,
            const char* db_name, int conn_pool_num, int thread_num,
            bool open_log, int log_level, int log_que_size);

  ~WebServer();
  
  // 启动服务器主事件循环
  void Start();

 private:
  // 初始化系统网络监听 Socket
  bool InitSocket_();
  
  // 初始化连接模式 (ET 还是 LT)
  void InitEventMode_(int trig_mode);
  
  // 将新建连接存入连接池并注册 Epoll 和 Timer
  void AddClient_(int fd, sockaddr_in addr);
  
  // 关闭指定的客户端连接
  void CloseConn_(http::HttpConn* client);

  // 主线程 Reactor 的事件分发函数
  void DealListen_();
  void DealRead_(http::HttpConn* client);
  void DealWrite_(http::HttpConn* client);

  // 抛给 ThreadPool 异步执行的业务回调函数
  void OnRead_(http::HttpConn* client);
  void OnWrite_(http::HttpConn* client);
  void OnProcess_(http::HttpConn* client);

  // 设置 Socket 为非阻塞模式
  static int SetFdNonblock_(int fd);

  // 声明发送错误信息的方法
  void SendError_(int fd, const char* info);

  static const int kMaxFd = 65536; // 支持的最大文件描述符数量

  int port_;          // 监听端口
  bool open_linger_;  // TCP SO_LINGER 延迟关闭选项
  int timeout_ms_;    // 定时器超时时间 (毫秒)
  bool is_close_;     // 服务器是否已关闭
  int listen_fd_;     // 主监听 Socket
  char src_dir_[256]; // 静态资源根目录

  uint32_t listen_event_; // 监听 Socket 的 Epoll 事件标志
  uint32_t conn_event_;   // 客户端 Socket 的 Epoll 事件标志

  // 核心组件，采用智能指针进行生命周期管理
  std::unique_ptr<TimerManager> timer_;
  std::unique_ptr<base::ThreadPool> thread_pool_;
  std::unique_ptr<EpollPoller> epoll_;
  
  // HTTP 连接的资源池。利用哈希表将 FD 映射到对应的 HttpConn 实例
  std::unordered_map<int, http::HttpConn> users_;
};

}  // namespace net
}  // namespace web_server

#endif  // WEBSERVER_NET_WEB_SERVER_H_

#ifndef WEBSERVER_HTTP_HTTP_CONN_H_
#define WEBSERVER_HTTP_HTTP_CONN_H_

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>    // readv, writev
#include <atomic>
#include <string>

#include "base/buffer.h"
#include "base/log.h"
#include "base/sql_conn_pool.h"
#include "http/http_request.h"
#include "http/http_response.h"

namespace web_server {
namespace http {

// HTTP 连接抽象类
// 贯穿单个客户端连接的全生命周期
class HttpConn {
 public:
  HttpConn();
  ~HttpConn();

  // 初始化一个新接入的客户端连接
  void Init(int sock_fd, const sockaddr_in& addr);
  
  // 安全关闭连接，释放 FD 和 mmap 资源
  void Close();

  // 网络 I/O 读写接口（供线程池 Worker 调用）
  ssize_t Read(int* save_errno);
  ssize_t Write(int* save_errno);

  // 核心业务处理接口：解析读到的数据并生成响应数据
  bool Process();

  // 状态查询
  int GetFd() const;
  int GetPort() const;
  const char* GetIP() const;
  sockaddr_in GetAddr() const;
  bool IsKeepAlive() const;

  // 还有多少字节的数据等待发送
  int ToWriteBytes() const { 
    return iov_[0].iov_len + iov_[1].iov_len; 
  }

  // 静态成员：所有 HttpConn 共享的全局配置
  static bool is_et;         // 是否使用 Epoll ET (边缘触发) 模式
  static const char* src_dir;// 网站根目录路径
  static std::atomic<int> user_count; // 全局原子计数器，统计当前并发连接数

 private:
  int fd_;                   // 客户端 Socket FD
  struct sockaddr_in addr_;  // 客户端 IP 和端口信息
  bool is_close_;            // 连接关闭标志

  int iov_cnt_;              // writev 的块数
  struct iovec iov_[2];      // iov_[0] 指向请求头，iov_[1] 指向映射的文件数据

  base::Buffer read_buff_;   // 读缓冲区
  base::Buffer write_buff_;  // 写缓冲区

  HttpRequest request_;      // HTTP 请求解析大脑
  HttpResponse response_;    // HTTP 响应生成器
};

}  // namespace http
}  // namespace web_server

#endif  // WEBSERVER_HTTP_HTTP_CONN_H_

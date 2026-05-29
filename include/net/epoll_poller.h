#ifndef WEBSERVER_NET_EPOLL_POLLER_H_
#define WEBSERVER_NET_EPOLL_POLLER_H_

#include <sys/epoll.h>
#include <vector>

namespace web_server {
namespace net {

// Epoll I/O 多路复用分发器
// 封装 Linux 的 Epoll 高性能接口
class EpollPoller {
 public:
  // 默认每次 wait 最大返回 1024 个事件
  explicit EpollPoller(int max_event = 1024);
  ~EpollPoller();

  // 禁用拷贝与赋值，因为系统级别的 FD 不应被隐式复制
  EpollPoller(const EpollPoller&) = delete;
  EpollPoller& operator=(const EpollPoller&) = delete;

  // 将 Socket FD 及其关注的事件 挂载到 Epoll 监听树上
  bool AddFd(int fd, uint32_t events);
  
  // 修改已挂载的 FD 的监听事件 
  bool ModFd(int fd, uint32_t events);
  
  // 将 FD 从 Epoll 的监听树中摘除
  bool DelFd(int fd);

  // 阻塞或非阻塞地等待事件发生
  // timeout_ms = -1 表示死等，0 表示立即返回，>0 表示带超时的阻塞
  // 返回发生事件的就绪连接数量
  int Wait(int timeout_ms = -1);

  // 获取第 i 个就绪事件对应的 Socket FD
  int GetEventFd(size_t i) const;
  
  // 获取第 i 个就绪事件的具体类型 (读、写、错误等)
  uint32_t GetEvents(size_t i) const;

 private:
  int epoll_fd_;                           // 由 epoll_create 返回的内核事件表句柄
  std::vector<struct epoll_event> events_; // 存放由内核返回的就绪事件数组
};

}  // namespace net
}  // namespace web_server

#endif  // WEBSERVER_NET_EPOLL_POLLER_H_

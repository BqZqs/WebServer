#include "net/epoll_poller.h"

#include <unistd.h>
#include <cassert>
#include <cstring>

namespace web_server {
namespace net {

EpollPoller::EpollPoller(int max_event) 
    : epoll_fd_(epoll_create(512)), // 参数 512 在现代 Linux 中会被忽略，只要大于 0 即可
      events_(max_event) {
  // 确保内核 Epoll 句柄创建成功且事件数组容量合法
  assert(epoll_fd_ >= 0 && events_.size() > 0);
}

EpollPoller::~EpollPoller() {
  // 离开生命周期时安全关闭内核分配的文件描述符
  close(epoll_fd_);
}

bool EpollPoller::AddFd(int fd, uint32_t events) {
  if (fd < 0) return false;
  
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.data.fd = fd;
  ev.events = events;
  
  // EPOLL_CTL_ADD: 往事件表中注册 fd 以及期望的事件 ev
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

bool EpollPoller::ModFd(int fd, uint32_t events) {
  if (fd < 0) return false;
  
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.data.fd = fd;
  ev.events = events;
  
  // EPOLL_CTL_MOD: 修改已经注册的 fd 的监听事件
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

bool EpollPoller::DelFd(int fd) {
  if (fd < 0) return false;
  
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  // 注意：在 Linux 内核 2.6.9 之前，即使是 DEL 操作，ev 也不能传 NULL
  // 传一个空的 ev 是为了向后兼容更老的 Linux 内核
  
  // EPOLL_CTL_DEL: 将 fd 从事件表中删除
  return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev);
}

int EpollPoller::Wait(int timeout_ms) {
  // epoll_wait 会将产生事件的 fd 及其状态拷贝到 events_ 数组中
  // 并返回就绪事件的个数
  return epoll_wait(epoll_fd_, &events_[0], static_cast<int>(events_.size()), timeout_ms);
}

int EpollPoller::GetEventFd(size_t i) const {
  assert(i < events_.size());
  return events_[i].data.fd;
}

uint32_t EpollPoller::GetEvents(size_t i) const {
  assert(i < events_.size());
  return events_[i].events;
}

}  // namespace net
}  // namespace web_server

#ifndef WEBSERVER_NET_TIMER_MANAGER_H_
#define WEBSERVER_NET_TIMER_MANAGER_H_

#include <chrono>
#include <functional>
#include <unordered_map>
#include <vector>

namespace web_server {
namespace net {

// 回调函数类型（通常用于包裹关闭 Socket 的逻辑）
using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

// 定时器节点
struct TimerNode {
  int id;               // 客户端 Socket FD
  TimeStamp expires;    // 绝对过期时间点
  TimeoutCallBack cb;   // 过期时触发的回调函数
  
  // 优先队列比较逻辑：时间越早的越小，放在堆顶
  bool operator<(const TimerNode& t) const {
    return expires < t.expires;
  }
};

// 定时器管理器 (基于最小堆 + 哈希表)
class TimerManager {
 public:
  TimerManager() { heap_.reserve(64); }
  ~TimerManager() { Clear(); }

  // 新增定时器。如果 id 已存在，则更新其超时时间并重置回调
  void Add(int id, int timeout_ms, const TimeoutCallBack& cb);
  
  // 仅调整已存在的定时器，用于客户端发送新数据时的“心跳续命”
  void Adjust(int id, int timeout_ms);
  
  // 主循环调用：剔除所有已超时的节点并执行回调
  void Tick();
  
  // 手动删除指定定时器并立即执行回调（如客户端主动断开）
  void DoWork(int id);
  
  // 清空所有定时器
  void Clear();
  
  // 核心！获取距离最近一个定时器超时所需的等待时间（毫秒）
  // 这个返回值将直接作为 epoll_wait 的 timeout 参数
  int GetNextTick();

 private:
  // 最小堆的核心操作算法
  void Del_(size_t index);
  void SiftUp_(size_t i);
  bool SiftDown_(size_t index, size_t n);
  void SwapNode_(size_t i, size_t j);

  std::vector<TimerNode> heap_;         // 底层使用动态数组实现最小二叉堆
  std::unordered_map<int, size_t> ref_; // 映射：Socket FD -> 该节点在 heap_ 数组中的索引
};

}  // namespace net
}  // namespace web_server

#endif  // WEBSERVER_NET_TIMER_MANAGER_H_

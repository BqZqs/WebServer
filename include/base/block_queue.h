#ifndef WEBSERVER_BASE_BLOCK_QUEUE_H_
#define WEBSERVER_BASE_BLOCK_QUEUE_H_

#include <cassert>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <sys/time.h>

namespace web_server {
namespace base {

// 线程安全的有界阻塞队列
// 用于实现异步日志
template <typename T>
class BlockQueue {
 public:
  explicit BlockQueue(size_t max_capacity = 1000);
  ~BlockQueue();

  void Clear();
  bool Empty();
  bool Full();
  void Close();
  size_t Size();
  size_t Capacity();
  // 用于清空阻塞队列内容
  void Flush();

  // 日志主线程调用，将缓冲内组装好数据压入阻塞队列
  void Push(const T& item);
  
  // 日志刷盘线程调用，将阻塞队列内日志内容写入文件
  bool Pop(T& item);
  // 暂未实现
  bool Pop(T& item, int timeout_ms);

 private:
  std::deque<T> deq_;
  size_t capacity_;                     // 队列最大容量
  bool is_close_;                       // 队列关闭标志位

  std::mutex mtx_;                      // 全局互斥锁
  std::condition_variable cond_consumer_; // 消费者条件变量 (非空唤醒)
  std::condition_variable cond_producer_; // 生产者条件变量 (非满唤醒)
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t max_capacity) 
    : capacity_(max_capacity), is_close_(false) {
  assert(max_capacity > 0);
}

template <typename T>
BlockQueue<T>::~BlockQueue() {
  Close();
}

template <typename T>
void BlockQueue<T>::Close() {
  {
    std::unique_lock<std::mutex> lock(mtx_);
    deq_.clear();
    is_close_ = true;
  }
  // 唤醒所有阻塞在 Push 或 Pop 上的线程，让它们安全退出
  cond_producer_.notify_all();
  cond_consumer_.notify_all();
}

template <typename T>
void BlockQueue<T>::Clear() {
  std::unique_lock<std::mutex> lock(mtx_);
  deq_.clear();
}

template <typename T>
bool BlockQueue<T>::Empty() {
  std::unique_lock<std::mutex> lock(mtx_);
  return deq_.empty();
}

template <typename T>
bool BlockQueue<T>::Full() {
  std::unique_lock<std::mutex> lock(mtx_);
  return deq_.size() >= capacity_;
}

template <typename T>
void BlockQueue<T>::Flush() {
  cond_consumer_.notify_one();
}

template <typename T>
void BlockQueue<T>::Push(const T& item) {
  std::unique_lock<std::mutex> lock(mtx_);
  // 队列满时，生产者等待
  cond_producer_.wait(lock, [this] { 
    return deq_.size() < capacity_ || is_close_; 
  });
  
  if (is_close_) return;
  
  deq_.push_back(item);
  cond_consumer_.notify_one(); // 唤醒一个消费者
}

template <typename T>
bool BlockQueue<T>::Pop(T& item) {
  std::unique_lock<std::mutex> lock(mtx_);
  // 队列空时，消费者等待
  cond_consumer_.wait(lock, [this] { 
    return !deq_.empty() || is_close_; 
  });
  
  if (is_close_ && deq_.empty()) {
    return false;
  }
  
  item = deq_.front();
  deq_.pop_front();
  cond_producer_.notify_one(); // 唤醒一个生产者
  return true;
}

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_BLOCK_QUEUE_H_

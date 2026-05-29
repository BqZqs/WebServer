#ifndef WEBSERVER_BASE_THREAD_POOL_H_
#define WEBSERVER_BASE_THREAD_POOL_H_

#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace web_server {
namespace base {

// 高性能线程池
class ThreadPool {
 public:
  explicit ThreadPool(size_t thread_count = 8);

  // 负责底层系统资源管理的类，明确禁用拷贝构造和赋值操作
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  ~ThreadPool();

  // 添加任务到队列中
  void AddTask(std::function<void()> task);

 private:
  // 工作线程容器
  std::vector<std::thread> workers_;
  // 任务队列
  std::queue<std::function<void()>> tasks_;

  // 互斥锁与条件变量，用于控制并发调度
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  
  // 线程池停止标志位
  bool stop_;
};

inline ThreadPool::ThreadPool(size_t thread_count) : stop_(false) {
  // 预先创建固定数量的工作线程，避免运行时的频繁创建/销毁开销
  for (size_t i = 0; i < thread_count; ++i) {
    workers_.emplace_back([this]() {
      while (true) {
        std::function<void()> task;

        {
          // 加锁
          std::unique_lock<std::mutex> lock(this->queue_mutex_);
          
          // 当线程池停止或有新任务时唤醒，防止虚假唤醒
          this->condition_.wait(lock, [this] { 
            return this->stop_ || !this->tasks_.empty(); 
          });

          // 退出条件：收到停止信号且任务队列已清空
          if (this->stop_ && this->tasks_.empty()) {
            return;
          }

          // 取出任务 (使用 std::move 避免 std::function 内部状态的拷贝)
          task = std::move(this->tasks_.front());
          this->tasks_.pop();
        }

        // 释放锁后执行任务，提高并发度
        if (task) {
          task();
        }
      }
    });
  }
}

inline void ThreadPool::AddTask(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    // 此处使用 assert 进行契约编程，确保不会向已关闭的线程池投递任务
    assert(!stop_ && "ThreadPool is stopped, cannot add new tasks");
    tasks_.emplace(std::move(task));
  }
  // 唤醒一个阻塞的工作线程
  condition_.notify_one();
}

inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  // 广播唤醒所有处于 wait 状态的线程，让它们检查 stop_ 标志并安全退出
  condition_.notify_all();
  
  // 阻塞主线程，等待所有工作线程平滑处理完积压任务并销毁
  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_THREAD_POOL_H_

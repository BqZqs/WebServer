#ifndef WEBSERVER_BASE_LOG_H_
#define WEBSERVER_BASE_LOG_H_

#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <cstdarg> // for va_list

#include "base/block_queue.h"
#include "base/buffer.h"

namespace web_server {
namespace base {

// 日志系统 (单例模式)
class Log {
 public:
  // 初始化系统：参数包含级别、路径、文件后缀、阻塞队列最大容量
  // 若 max_queue_capacity = 0，则退化为同步日志
  void Init(int level, const char* path = "./log", 
            const char* suffix = ".log", size_t max_queue_capacity = 1024);

  // 单例获取方式
  static Log& Instance() {
    static Log instance;
    return instance;
  }

  // 异步后端刷盘线程入口函数
  static void FlushLogThread() {
    Log::Instance().AsyncWrite_();
  }

  // 前端线程写日志函数
  void Write(int level, const char* format, ...);
  // 前端写完日志内容后调用，保证内容存入文件
  void Flush();

  // 获取日志级别
  int GetLevel() const { return level_; }
  // 设置日志级别
  void SetLevel(int level) { level_ = level; }
  bool IsOpen() const { return is_open_; }

 private:
  Log();
  virtual ~Log();
  Log(const Log&) = delete;
  Log& operator=(const Log&) = delete;

  void AppendLogLevelTitle_(int level);
  void AsyncWrite_(); // 后端实际的刷盘逻辑

  static const int kMaxLogLineLength = 256;
  static const int kMaxLogLines = 50000;  // 单个日志文件最大行数，超过则切割

  const char* path_;
  const char* suffix_;
  
  int max_lines_;
  int line_count_;
  int to_day_;
  bool is_open_;
  int level_;      // 日志输出最低级别 (0: Debug, 1: Info, 2: Warn, 3: Error)
  
  Buffer buff_;    // 线程内字符串格式化缓冲
  FILE* fp_;       // 日志文件指针
  
  std::unique_ptr<BlockQueue<std::string>> deque_; // 指向阻塞队列的指针
  std::unique_ptr<std::thread> write_thread_;      // 后端刷盘线程
  std::mutex mtx_;                                 // 文件写入互斥锁
};

// ================= 提供给上层的宏定义调用 =================
#define LOG_BASE(level, format, ...) \
    do { \
        if (web_server::base::Log::Instance().IsOpen() && \
            web_server::base::Log::Instance().GetLevel() <= level) { \
            web_server::base::Log::Instance().Write(level, format, ##__VA_ARGS__); \
            web_server::base::Log::Instance().Flush(); \
        } \
    } while (0);

// 支持自动追加换行符，并与 Printf 参数兼容
#define LOG_DEBUG(format, ...) do { LOG_BASE(0, format, ##__VA_ARGS__) } while (0);
#define LOG_INFO(format, ...)  do { LOG_BASE(1, format, ##__VA_ARGS__) } while (0);
#define LOG_WARN(format, ...)  do { LOG_BASE(2, format, ##__VA_ARGS__) } while (0);
#define LOG_ERROR(format, ...) do { LOG_BASE(3, format, ##__VA_ARGS__) } while (0);

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_LOG_H_

#include "base/log.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <cassert>
#include <cstring>

namespace web_server {
namespace base {

Log::Log() 
    : line_count_(0), to_day_(0), is_open_(false), level_(1), 
      fp_(nullptr), deque_(nullptr), write_thread_(nullptr) {}

Log::~Log() {
  if (write_thread_ && write_thread_->joinable()) {
    while (!deque_->Empty()) {
		deque_->Flush(); // 唤醒清空队列
    }
    deque_->Close();
    write_thread_->join();
  }
  if (fp_) {
    std::lock_guard<std::mutex> lock(mtx_);
    fflush(fp_);
    fclose(fp_);
    fp_ = nullptr;
  }
}

void Log::Init(int level, const char* path, const char* suffix, size_t max_queue_capacity) {
  is_open_ = true;
  level_ = level;
  path_ = path;
  suffix_ = suffix;

  // 1. 如果 max_queue_capacity > 0，则启用异步日志系统
  if (max_queue_capacity > 0) {
    deque_ = std::make_unique<BlockQueue<std::string>>(max_queue_capacity);
    write_thread_ = std::make_unique<std::thread>(FlushLogThread);
  }

  // 2. 检查日志目录是否存在，不存在则创建
  struct stat st;
  if (stat(path_, &st) != 0) {
    mkdir(path_, 0777);
  }

  // 3. 构建初始的文件名：日期+后缀
  time_t timer = time(nullptr);
  struct tm* sys_time = localtime(&timer);
  to_day_ = sys_time->tm_mday;
  
  char file_name[256] = {0};
  snprintf(file_name, sizeof(file_name) - 1, "%s/%04d_%02d_%02d%s", 
           path_, sys_time->tm_year + 1900, sys_time->tm_mon + 1, sys_time->tm_mday, suffix_);

  {
    std::lock_guard<std::mutex> lock(mtx_);
    buff_.RetrieveAll(); // 使用 Buffer 组件进行字符串拼接
    if (fp_) { 
        fflush(fp_);
        fclose(fp_); 
    }
    fp_ = fopen(file_name, "a");
    assert(fp_ != nullptr);
  }
}

void Log::AppendLogLevelTitle_(int level) {
  switch(level) {
    case 0: buff_.Append("[DEBUG]: ", 9); break;
    case 1: buff_.Append("[INFO] : ", 9); break;
    case 2: buff_.Append("[WARN] : ", 9); break;
    case 3: buff_.Append("[ERROR]: ", 9); break;
    default:buff_.Append("[INFO] : ", 9); break;
  }
}

// 前端线程调用的写函数
void Log::Write(int level, const char* format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, nullptr);
  time_t t_sec = now.tv_sec;
  struct tm* sys_time = localtime(&t_sec);

  // 1：日志自动按天/按行切割
  // 当日期发生变化，或者当前文件行数达到 max_lines_ 的倍数时，新建文件
  if (to_day_ != sys_time->tm_mday || (line_count_ && (line_count_ % kMaxLogLines == 0))) {
    std::unique_lock<std::mutex> lock(mtx_);
    lock.unlock(); // 尽早释放，减少竞争

    char new_file[256] = {0};
    if (to_day_ != sys_time->tm_mday) {
      // 跨天切割
      snprintf(new_file, sizeof(new_file) - 1, "%s/%04d_%02d_%02d%s", 
               path_, sys_time->tm_year + 1900, sys_time->tm_mon + 1, sys_time->tm_mday, suffix_);
      to_day_ = sys_time->tm_mday;
      line_count_ = 0;
    } else {
      // 当天内容过多，按分卷切割
      snprintf(new_file, sizeof(new_file) - 1, "%s/%04d_%02d_%02d-%d%s", 
               path_, sys_time->tm_year + 1900, sys_time->tm_mon + 1, sys_time->tm_mday, 
               (line_count_ / kMaxLogLines), suffix_);
    }

    lock.lock();
    fflush(fp_);
    fclose(fp_);
    fp_ = fopen(new_file, "a");
    assert(fp_ != nullptr);
  }

  // 2：格式化组装日志内容
  {
    std::lock_guard<std::mutex> lock(mtx_);
    line_count_++;
    
    // 写入时间戳 yyyy-MM-dd HH:mm:ss.uuuuuu
    int n = snprintf(buff_.BeginWrite(), 128, "%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
                     sys_time->tm_year + 1900, sys_time->tm_mon + 1, sys_time->tm_mday,
                     sys_time->tm_hour, sys_time->tm_min, sys_time->tm_sec, now.tv_usec);
    buff_.HasWritten(n);

    // 写入级别
    AppendLogLevelTitle_(level);

    // 写入用户的变参信息 (va_list)
    va_list va_list;
    va_start(va_list, format);
    // vsnprintf 解析变长参数并写入
    int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, va_list);
    va_end(va_list);

    buff_.HasWritten(m);
    buff_.Append("\n\0", 2);

    // 3：交由前端/后端处理
    // 将整个 Buffer 提取为 string
    if (deque_ && !deque_->Full()) {
      deque_->Push(buff_.RetrieveAllToStr()); // 异步写入：扔进队列
    } else {
      // 存在问题，写日志时如果后端刷盘线程阻塞队列满，前端强制写会导致日志乱序，但是强制push会被阻塞
      fputs(buff_.Peek(), fp_);               // 同步写入或队列满：直接写磁盘
      buff_.RetrieveAll();
    }
  }
}

void Log::Flush() {
  if (deque_) {
    // 异步模式下 Flush，只需唤醒后端即可
	deque_->Flush();
  }
  // 强制刷盘，防止意外宕机丢失 OS 缓冲区的数据
  fflush(fp_);
}

// 后端工作线程：死循环提取数据并刷盘
void Log::AsyncWrite_() {
  std::string log_str;
  while (deque_->Pop(log_str)) {
    std::lock_guard<std::mutex> lock(mtx_);
    fputs(log_str.c_str(), fp_);
  }
}

}  // namespace base
}  // namespace web_server

#ifndef WEBSERVER_BASE_BUFFER_H_
#define WEBSERVER_BASE_BUFFER_H_

#include <atomic>
#include <string>
#include <vector>

namespace web_server {
namespace base {

// 应用层缓冲区，支持自动扩容与分散读操作
// 内部是一段连续内存: [ prependable ] [ readable ] [ writable ]
class Buffer {
 public:
  // 默认初始大小 1024 字节
  static constexpr size_t kInitialSize = 1024;

  explicit Buffer(size_t init_buff_size = kInitialSize);
  ~Buffer() = default;

  // 获取各种区域的字节数
  size_t WritableBytes() const;
  size_t ReadableBytes() const;
  size_t PrependableBytes() const;

  // 返回可读数据的起始只读指针
  const char* Peek() const;

  // 确保有足够的空间可写，如果不够则自动扩容或碎片整理
  void EnsureWritable(size_t len);
  
  // 写入数据后，移动写指针
  void HasWritten(size_t len);

  // 取出指定长度的数据（移动读指针）
  void Retrieve(size_t len);
  // 取出数据直到某个特定的位置（用于解析 HTTP 报文按 \r\n 截断）
  void RetrieveUntil(const char* end);
  // 清空缓冲区
  void RetrieveAll();
  // 取出所有数据并转化为 std::string 类型
  std::string RetrieveAllToStr();

  // 返回可写区域的起始指针
  const char* BeginWriteConst() const;
  char* BeginWrite();

  // 向缓冲区追加数据
  void Append(const std::string& str);
  void Append(const char* str, size_t len);
  void Append(const void* data, size_t len);
  void Append(const Buffer& buff);

  // 与 Socket FD 直接交互的网络 I/O 接口
  // save_errno 向上层抛出 EAGAIN 或 EWOULDBLOCK 等非阻塞状态
  ssize_t ReadFd(int fd, int* save_errno);
  ssize_t WriteFd(int fd, int* save_errno);

 private:
  // 返回底层 vector 的首地址
  char* BeginPtr_();
  const char* BeginPtr_() const;
  
  // 空间整理与扩容核心逻辑
  void MakeSpace_(size_t len);

  std::vector<char> buffer_;
  size_t read_pos_;   // 读位移标记
  size_t write_pos_;  // 写位移标记
};

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_BUFFER_H_

#include "base/buffer.h"

#include <sys/uio.h>   // readv
#include <unistd.h>    // read, write
#include <cassert>
#include <cerrno>
#include <cstring>
#include <algorithm>   // std::copy

namespace web_server {
namespace base {

Buffer::Buffer(size_t init_buff_size)
    : buffer_(init_buff_size), read_pos_(0), write_pos_(0) {}

size_t Buffer::ReadableBytes() const { return write_pos_ - read_pos_; }

size_t Buffer::WritableBytes() const { return buffer_.size() - write_pos_; }

size_t Buffer::PrependableBytes() const { return read_pos_; }

const char* Buffer::Peek() const { return BeginPtr_() + read_pos_; }

void Buffer::Retrieve(size_t len) {
  assert(len <= ReadableBytes());
  if (len < ReadableBytes()) {
    read_pos_ += len;
  } else {
    RetrieveAll();
  }
}

void Buffer::RetrieveUntil(const char* end) {
  assert(Peek() <= end);
  Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
  read_pos_ = 0;
  write_pos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
  std::string str(Peek(), ReadableBytes());
  RetrieveAll();
  return str;
}

const char* Buffer::BeginWriteConst() const { return BeginPtr_() + write_pos_; }

char* Buffer::BeginWrite() { return BeginPtr_() + write_pos_; }

void Buffer::HasWritten(size_t len) { write_pos_ += len; }

void Buffer::Append(const std::string& str) { Append(str.data(), str.length()); }

void Buffer::Append(const void* data, size_t len) {
  assert(data);
  Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
  assert(str);
  EnsureWritable(len);
  std::copy(str, str + len, BeginWrite());
  HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
  Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWritable(size_t len) {
  if (WritableBytes() < len) {
    MakeSpace_(len);
  }
  assert(WritableBytes() >= len);
}

// 核心 I/O 方法：从套接字读取数据
// 使用 readv 结合栈上临时空间，极大减少系统调用并动态自适应数据量
ssize_t Buffer::ReadFd(int fd, int* save_errno) {
  // 64KB 栈上临时缓冲区，应对突发的大流量数据
  char extra_buff[65536];
  struct iovec iov[2];
  
  const size_t writable = WritableBytes();
  
  // 第一块缓冲区：Buffer 内部的剩余可写空间
  iov[0].iov_base = BeginPtr_() + write_pos_;
  iov[0].iov_len = writable;
  
  // 第二块缓冲区：栈上的 64KB 临时空间
  iov[1].iov_base = extra_buff;
  iov[1].iov_len = sizeof(extra_buff);

  // 如果内部空间大于 64KB，就没必要用临时栈空间了
  const int iovcnt = (writable < sizeof(extra_buff)) ? 2 : 1;
  const ssize_t len = readv(fd, iov, iovcnt);

  if (len < 0) {
    *save_errno = errno;
  } else if (static_cast<size_t>(len) <= writable) {
    // 内部空间足够，直接移动写指针
    write_pos_ += len;
  } else {
    // 内部空间被塞满，超出的部分被写到了 extra_buff 里
    write_pos_ = buffer_.size();
    Append(extra_buff, len - writable); // Append 会自动触发扩容
  }
  return len;
}

// 核心 I/O 方法：向套接字写入数据
ssize_t Buffer::WriteFd(int fd, int* save_errno) {
  size_t read_size = ReadableBytes();
  ssize_t len = write(fd, Peek(), read_size);
  if (len < 0) {
    *save_errno = errno;
    return len;
  }
  read_pos_ += len;
  return len;
}

char* Buffer::BeginPtr_() { return &buffer_[0]; }

const char* Buffer::BeginPtr_() const { return &buffer_[0]; }

// 内存复用与扩容策略
void Buffer::MakeSpace_(size_t len) {
  // 剩余可写空间 + 已被读过废弃的空间（读指针前的空间） 如果都不够 len，则进行物理扩容
  if (WritableBytes() + PrependableBytes() < len) {
    buffer_.resize(write_pos_ + len + 1);
  } else {
    // 空间足够，只是因为之前数据的写入/读出导致碎片化，将现存数据往前搬运（数据紧凑）
    size_t readable = ReadableBytes();
    std::copy(BeginPtr_() + read_pos_, BeginPtr_() + write_pos_, BeginPtr_());
    read_pos_ = 0;
    write_pos_ = read_pos_ + readable;
    assert(readable == ReadableBytes());
  }
}

}  // namespace base
}  // namespace web_server

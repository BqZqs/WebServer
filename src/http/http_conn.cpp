#include "http/http_conn.h"

#include <unistd.h>
#include <cstring>

namespace web_server {
namespace http {

// 静态成员初始化
const char* HttpConn::src_dir;
std::atomic<int> HttpConn::user_count;
bool HttpConn::is_et;

HttpConn::HttpConn() : fd_(-1), is_close_(true) {
	memset(&addr_, 0, sizeof(addr_));
}

HttpConn::~HttpConn() { 
  Close(); 
}

void HttpConn::Init(int sock_fd, const sockaddr_in& addr) {
  assert(sock_fd > 0);
  user_count++;
  addr_ = addr;
  fd_ = sock_fd;
  write_buff_.RetrieveAll();
  read_buff_.RetrieveAll();
  is_close_ = false;
  LOG_INFO("Client[%d](%s:%d) in, user_count:%d", fd_, GetIP(), GetPort(), static_cast<int>(user_count));
}

void HttpConn::Close() {
  response_.UnmapFile();
  if (is_close_ == false) {
    is_close_ = true; 
    user_count--;
    close(fd_);
    LOG_INFO("Client[%d](%s:%d) quit, user_count:%d", fd_, GetIP(), GetPort(), static_cast<int>(user_count));
  }
}

int HttpConn::GetFd() const { return fd_; }
struct sockaddr_in HttpConn::GetAddr() const { return addr_; }
const char* HttpConn::GetIP() const { return inet_ntoa(addr_.sin_addr); }
int HttpConn::GetPort() const { return ntohs(addr_.sin_port); }

// 调用 Buffer 封装好的分散读
ssize_t HttpConn::Read(int* save_errno) {
  ssize_t len = -1;
  do {
    len = read_buff_.ReadFd(fd_, save_errno);
    if (len <= 0) {
      break;
    }
  } while (is_et); // 如果是 ET (边缘触发) 模式，必须一次性把 Socket 里的数据读空
  return len;
}

// ================== 核心业务逻辑驱动 ==================
bool HttpConn::Process() {
  request_.Init();
  
  if (read_buff_.ReadableBytes() <= 0) {
    return false;
  }
  
  // 1. 调用 HttpRequest 解析读到的数据
  if (request_.Parse(read_buff_)) {
    // 解析成功，生成 200 OK
    LOG_DEBUG("%s", request_.path().c_str());
    response_.Init(src_dir, request_.path(), request_.IsKeepAlive(), 200);
  } else {
    // 解析失败（报文损坏或格式不对），生成 400 Bad Request
    response_.Init(src_dir, request_.path(), false, 400);
  }

  // 2. 调用 HttpResponse 生成响应头和映射文件
  response_.MakeResponse(write_buff_);
  
  // 3. 将生成的 Header 和 File 绑定到 iovec，准备发送
  iov_[0].iov_base = const_cast<char*>(write_buff_.Peek());
  iov_[0].iov_len = write_buff_.ReadableBytes();
  iov_cnt_ = 1;

  // 如果请求的是真实存在的文件且大小大于 0，就挂载第二块 iovec
  if (response_.FileLen() > 0 && response_.File()) {
    iov_[1].iov_base = response_.File();
    iov_[1].iov_len = response_.FileLen();
    iov_cnt_ = 2;
  }
  
  LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iov_cnt_, ToWriteBytes());
  return true;
}

// ================== 聚集写与动态位移核心逻辑 ==================
ssize_t HttpConn::Write(int* save_errno) {
  ssize_t len = -1;
  do {
    // writev: 将多块分散的内存数据一并写入 Socket
    len = writev(fd_, iov_, iov_cnt_);
    
    if (len <= 0) {
      *save_errno = errno;
      break;
    }

    // 全部发送完毕
    if (iov_[0].iov_len + iov_[1].iov_len == 0) {
      break; 
    }

    // ===================================================
    // 判断写入的数据量，进行指针位移操作 (应对非阻塞 I/O 的部分发送)
    // ===================================================
    if (static_cast<size_t>(len) > iov_[0].iov_len) {
      // 走到这里说明 Header 已经全部发完，并且 File 数据也发了一部分
      
      // 更新第二块数据的指针和长度：减去已发送的 File 部分 (len - 之前的 Header 长度)
      iov_[1].iov_base = (uint8_t*)iov_[1].iov_base + (len - iov_[0].iov_len);
      iov_[1].iov_len -= (len - iov_[0].iov_len);
      
      // Header 彻底清空
      if (iov_[0].iov_len) {
        write_buff_.RetrieveAll();
        iov_[0].iov_len = 0;
      }
    } else {
      // 走到这里说明连 Header 都还没发完，网卡缓冲区就满了
      
      // 更新第一块数据（Header）的指针和长度
      iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
      iov_[0].iov_len -= len;
      write_buff_.Retrieve(len);
    }
  } while (is_et || ToWriteBytes() > 10240); // ET 模式下或数据过多时循环写入

  return len;
}

bool HttpConn::IsKeepAlive() const {
  return request_.IsKeepAlive();
}

}  // namespace http
}  // namespace web_server

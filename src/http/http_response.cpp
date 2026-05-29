#include "http/http_response.h"

#include <fcntl.h>       // open
#include <sys/mman.h>    // mmap, munmap
#include <unistd.h>      // close
#include <cassert>
#include <cstring>

#include "base/log.h"

namespace web_server {
namespace http {

// 静态成员初始化：文件后缀与 MIME 类型映射
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html",   "text/html"},
    {".xml",    "text/xml"},
    {".xhtml",  "application/xhtml+xml"},
    {".txt",    "text/plain"},
    {".rtf",    "application/rtf"},
    {".pdf",    "application/pdf"},
    {".word",   "application/msword"},
    {".png",    "image/png"},
    {".gif",    "image/gif"},
    {".jpg",    "image/jpeg"},
    {".jpeg",   "image/jpeg"},
    {".au",     "audio/basic"},
    {".mpeg",   "video/mpeg"},
    {".mpg",    "video/mpeg"},
    {".avi",    "video/x-msvideo"},
    {".gz",     "application/x-gzip"},
    {".tar",    "application/x-tar"},
    {".css",    "text/css"},
    {".js",     "text/javascript"},
};

// 状态码及其描述映射
const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

// 错误状态码跳转页面映射
const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse() : code_(-1), is_keep_alive_(false), mm_file_(nullptr) {
	memset(&mm_file_stat_, 0, sizeof(mm_file_stat_));
}

HttpResponse::~HttpResponse() {
  UnmapFile();
}

void HttpResponse::Init(const std::string& src_dir, std::string& path, 
                        bool is_keep_alive, int code) {
  assert(!src_dir.empty());
  if (mm_file_) { UnmapFile(); }
  
  code_ = code;
  is_keep_alive_ = is_keep_alive;
  path_ = path;
  src_dir_ = src_dir;
  mm_file_ = nullptr;
  memset(&mm_file_stat_, 0, sizeof(mm_file_stat_));
}

void HttpResponse::MakeResponse(base::Buffer& buff) {
  // 判断请求的资源文件
  // stat 获取文件状态，如果返回值 < 0 说明文件不存在
  if (stat((src_dir_ + path_).data(), &mm_file_stat_) < 0 || S_ISDIR(mm_file_stat_.st_mode)) {
    code_ = 404;
  } else if (!(mm_file_stat_.st_mode & S_IROTH)) {
    code_ = 403; // 其他用户无读权限
  } else if (code_ == -1) { 
    code_ = 200; // 权限正常且存在
  }

  // 发生错误时，自动将路径重定向到对应的错误 HTML 页面
  ErrorHtml_();

  // 严格按 HTTP 协议规范依序拼接
  AddStateLine_(buff);
  AddHeader_(buff);
  AddContent_(buff);
}

char* HttpResponse::File() const { return mm_file_; }

size_t HttpResponse::FileLen() const { return mm_file_stat_.st_size; }

void HttpResponse::ErrorHtml_() {
  if (CODE_PATH.count(code_) == 1) {
    path_ = CODE_PATH.at(code_);
    stat((src_dir_ + path_).data(), &mm_file_stat_);
  }
}

void HttpResponse::AddStateLine_(base::Buffer& buff) {
  std::string status;
  if (CODE_STATUS.count(code_) == 1) {
    status = CODE_STATUS.at(code_);
  } else {
    code_ = 400;
    status = CODE_STATUS.at(400);
  }
  buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(base::Buffer& buff) {
  buff.Append("Connection: ");
  if (is_keep_alive_) {
    buff.Append("keep-alive\r\n");
    buff.Append("keep-alive: max=6, timeout=120\r\n");
  } else {
    buff.Append("close\r\n");
  }
  buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(base::Buffer& buff) {
  int src_fd = open((src_dir_ + path_).data(), O_RDONLY);
  if (src_fd < 0) {
    // 连 404 页面都没有的情况，直接输出纯文本兜底
    ErrorContent(buff, "File Not Found!");
    return; 
  }

  // ============== 核心：mmap 内存映射 ==============
  // MAP_PRIVATE 建立一个写入时拷贝的私有映射（只读安全）
  void* mm_ret = mmap(0, mm_file_stat_.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
  if (mm_ret == MAP_FAILED) {
    ErrorContent(buff, "File Read Error!");
    close(src_fd);
    return;
  }
  
  mm_file_ = static_cast<char*>(mm_ret);
  close(src_fd); // 映射成功后，文件描述符即可关闭，不占用 FD 额度

  // 追加 Content-length 首部，并在 Header 结束后增加最后的空行 \r\n
  buff.Append("Content-length: " + std::to_string(mm_file_stat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
  if (mm_file_) {
    munmap(mm_file_, mm_file_stat_.st_size);
    mm_file_ = nullptr;
  }
}

std::string HttpResponse::GetFileType_() {
  // 寻找路径中最后一个 '.'
  std::string::size_type idx = path_.find_last_of('.');
  if (idx == std::string::npos) {
    return "text/plain";
  }
  std::string suffix = path_.substr(idx);
  if (SUFFIX_TYPE.count(suffix) == 1) {
    return SUFFIX_TYPE.at(suffix);
  }
  return "text/plain";
}

void HttpResponse::ErrorContent(base::Buffer& buff, std::string message) {
  std::string body;
  std::string status;
  if (CODE_STATUS.count(code_) == 1) {
    status = CODE_STATUS.at(code_);
  } else {
    status = "Bad Request";
  }
  // 如果读取磁盘文件失败，直接用 string 组装一段极简 HTML
  body += "<html><title>Error</title>";
  body += "<body bgcolor=\"ffffff\">";
  body += std::to_string(code_) + " : " + status + "\n";
  body += "<p>" + message + "</p>";
  body += "<hr><em>WebServer</em></body></html>";

  buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
  buff.Append(body);
}

}  // namespace http
}  // namespace web_server

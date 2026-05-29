#include "http/http_request.h"

#include <algorithm>
#include <cassert>

namespace web_server {
namespace http {

// 静态成员初始化：定义默认路由映射
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture"};

// 静态成员初始化：定义简单的登录注册路由标识
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0}, {"/login.html", 1}};

void HttpRequest::Init() {
  method_ = path_ = version_ = body_ = "";
  state_ = ParseState::REQUEST_LINE;
  header_.clear();
  post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
  if (header_.count("Connection") == 1) {
    return header_.at("Connection") == "keep-alive" && version_ == "1.1";
  }
  // HTTP/1.1 默认长连接
  return version_ == "1.1";
}

bool HttpRequest::Parse(base::Buffer& buff) {
  const char CRLF[] = "\r\n";
  if (buff.ReadableBytes() <= 0) {
    return false;
  }

  // 从状态机驱动逻辑：不断从 Buffer 寻找 \r\n，截取一整行交给主状态机处理
  while (buff.ReadableBytes() && state_ != ParseState::FINISH) {
    // 寻找行结束符
    const char* line_end = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
    std::string line(buff.Peek(), line_end);

    switch (state_) {
      case ParseState::REQUEST_LINE:
        if (!ParseRequestLine_(line)) {
          return false; // 请求行格式错误
        }
        ParsePath_();   // 映射默认路由
        break;

      case ParseState::HEADERS:
        ParseHeader_(line);
        // 如果读到了空行，说明 Header 结束。若是 GET，则整个请求结束；若是 POST，则准备读 Body。
        if (buff.ReadableBytes() <= 2) {
          state_ = ParseState::FINISH;
        }
        break;

      case ParseState::BODY:
        ParseBody_(line);
        break;

      default:
        break;
    }

    // 将刚才读过的这一行连同 \r\n 一起从 Buffer 中移除
    if (line_end == buff.BeginWriteConst()) {
      break; 
    }
    buff.RetrieveUntil(line_end + 2);
  }
  return true;
}

void HttpRequest::ParsePath_() {
  // 如果请求的是根目录，默认映射到 index.html
  if (path_ == "/") {
    path_ = "/index.html";
  } else {
    // 处理无后缀的伪静态路由 (例如 /login 映射为 /login.html)
    if (DEFAULT_HTML.count(path_)) {
      path_ += ".html";
    }
  }
}

bool HttpRequest::ParseRequestLine_(const std::string& line) {
  // 预期格式: "GET /index.html HTTP/1.1"
  size_t pos_method = line.find(' ');
  if (pos_method == std::string::npos) return false;

  size_t pos_path = line.find(' ', pos_method + 1);
  if (pos_path == std::string::npos) return false;

  method_ = line.substr(0, pos_method);
  path_ = line.substr(pos_method + 1, pos_path - pos_method - 1);
  version_ = line.substr(pos_path + 1);

  state_ = ParseState::HEADERS; // 状态转换
  return true;
}

void HttpRequest::ParseHeader_(const std::string& line) {
  // 预期格式: "Host: 127.0.0.1"
  size_t pos = line.find(':');
  if (pos == std::string::npos) {
    // 读到空行，Header 结束
    if (method_ == "POST") {
      state_ = ParseState::BODY; // 转换到 Body
    } else {
      state_ = ParseState::FINISH; // GET/HEAD 等没有 Body 的请求，直接结束
    }
    return;
  }
  
  // 提取键和值，跳过冒号后的空格
  std::string key = line.substr(0, pos);
  std::string value = line.substr(pos + 2);
  header_[key] = value;
}

void HttpRequest::ParseBody_(const std::string& line) {
  body_ = line;
  ParsePost_();
  state_ = ParseState::FINISH; 
}

// POST 表单数据的深入解析

int HttpRequest::ConverHex_(char ch) {
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return ch - '0';
}

void HttpRequest::ParsePost_() {
  if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
    ParseFromUrlEncoded_();
    
    // (留白扩展)：如果此处 path_ 为 /login.html，可以在此衔接 SqlConnPool 验证账号密码
    if (DEFAULT_HTML_TAG.count(path_)) {
      int tag = DEFAULT_HTML_TAG.at(path_);
      if (tag == 0 || tag == 1) {
        // bool isLogin = (tag == 1);
        // UserVerify(post_["username"], post_["password"], isLogin);
      }
    }
  }
}

void HttpRequest::ParseFromUrlEncoded_() {
  if (body_.empty()) return;

  std::string key, value;
  int num = 0;
  int n = body_.size();
  int i = 0, j = 0;

  for (; i < n; i++) {
    char ch = body_[i];
    switch (ch) {
      case '=':
        key = body_.substr(j, i - j);
        j = i + 1;
        break;
      case '+': // URL 编码中，空格可能被替换为 +
        body_[i] = ' ';
        break;
      case '%': // URL 解码 (如 %20 -> 空格)
        // 此处为无效解码，并没有将%20转化为空格，而是将 2 和 0 分别替换
        // 这是为了简化处理逻辑，因为%20占3位，空格站1位，需要进行移位操作，影响循环进行
        num = ConverHex_(body_[i + 1]) * 16 + ConverHex_(body_[i + 2]);
        body_[i + 2] = num % 10 + '0';
        body_[i + 1] = num / 10 + '0';
        i += 2;
        break;
      case '&': // 键值对分割
        value = body_.substr(j, i - j);
        j = i + 1;
        post_[key] = value;
        break;
      default:
        break;
    }
  }
  assert(j <= i);
  // 插入最后一个键值对
  if (post_.count(key) == 0 && j < i) {
    value = body_.substr(j, i - j);
    post_[key] = value;
  }
}

std::string HttpRequest::path() const { return path_; }
std::string& HttpRequest::path() { return path_; }
std::string HttpRequest::method() const { return method_; }
std::string HttpRequest::version() const { return version_; }
std::string HttpRequest::GetPost(const std::string& key) const {
  if (post_.count(key) == 1) return post_.at(key);
  return "";
}
std::string HttpRequest::GetPost(const char* key) const {
  if (post_.count(key) == 1) return post_.at(key);
  return "";
}

}  // namespace http
}  // namespace web_server

#ifndef WEBSERVER_HTTP_HTTP_REQUEST_H_
#define WEBSERVER_HTTP_HTTP_REQUEST_H_

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "base/buffer.h"

namespace web_server {
namespace http {

// 主状态机的状态枚举
enum class ParseState {
  REQUEST_LINE, // 正在解析请求行 (例如: GET /index.html HTTP/1.1)
  HEADERS,      // 正在解析请求头 (例如: Host: 127.0.0.1)
  BODY,         // 正在解析请求体 (例如: POST 表单数据)
  FINISH,       // 解析完成
};

// HTTP 解析结果状态码
enum class HttpCode {
  NO_REQUEST = 0,    // 请求不完整，需要继续读取 Socket
  GET_REQUEST,       // 成功解析到一个完整的请求
  BAD_REQUEST,       // 报文语法错误
  NO_RESOURCE,       // 请求资源不存在 (常留给 Response 处理)
  FORBIDDEN_REQUEST, // 无权限访问
  FILE_REQUEST,      // 文件请求
  INTERNAL_ERROR,    // 服务器内部错误
  CLOSED_CONNECTION, // 客户端关闭连接
};

// HTTP 请求解析器
class HttpRequest {
 public:
  HttpRequest() { Init(); }
  ~HttpRequest() = default;

  // 复位解析器，为下一个请求做准备 (Keep-Alive 场景)
  void Init();

  // 核心入口：从 Buffer 中消费数据并执行状态机解析
  bool Parse(base::Buffer& buff);

  // 获取解析后的属性
  std::string path() const;
  std::string& path();
  std::string method() const;
  std::string version() const;
  std::string GetPost(const std::string& key) const;
  std::string GetPost(const char* key) const;

  // 检查是否保持长连接 (HTTP/1.1 默认 Keep-Alive)
  bool IsKeepAlive() const;

 private:
  // 主状态机的三个处理动作
  bool ParseRequestLine_(const std::string& line);
  void ParseHeader_(const std::string& line);
  void ParseBody_(const std::string& line);

  // POST 请求体的额外解析逻辑（如 application/x-www-form-urlencoded）
  void ParsePath_();
  void ParsePost_();
  void ParseFromUrlEncoded_();
  
  // 辅助函数：将 URL 编码字符（如 %20）解码为普通字符
  static int ConverHex_(char ch);

  ParseState state_;
  std::string method_;
  std::string path_;
  std::string version_;
  std::string body_;

  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_set<std::string> DEFAULT_HTML;
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
};

}  // namespace http
}  // namespace web_server

#endif  // WEBSERVER_HTTP_HTTP_REQUEST_H_

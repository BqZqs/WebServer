#ifndef WEBSERVER_HTTP_HTTP_REQUEST_H_
#define WEBSERVER_HTTP_HTTP_REQUEST_H_

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "base/buffer.h"

namespace web_server {
namespace http {

// 主状态机
enum class ParseState {
  REQUEST_LINE, // 正在解析请求行 
  HEADERS,      // 正在解析请求头 
  BODY,         // 正在解析请求体 
  FINISH,       // 解析完成
};

// HTTP 解析结果状态码
enum class HttpCode {
  NO_REQUEST = 0,    // 请求不完整，需要继续读取 Socket
  GET_REQUEST,       // 成功解析到一个完整的请求
  BAD_REQUEST,       // 报文语法错误
  NO_RESOURCE,       // 请求资源不存在
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

  // 复位解析器，为下一个请求做准备，用于Keep-Alive
  void Init();

  // 从 Buffer 中取出数据并执行状态机解析
  bool Parse(base::Buffer& buff);

  // 获取解析后的属性
  std::string path() const;
  std::string& path();
  std::string method() const;
  std::string version() const;
  std::string GetPost(const std::string& key) const;
  std::string GetPost(const char* key) const;

  // 检查是否保持长连接 
  bool IsKeepAlive() const;

 private:
  // 主状态机的三个处理动作
  bool ParseRequestLine_(const std::string& line);
  void ParseHeader_(const std::string& line);
  void ParseBody_(const std::string& line);

  // POST 请求体的额外解析逻辑
  void ParsePath_();
  void ParsePost_();
  void ParseFromUrlEncoded_();
  
  // 辅助函数：将 URL 编码字符解码为普通字符
  static int ConverHex_(char ch);

  ParseState state_;
  std::string method_;
  std::string path_;
  std::string version_;
  std::string body_;

  std::unordered_map<std::string, std::string> header_; // 请求header键值对解析结果
  std::unordered_map<std::string, std::string> post_;   // POST请求Body解析结果

  static const std::unordered_set<std::string> DEFAULT_HTML;    // 定义默认映射路由
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;   // 登录注册路由标识
};

}  // namespace http
}  // namespace web_server

#endif  // WEBSERVER_HTTP_HTTP_REQUEST_H_

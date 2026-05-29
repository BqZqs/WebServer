#ifndef WEBSERVER_HTTP_HTTP_RESPONSE_H_
#define WEBSERVER_HTTP_HTTP_RESPONSE_H_

#include <sys/stat.h>
#include <string>
#include <unordered_map>

#include "base/buffer.h"

namespace web_server {
namespace http {

// HTTP 响应生成器
class HttpResponse {
 public:
  HttpResponse();
  ~HttpResponse();

  // 初始化响应状态
  void Init(const std::string& src_dir, std::string& path, 
            bool is_keep_alive = false, int code = -1);

  // 核心逻辑：将组装完整的 HTTP 响应报文并写入 Buffer
  void MakeResponse(base::Buffer& buff);

  // 清理解除内存映射
  void UnmapFile();

  // 提供给 HttpConn 获取映射好的文件内存指针和长度
  char* File() const;
  size_t FileLen() const;

  // 错误页面的 HTML 生成
  void ErrorContent(base::Buffer& buff, std::string message);

  // 获取状态码
  int code() const { return code_; }

 private:
  // 组装报文的具体步骤
  void AddStateLine_(base::Buffer& buff);
  void AddHeader_(base::Buffer& buff);
  void AddContent_(base::Buffer& buff);

  // 错误页响应逻辑
  void ErrorHtml_();
  // 根据文件后缀推断 MIME 类型
  std::string GetFileType_();

  int code_;                  // HTTP 状态码 
  bool is_keep_alive_;        // 是否保持长连接

  std::string path_;          // 请求的相对路径
  std::string src_dir_;       // 网站根目录绝对路径
  
  char* mm_file_;             // mmap 内存映射指针
  struct stat mm_file_stat_;  // 映射文件的状态属性

  // 静态哈希表：状态码、MIME 类型推导
  static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
  static const std::unordered_map<int, std::string> CODE_STATUS;
  static const std::unordered_map<int, std::string> CODE_PATH;
};

}  // namespace http
}  // namespace web_server

#endif  // WEBSERVER_HTTP_HTTP_RESPONSE_H_

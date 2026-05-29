#ifndef WEBSERVER_BASE_CONFIG_H_
#define WEBSERVER_BASE_CONFIG_H_

#include <string>
#include <unordered_map>

namespace web_server {
namespace base {

// 配置解析器
// 负责读取键值对格式的 .conf 文件，屏蔽空行与注释，提供类型安全的查询接口
class Config {
 public:
  Config() = default;
  ~Config() = default;

  // 禁用拷贝与赋值操作，确保配置数据的全局唯一性引用
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;

  // 加载解析配置文件，成功返回 true，失败（如文件不存在）返回 false
  bool LoadFile(const std::string& file_path);

  // 类型安全的数据获取接口，并支持设定默认值 
  int GetInt(const std::string& key, int default_value = 0) const;
  std::string GetString(const std::string& key, const std::string& default_value = "") const;

 private:
  // 辅助函数：去除字符串首尾的空格与不可见字符
  static void Trim_(std::string& str);

  // 底层使用哈希表存储解析后的键值对，保证 O(1) 的查询复杂度
  std::unordered_map<std::string, std::string> data_;
};

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_CONFIG_H_

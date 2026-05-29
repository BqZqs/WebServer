#include "base/config.h"

#include <fstream>
#include <algorithm>

namespace web_server {
namespace base {

bool Config::LoadFile(const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    Trim_(line);
    
    // 过滤空行以及以 '#' 开头的注释行
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // 寻找键值对的分隔符 '='
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;  // 格式不规范的行直接跳过，容错处理
    }

    // 提取 Key 和 Value
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);

    // 去除键和值两侧可能包含的多余空格 (例如: "port = 8080")
    Trim_(key);
    Trim_(value);

    // 存入哈希表
    if (!key.empty()) {
      data_[key] = value;
    }
  }
  
  file.close();
  return true;
}

int Config::GetInt(const std::string& key, int default_value) const {
  auto it = data_.find(key);
  if (it == data_.end()) {
    return default_value;
  }
  
  try {
    // std::stoi 在转换非数字字符串时会抛出异常
    return std::stoi(it->second);
  } catch (...) {
    // 捕获到格式错误时平滑降级，返回默认值而不是让整个 Server 崩溃
    return default_value;
  }
}

std::string Config::GetString(const std::string& key, const std::string& default_value) const {
  auto it = data_.find(key);
  if (it != data_.end()) {
    return it->second;
  }
  return default_value;
}

void Config::Trim_(std::string& str) {
  if (str.empty()) return;
  
  // 查找第一个非空字符 (\t, \r, \n, 空格)
  auto start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    str.clear();
    return;
  }
  
  // 查找最后一个非空字符
  auto end = str.find_last_not_of(" \t\r\n");
  
  // 截取核心部分
  str = str.substr(start, end - start + 1);
}

}  // namespace base
}  // namespace web_server

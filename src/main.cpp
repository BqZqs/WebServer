#include <iostream>
#include <string>

#include "base/config.h"
#include "net/web_server.h"

int main() {
  // 1. 实例化配置类并加载配置文件
  web_server::base::Config config;
  
  // 提前设置 Fail-Safe 的默认参数，防止配置文件丢失或读取失败
  int port = 8080;
  int trig_mode = 3;
  int timeout_ms = 60000;
  bool opt_linger = false;
  int sql_port = 3306;
  std::string sql_user = "root";
  std::string sql_pwd = "password";
  std::string db_name = "webserver";
  int conn_pool_num = 12;
  int thread_num = 8;
  bool open_log = true;
  int log_level = 1;
  int log_que_size = 1024;

  if (config.LoadFile("./conf/server.conf")) {
    port = config.GetInt("port", port);
    trig_mode = config.GetInt("trig_mode", trig_mode);
    timeout_ms = config.GetInt("timeout_ms", timeout_ms);
    opt_linger = config.GetInt("opt_linger", opt_linger);
    
    sql_port = config.GetInt("sql_port", sql_port);
    sql_user = config.GetString("sql_user", sql_user);
    sql_pwd = config.GetString("sql_pwd", sql_pwd);
    db_name = config.GetString("db_name", db_name);
    
    conn_pool_num = config.GetInt("conn_pool_num", conn_pool_num);
    thread_num = config.GetInt("thread_num", thread_num);
    
    open_log = config.GetInt("open_log", open_log);
    log_level = config.GetInt("log_level", log_level);
    log_que_size = config.GetInt("log_que_size", log_que_size);
  } else {
    std::cout << "[Warning] ./conf/server.conf load failed! Using default parameters.\n";
  }

  // 2. 实例化 Reactor
  web_server::net::WebServer server(
      port, trig_mode, timeout_ms, opt_linger,
      sql_port, sql_user.c_str(), sql_pwd.c_str(), db_name.c_str(),
      conn_pool_num, thread_num, open_log, log_level, log_que_size);
      
  // 3. 启动死循环监听
  server.Start();

  return 0;
}

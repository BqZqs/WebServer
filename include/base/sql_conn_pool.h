#ifndef WEBSERVER_BASE_SQL_CONN_POOL_H_
#define WEBSERVER_BASE_SQL_CONN_POOL_H_

#include <mysql/mysql.h>
#include <mutex>
#include <queue>
#include <string>
#include <condition_variable>

namespace web_server {
namespace base {

// MySQL 数据库连接池 (单例模式)
class SqlConnPool {
 public:
  static SqlConnPool& Instance() {
    static SqlConnPool instance;
    return instance;
  }

  // 初始化连接池
  void Init(const char* host, int port,
            const char* user, const char* pwd,
            const char* db_name, int conn_size = 10);

  // 获取/释放连接
  MYSQL* GetConn();
  void FreeConn(MYSQL* conn);
  
  // 销毁连接池
  void ClosePool();
  
  // 获取当前空闲连接数
  int GetFreeConnCount();

 private:
  SqlConnPool();
  ~SqlConnPool();
  SqlConnPool(const SqlConnPool&) = delete;
  SqlConnPool& operator=(const SqlConnPool&) = delete;

  std::queue<MYSQL*> conn_que_;    // 存放预先建立的 MySQL 连接
  std::mutex mtx_;                 // 互斥锁，保证线程安全
  std::condition_variable cond_;   // 条件变量，用于阻塞等待连接释放
};

// =========================================================
// 遵循 C++ RAII 规范封装的连接借用器
// 作用：利用栈对象的生命周期，自动归还 MySQL 连接，避免死锁或忘记释放
// =========================================================
class SqlConnRAII {
 public:
  SqlConnRAII(MYSQL** sql, SqlConnPool* conn_pool);
  ~SqlConnRAII();

 private:
  MYSQL* sql_;
  SqlConnPool* conn_pool_;
};

}  // namespace base
}  // namespace web_server

#endif  // WEBSERVER_BASE_SQL_CONN_POOL_H_

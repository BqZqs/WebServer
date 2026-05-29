#include "base/sql_conn_pool.h"

#include <cassert>
#include "base/log.h"

namespace web_server {
namespace base {

SqlConnPool::SqlConnPool() {}

SqlConnPool::~SqlConnPool() {
  ClosePool();
}

void SqlConnPool::Init(const char* host, int port,
                       const char* user, const char* pwd,
                       const char* db_name, int conn_size) {
  assert(conn_size > 0);
  
  for (int i = 0; i < conn_size; ++i) {
    MYSQL* sql = nullptr;
    sql = mysql_init(sql);
    if (!sql) {
      LOG_ERROR("MySQL init failed!");
      assert(sql);
    }
    
    // 建立真实的物理连接
    sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
    if (!sql) {
      LOG_ERROR("MySQL connect failed!");
      assert(sql);
    }
    
    conn_que_.push(sql);
  }
  
  LOG_INFO("SqlConnPool Init Success! Total connections: %d", conn_size);
}

// 工作线程调用：获取一个空闲连接
MYSQL* SqlConnPool::GetConn() {
  std::unique_lock<std::mutex> lock(mtx_);
  
  // 队列为空时，阻塞等待其他线程归还连接
  cond_.wait(lock, [this] { return !conn_que_.empty(); });
  
  MYSQL* sql = conn_que_.front();
  conn_que_.pop();
  return sql;
}

// 归还连接
void SqlConnPool::FreeConn(MYSQL* conn) {
  assert(conn);
  std::lock_guard<std::mutex> lock(mtx_);
  conn_que_.push(conn);
  
  // 唤醒一个可能正在阻塞等待的线程
  cond_.notify_one();
}

int SqlConnPool::GetFreeConnCount() {
  std::lock_guard<std::mutex> lock(mtx_);
  return conn_que_.size();
}

// 安全销毁所有数据库连接
void SqlConnPool::ClosePool() {
  std::lock_guard<std::mutex> lock(mtx_);
  while (!conn_que_.empty()) {
    MYSQL* item = conn_que_.front();
    conn_que_.pop();
    mysql_close(item);
  }
  // 释放 MySQL 客户端库分配的内存
  mysql_library_end(); 
}

// ================== RAII 类的实现 ==================

// 构造时：通过二级指针将真正的连接地址传递给外部变量
SqlConnRAII::SqlConnRAII(MYSQL** sql, SqlConnPool* conn_pool) 
    : sql_(nullptr), conn_pool_(conn_pool) {
  assert(conn_pool);
  *sql = conn_pool_->GetConn();
  sql_ = *sql;
}

// 析构时：自动调用 FreeConn 将连接放回池子
SqlConnRAII::~SqlConnRAII() {
  if (sql_) {
    conn_pool_->FreeConn(sql_);
  }
}

}  // namespace base
}  // namespace web_server

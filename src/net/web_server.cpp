#include "net/web_server.h"

#include <unistd.h>
#include <cassert>
#include <cstring>

#include "base/log.h"

namespace web_server {
	namespace net {

		WebServer::WebServer(
				int port, int trig_mode, int timeout_ms, bool opt_linger,
				int sql_port, const char* sql_user, const char* sql_pwd,
				const char* db_name, int conn_pool_num, int thread_num,
				bool open_log, int log_level, int log_que_size)
			: port_(port), open_linger_(opt_linger), timeout_ms_(timeout_ms), 
			is_close_(false), timer_(new TimerManager()), 
			thread_pool_(new base::ThreadPool(thread_num)), 
			epoll_(new EpollPoller()) {

				// 1. 获取当前工作目录，拼接出真实的静态资源绝对路径
				char* ret = getcwd(src_dir_, 256);
				assert(src_dir_);
				strncat(src_dir_, "/resources/", 16);
				http::HttpConn::user_count = 0;
				http::HttpConn::src_dir = src_dir_;

				// 2. 初始化数据库连接池
				base::SqlConnPool::Instance().Init("localhost", sql_port, sql_user, sql_pwd, 
						db_name, conn_pool_num);

				// 3. 初始化触发模式与监听 Socket
				InitEventMode_(trig_mode);
				if (!InitSocket_()) {
					is_close_ = true;
				}

				// 4. 启动日志系统
				if (open_log) {
					base::Log::Instance().Init(log_level, "./log", ".log", log_que_size);
					if (is_close_) {
						LOG_ERROR("========== Server init error! ==========");
					} else {
						LOG_INFO("========== Server init Success! ==========");
						LOG_INFO("Port:%d, OpenLinger: %s", port_, opt_linger ? "true" : "false");
						LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
								(listen_event_ & EPOLLET ? "ET" : "LT"),
								(conn_event_ & EPOLLET ? "ET" : "LT"));
						LOG_INFO("src_dir: %s", http::HttpConn::src_dir);
						LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", conn_pool_num, thread_num);
					}
				}
			}

		WebServer::~WebServer() {
			close(listen_fd_);
			is_close_ = true;
			base::SqlConnPool::Instance().ClosePool();
		}

		void WebServer::InitEventMode_(int trig_mode) {
			listen_event_ = EPOLLRDHUP;
			conn_event_ = EPOLLONESHOT | EPOLLRDHUP;
			// 灵活配置 ET 和 LT
			switch (trig_mode) {
				case 0: break;
				case 1: conn_event_ |= EPOLLET; break;
				case 2: listen_event_ |= EPOLLET; break;
				case 3: 
						listen_event_ |= EPOLLET;
						conn_event_ |= EPOLLET;
						break;
				default:
						listen_event_ |= EPOLLET;
						conn_event_ |= EPOLLET;
						break;
			}
			http::HttpConn::is_et = (conn_event_ & EPOLLET);
		}

		// 核心主循环：等待事件 -> 分发任务
		void WebServer::Start() {
			int time_ms = -1; // -1 表示无事件时 epoll_wait 永久阻塞
			if (!is_close_) {
				LOG_INFO("========== Server start! ==========");
			}

			while (!is_close_) {
				// 调用 timer_ 获取下一个连接超时的时间，精准控制 epoll 阻塞时长
				if (timeout_ms_ > 0) {
					time_ms = timer_->GetNextTick();
				}

				// 主线程阻塞在这里，等待网卡或定时器事件
				int event_cnt = epoll_->Wait(time_ms);

				for (int i = 0; i < event_cnt; ++i) {
					int fd = epoll_->GetEventFd(i);
					uint32_t events = epoll_->GetEvents(i);

					// 1. 监听到新的客户端接入
					if (fd == listen_fd_) {
						DealListen_();
					} 
					// 2. 客户端异常断开 (RDHUP/HUP/ERR)
					else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
						assert(users_.count(fd) > 0);
						CloseConn_(&users_[fd]);
					} 
					// 3. 已存在的客户端发来了数据报文
					else if (events & EPOLLIN) {
						assert(users_.count(fd) > 0);
						DealRead_(&users_[fd]);
					} 
					// 4. 客户端所在 Socket 可写（通常用于大文件分块发送）
					else if (events & EPOLLOUT) {
						assert(users_.count(fd) > 0);
						DealWrite_(&users_[fd]);
					} else {
						LOG_ERROR("Unexpected event");
					}
				}
			}
		}

		// 接收新的连接
		void WebServer::DealListen_() {
			struct sockaddr_in addr;
			socklen_t len = sizeof(addr);
			do {
				int fd = accept(listen_fd_, (struct sockaddr*)&addr, &len);
				if (fd <= 0) { return; }
				else if (http::HttpConn::user_count >= kMaxFd) {
					// 达到服务器上限，拒绝连接
					SendError_(fd, "Server busy!");
					LOG_WARN("Clients is full!");
					return;
				}
				AddClient_(fd, addr);
			} while (listen_event_ & EPOLLET); // 如果是 ET 模式，必须用循环把队列里的 accept 读干净
		}

		// 封装读事件并抛给线程池
		void WebServer::DealRead_(http::HttpConn* client) {
			assert(client);
			timer_->Adjust(client->GetFd(), timeout_ms_); // 分配新的过期时间
														  // 利用 std::bind 将类的成员函数与具体对象绑定，抛进任务队列
			thread_pool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
		}

		// 封装写事件并抛给线程池
		void WebServer::DealWrite_(http::HttpConn* client) {
			assert(client);
			timer_->Adjust(client->GetFd(), timeout_ms_); // 分配新的过期时间
			thread_pool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
		}

		// Worker 线程实际执行的读逻辑
		void WebServer::OnRead_(http::HttpConn* client) {
			assert(client);
			int ret = -1;
			int read_errno = 0;
			ret = client->Read(&read_errno);
			if (ret <= 0 && read_errno != EAGAIN) {
				CloseConn_(client); // 读错误或对方已断开
				return;
			}
			OnProcess_(client);
		}

		// 业务处理中枢：解析 HTTP -> 生成响应 -> 切换监听事件
		void WebServer::OnProcess_(http::HttpConn* client) {
			if (client->Process()) {
				// 业务解析成功，准备回复数据，将 Epoll 监听状态改为可写 (EPOLLOUT)
				epoll_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
			} else {
				// 报文不完整，继续等待客户端发剩余报文，监听状态保持可读 (EPOLLIN)
				epoll_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);
			}
		}

		// Worker 线程实际执行的写逻辑
		void WebServer::OnWrite_(http::HttpConn* client) {
			assert(client);
			int ret = -1;
			int write_errno = 0;
			ret = client->Write(&write_errno);

			if (client->ToWriteBytes() == 0) {
				// 报文全部发送完毕！
				if (client->IsKeepAlive()) {
					OnProcess_(client); // Keep-Alive 则重置状态，等待下个请求
					return;
				}
			} else if (ret < 0) {
				if (write_errno == EAGAIN) {
					// 没发完但网卡缓冲区满了，继续监听 EPOLLOUT 等下次接着发
					epoll_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
					return;
				}
			}
			CloseConn_(client); // 发送完毕且不是长连接，或者发生严重写入错误，断开
		}

		// 辅助方法：初始化 Socket
		bool WebServer::InitSocket_() {
			int ret;
			struct sockaddr_in addr;
			if (port_ > 65535 || port_ < 1024) {
				LOG_ERROR("Port:%d error!", port_);
				return false;
			}
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_ANY);
			addr.sin_port = htons(port_);

			struct linger opt_linger;
			memset(&opt_linger, 0, sizeof(opt_linger));

			if (open_linger_) {
				//直到缓冲区剩余数据发送完毕或超时才真正断开
				opt_linger.l_onoff = 1;
				opt_linger.l_linger = 1;
			}

			listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
			if (listen_fd_ < 0) {
				LOG_ERROR("Create socket error!");
				return false;
			}

			// 端口复用，防止服务器异常重启时提示 "Address already in use"
			ret = setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
			int optval = 1;
			ret = setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

			ret = bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr));
			if (ret < 0) {
				LOG_ERROR("Bind Port:%d error!", port_);
				close(listen_fd_);
				return false;
			}

			ret = listen(listen_fd_, 6);
			if (ret < 0) {
				LOG_ERROR("Listen port:%d error!", port_);
				close(listen_fd_);
				return false;
			}

			ret = epoll_->AddFd(listen_fd_, listen_event_ | EPOLLIN);
			if (ret == 0) {
				LOG_ERROR("Add listen error!");
				close(listen_fd_);
				return false;
			}
			SetFdNonblock_(listen_fd_);
			return true;
		}

		void WebServer::AddClient_(int fd, sockaddr_in addr) {
			assert(fd > 0);
			users_[fd].Init(fd, addr);
			if (timeout_ms_ > 0) {
				// 将新建连接加入定时器，并绑定超时清理函数回调
				timer_->Add(fd, timeout_ms_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
			}
			epoll_->AddFd(fd, EPOLLIN | conn_event_);
			SetFdNonblock_(fd);
		}

		void WebServer::CloseConn_(http::HttpConn* client) {
			assert(client);
			epoll_->DelFd(client->GetFd());
			client->Close();
		}

		int WebServer::SetFdNonblock_(int fd) {
			assert(fd > 0);
			// fcntl 系统调用，将套接字设置为非阻塞模式 (O_NONBLOCK)
			return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
		}

		void WebServer::SendError_(int fd, const char* info) {
			assert(fd > 0);
			int ret = send(fd, info, strlen(info), 0);
			if (ret < 0) {
				LOG_WARN("send error to client[%d] error!", fd);
			}
			close(fd);
		}

	}  // namespace net
}  // namespace web_server

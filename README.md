WebServer
一个基于 C++11 编写的轻量级、高性能 Web 服务器。

✨ 特性
基于 Epoll 和 线程池 实现的 Reactor 并发模型。

支持 HTTP 的 GET、POST 请求解析，支持 HTTP/1.1 长连接 (Keep-Alive)。

实现 数据库连接池，减少 MySQL 连接开销。

实现 异步日志系统，支持按天自动切分日志。

实现 定时器 (基于小顶堆)，自动清理超时和非活跃的客户端连接。

🛠️ 环境依赖
操作系统: Linux (推荐 Ubuntu)

编译器: 支持 C++11 的 G++ 编译器

数据库: MySQL (sudo apt-get install libmysqlclient-dev)

构建工具: Make

🚀 快速开始
1. 准备数据库
请在本地 MySQL 中创建一个供本项目使用的数据库（默认库名为 webserver），并准备好账号密码。

2. 修改配置
打开配置文件 ./conf/server.conf，填入你的数据库信息及想要监听的端口：

Ini, TOML
port = 8080
sql_user = root
sql_pwd = 你的数据库密码
db_name = webserver
3. 编译与运行
在项目根目录下打开终端，执行以下命令：

Bash
# 编译项目
make

# 启动服务器
./bin/server
启动成功后，打开浏览器访问：http://127.0.0.1:8080 即可看到页面。

📂 核心目录说明
base/ : 基础组件（日志系统、配置加载等）

http/ : HTTP 协议解析与响应组装

net/ : 网络引擎（Epoll 封装、WebServer 主循环）

pool/ : 资源池（线程池、MySQL 连接池）

resources/ : 存放 HTML/CSS/图片 等前端静态文件

cmake_minimum_required(VERSION 3.10)
project(WebServer VERSION 1.0.0 LANGUAGES CXX)

# 设置现代 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 指定可执行文件生成到 bin 目录下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 递归搜集 src 目录下的所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 声明可执行目标
add_executable(${PROJECT_NAME} ${SOURCES})

# 现代 CMake 规范：为指定 Target 设置私有头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 开启严格的编译器警告 (-Wall -Wextra) 以及 O2 级别优化
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O2)

# 查找系统的线程库
find_package(Threads REQUIRED)

# 链接依赖库：多线程 (Threads) 和 MySQL 客户端驱动 (mysqlclient)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Threads::Threads 
    mysqlclient
)

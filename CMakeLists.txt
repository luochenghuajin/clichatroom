# CMakeLists.txt (最终修正版，分离测试目标)

# 1. 项目基本设置 (不变)
cmake_minimum_required(VERSION 3.15)
project(CLIChatRoom_Tests LANGUAGES CXX)
# 在 project(...) 这一行下面
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -pthread")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 2. 核心逻辑库 (不变)
#    我们将所有可复用的业务逻辑和底层工具编译成一个静态库
add_library(chatroom_core
    network.cpp
    services.cpp
    file_io.cpp
)
# ====================================================================
# 产品级可执行文件定义
# ====================================================================

# 1. 构建聊天服务器
#    它使用 server.cpp (里面有 main 函数) 并链接核心库
add_executable(chat_server server.cpp)
target_link_libraries(chat_server PRIVATE chatroom_core pthread)

# 2. 构建聊天客户端
#    它使用 client.cpp (里面应该也有一个 main 函数) 并链接核心库
add_executable(chat_client client.cpp)
target_link_libraries(chat_client PRIVATE chatroom_core pthread)
# ====================================================================
# 测试设置
# ====================================================================
enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# 3. [修正] 为NetworkLayer创建独立的测试程序
#    这个程序只包含 test_network.cpp 和它需要测试的 network.cpp
add_executable(run_network_tests
    tests/test_network.cpp
    network.cpp
)
target_link_libraries(run_network_tests PRIVATE gtest gtest_main pthread)
gtest_discover_tests(run_network_tests)

# 4. [修正] 为Services创建独立的测试程序
#    这个程序包含 test_services.cpp 和它需要测试的 services.cpp
#    关键在于：它不链接 network.cpp，因为它在内部使用了“假的”NetworkLayer替身
add_executable(run_services_tests
    tests/test_services.cpp
    services.cpp
)
target_link_libraries(run_services_tests PRIVATE gtest gtest_main pthread)
gtest_discover_tests(run_services_tests)

# 5. [新增] 为Server主程序逻辑创建独立的测试程序
add_executable(run_server_tests
    tests/test_server.cpp
    server.cpp
)
target_compile_definitions(run_server_tests PRIVATE TEST_BUILD)
# server.cpp 依赖于我们所有的核心服务，所以必须链接 chatroom_core
target_link_libraries(run_server_tests 
    PRIVATE 
    chatroom_core 
    gtest
    gmock
    gtest_main 
    pthread
)

# 6. 为Client主程序逻辑创建独立的测试程序
add_executable(run_client_tests
    tests/main.cpp
    tests/test_client.cpp
    client.cpp
)
target_compile_definitions(run_client_tests PRIVATE TEST_BUILD) # <--- [关键] 定义宏
target_link_libraries(run_client_tests PRIVATE chatroom_core gtest gmock gtest_main pthread)
gtest_discover_tests(run_client_tests)
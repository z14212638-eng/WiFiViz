cmake_minimum_required(VERSION 3.10)
project(NS3ScriptGenerator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NS3_ROOT "" CACHE PATH "Path to ns-3 root directory")
if(NOT NS3_ROOT)
    message(FATAL_ERROR "NS3_ROOT not set. Pass -DNS3_ROOT=/path/to/ns-3")
endif()

# 查找 nlohmann_json 库
find_package(nlohmann_json 3.2.0)

if(nlohmann_json_FOUND)
    # 如果系统安装了 nlohmann_json
    add_executable(ns3-script-generator ns3-script-generator.cc)
    target_link_libraries(ns3-script-generator nlohmann_json::nlohmann_json)
else()
    # 如果没有安装，尝试使用头文件版本
    # 你可以从 https://github.com/nlohmann/json/releases 下载 json.hpp
    # 并放在 include/nlohmann/ 目录下
    message(STATUS "nlohmann_json not found, using header-only version")
    add_executable(ns3-script-generator ns3-script-generator.cc)
    target_include_directories(ns3-script-generator PRIVATE 
        ${NS3_ROOT}/contrib/QNs3/model
        /usr/include
        /usr/local/include
    )
endif()

# 使用 filesystem 库
target_link_libraries(ns3-script-generator stdc++fs)

install(TARGETS ns3-script-generator DESTINATION bin)

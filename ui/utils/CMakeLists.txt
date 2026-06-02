cmake_minimum_required(VERSION 3.10)
project(WiFiVizScriptGenerator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NS3_ROOT "" CACHE PATH "Path to ns-3 root directory")
if(NOT NS3_ROOT)
    message(FATAL_ERROR "NS3_ROOT not set. Pass -DNS3_ROOT=/path/to/ns-3")
endif()

find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(wifiviz-script-generator wifiviz-script-generator.cc)
target_link_libraries(wifiviz-script-generator nlohmann_json::nlohmann_json)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(wifiviz-script-generator stdc++fs)
endif()

install(TARGETS wifiviz-script-generator DESTINATION bin)

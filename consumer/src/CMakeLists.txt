cmake_minimum_required(VERSION 3.16)
project(pact-cpp-consumer VERSION 0.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
find_package(nlohmann_json REQUIRED)
find_package(cpprestsdk REQUIRED)
add_library(pact-cpp-consumer consumer.cpp matchers.cpp)
target_include_directories(pact-cpp-consumer
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        # ✓ 加这一行：让 #include <pact.h> 能找到头文件
        $<BUILD_INTERFACE:${PACT_FFI_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(pact-cpp-consumer
    PUBLIC
        nlohmann_json::nlohmann_json
        cpprestsdk::cpprest
        pact_ffi                    # ✓ 加这一行：链接 pact_ffi
)
# ✓ 但头文件路径需要传递出去
target_include_directories(pact-cpp-consumer
    PUBLIC
        $<BUILD_INTERFACE:${PACT_FFI_DIR}>
)
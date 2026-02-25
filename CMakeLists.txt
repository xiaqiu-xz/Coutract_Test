cmake_minimum_required(VERSION 3.16)
project(ContractTests CXX)
set(CMAKE_CXX_STANDARD 17)

# ── 依赖查找 ──────────────────────────────────────
find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(cpprestsdk REQUIRED)
find_package(nlohmann_json REQUIRED)

# ── pact_ffi 静态库（手动引入）────────────────────
add_library(pact_ffi STATIC IMPORTED)
set_target_properties(pact_ffi PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libpact_ffi/libpact_ffi-linux-x86_64.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/libpact_ffi"
)

# ── pact_cpp_consumer（你项目里的 consumer/ 目录）─
add_library(pact_cpp_consumer
    consumer/src/consumer.cpp
    consumer/src/matchers.cpp
)
target_include_directories(pact_cpp_consumer PUBLIC
    consumer/include
)
target_link_libraries(pact_cpp_consumer PUBLIC
    pact_ffi
    Boost::system
    cpprestsdk::cpprest
    nlohmann_json::nlohmann_json
    ssl crypto dl pthread
)

# ── Consumer 测试 ─────────────────────────────────
add_executable(consumer_test
    consumer_test/consumer_test.cpp
)
target_include_directories(consumer_test PRIVATE
    consumer/include
    src
)
target_link_libraries(consumer_test PRIVATE
    pact_cpp_consumer
    GTest::gtest_main
)

# ── Provider 测试 ─────────────────────────────────
add_executable(provider_test
    provider_test/provider_test.cpp
)
target_include_directories(provider_test PRIVATE
    libpact_ffi
)
target_link_libraries(provider_test PRIVATE
    pact_ffi
    GTest::gtest_main
    ssl crypto dl pthread
)

# ── CTest 注册 ────────────────────────────────────
enable_testing()
add_test(NAME ConsumerTest COMMAND consumer_test)
add_test(NAME ProviderTest COMMAND provider_test)
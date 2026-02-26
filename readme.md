# C++ 消费者驱动契约测试示例
本项目演示如何在 C++ 中使用 [Pact](https://pact.io) 框架实现**消费者驱动契约测试（Consumer-Driven Contract Testing）**，验证 HTTP 客户端（Consumer）与服务端（Provider）之间的接口契约。
## 什么是契约测试
传统集成测试需要同时启动 Consumer 和 Provider，环境复杂、速度慢。契约测试将两端解耦：
```
Consumer 测试                    Provider 测试
─────────────                    ─────────────
TodoClient                       TodoService
    │                                │
    │  发请求                         │
    ▼                                │
Pact Mock Server  ──生成──▶  契约文件  ──验证──▶  真实 Provider
    │                                │
    │  返回约定响应                    │  回放请求，检查响应
    ▼                                ▼
断言客户端行为正确              断言服务端实现符合契约
```
1. **Consumer 先写测试**，定义它期望 Provider 提供什么接口
2. Pact 自动生成**契约文件**（JSON），记录所有交互期望
3. **Provider 拿到契约文件**，回放每条请求验证自己的实现
这样两端可以独立开发、独立测试，契约文件就是唯一的接口协议。
## 项目结构
```
project/
│── .github
│    └── workflows
│           └── contract-test.yml
├── CMakeLists.txt               # 构建配置
├── conanfile.txt                # 第三方依赖声明（Boost、cpprestsdk 等）
│
├── consumer/                    # pact-cplusplus 消费者 DSL 库
│   ├── include/
│   │   ├── consumer.h           # Pact / Interaction / MockServerHandle 等核心类
│   │   └── matchers.h           # 匹配器（type matcher、integer matcher 等）
│   └── src/
│       ├── consumer.cpp         # 调用 pact_ffi C API，实现 Mock Server 启停和契约写入
│       ├── matchers.cpp         # 匹配器的 JSON 序列化实现
│       └── CMakeLists.txt
│
├── consumer_test/
│   └── consumer_test.cpp        # ← 消费者契约测试（本项目核心之一）
│
├── libpact_ffi/                 # Pact 核心引擎（Rust 编写，提供 C ABI）
│   ├── libpact_ffi-linux-x86_64.a   # 从https://github.com/pact-foundation/pact-reference/releases下载对应平台的预编译包 下载的静态库
│   └── pact.h                   # C 头文件，声明所有 pactffi_* 函数
│
├── pacts/
│   └── TodoClient-TodoService.json  # 自动生成的契约文件，Consumer 测试通过后产生
│
├── provider_test/
│   └── provider_test.cpp        # ← 提供者验证测试（本项目核心之二）
│
└── src/
    └── todo_client.hpp          # 被测的 HTTP 客户端（Consumer 的真实业务代码）
```
## 核心文件说明
### `src/todo_client.hpp` — 被测客户端
封装了对 `TodoService` REST API 的调用，是**真正的业务代码**，不依赖任何测试框架：
----
| 方法 | HTTP 请求 | 说明 |
|------|-----------|------|
| `getProjects()` | `GET /projects` | 返回项目列表 |
| `getProject(id)` | `GET /projects/{id}` | 返回单个项目，404 时抛出异常 |
| `createProject(name)` | `POST /projects` | 创建项目，201 时返回 true |
----
### `consumer_test/consumer_test.cpp` — 消费者测试
**目的**：定义 TodoClient 对 TodoService 的期望，生成契约文件。
测试流程：
1. 声明交互期望（请求方法、路径、头、响应状态码、响应体）
2. Pact 启动本地 Mock Server
3. 将 `TodoClient.serverUrl` 指向 Mock Server
4. 调用真实的 `TodoClient` 方法，断言解析行为正确
5. 测试通过后，Pact 自动写入 `pacts/TodoClient-TodoService.json`
覆盖的交互：
----
| 测试名 | 场景 |
|--------|------|
| `GetProjectsList` | 获取所有项目，验证列表解析 |
| `GetSingleProject` | 获取已存在的项目 |
| `GetNonExistentProject` | 项目不存在时客户端抛异常 |
| `CreateProject` | 创建项目，验证返回 true |
----
### `provider_test/provider_test.cpp` — 提供者验证
**目的**：读取契约文件，向真实运行的 Provider 服务回放所有请求，验证响应是否符合契约。
直接调用 `pact_ffi` C API（`pactffi_verifier_*`），不需要消费者 DSL：
```
契约文件
   │
   ▼
pactffi_verifier_execute()
   │  逐条回放 interaction
   ▼
真实 Provider（localhost:8080）
   │  /_pact/state-change  ← 设置数据库前置状态
   │  GET /projects        ← 实际业务请求
   ▼
验证响应状态码、头、Body 是否匹配契约
```
### `libpact_ffi/` — Pact 核心引擎
所有 Pact 语言实现（Ruby/Go/JS/C++ 等）的底层都是这个 Rust 库。
从 [pact-foundation/pact-reference releases](https://github.com/pact-foundation/pact-reference/releases) 下载对应平台的预编译包：
- Linux x86_64：`libpact_ffi-linux-x86_64.a`
- macOS：`libpact_ffi-osx-x86_64.a`
- Windows：`pact_ffi.dll`
## 快速开始
### 环境要求
```bash
sudo apt-get install -y \
    build-essential cmake ninja-build \
    libssl-dev libcurl4-openssl-dev \
    libboost-all-dev \
    nlohmann-json3-dev \
    libcpprest-dev \
    libgtest-dev
```
### 构建
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build -j$(nproc)
```
### 运行消费者测试（生成契约文件）
```bash
mkdir -p pacts
cd build && ./consumer_test
```
成功后契约文件生成在 `pacts/TodoClient-TodoService.json`。
### 运行提供者验证
先启动 Provider 服务（需实现 `/_pact/state-change` 端点）：
```bash
# 用内置的 Python mock 服务快速验证
python3 mock_provider.py &
cd build && ./provider_test
```
## Provider State
Provider State 是契约测试中的**前置条件机制**。
例如测试"获取 id=1 的项目"时，需要确保数据库中存在该记录。消费者在写测试时用 `.given("project with id 1 exists")` 声明这个前提，Provider 验证时会收到：
```http
POST /_pact/state-change
Content-Type: application/json
{"state": "project with id 1 exists", "action": "setup"}
```
Provider 服务需要实现这个端点，根据 state 名称向测试数据库插入/删除对应数据。
## CI/CD 流程
`.github/workflows/contract-test.yml` 定义了两个 Job：
```
push / pull_request
       │
       ▼
┌─────────────────┐
│  consumer-test  │  构建并运行消费者测试 → 生成契约文件
│                 │  → 发布到 Pact Broker（配置了 secrets 才执行）
│                 │  → 上传为 GitHub Artifact
└────────┬────────┘
         │ needs
         ▼
┌─────────────────┐
│ provider-verify │  下载契约文件
│                 │  → 启动 mock Provider 服务
│                 │  → 构建并运行提供者验证测试
│                 │  → Can I Deploy 检查（配置了 secrets 才执行）
└─────────────────┘
```
如需接入 [Pact Broker](https://docs.pact.io/pact_broker)，在 GitHub 仓库 Settings → Secrets 中添加：
- `PACT_BROKER_URL`
- `PACT_BROKER_TOKEN`
## 关键依赖版本
----
| 依赖 | 版本 | 用途 |
|------|------|------|
| pact_ffi | 0.5.3 | Pact 核心引擎 |
| pact-cplusplus consumer DSL | master | Consumer 测试 DSL |
| cpprestsdk | 2.10.19 | HTTP 客户端 |
| nlohmann/json | 3.11.x | JSON 序列化 |
| GTest | 1.14.0 | 测试框架 |
| Boost | 1.83.0 | cpprestsdk 依赖 |
----
### 依赖配置
```ini
# conanfile.txt
[requires]
boost/1.82.0
cpprestsdk/2.10.19
nlohmann_json/3.11.2
gtest/1.13.0
[generators]
cmake
```
```cmake
# CMakeLists.txt
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
```
### 被测的 HTTP 客户端
```cpp
// src/todo_client.hpp
#pragma once
#include <cpprest/http_client.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
struct Project {
    int id;
    std::string name;
    bool completed;
};
class TodoClient {
public:
    std::string serverUrl;
    // 获取所有项目列表
    std::vector<Project> getProjects() {
    web::http::client::http_client client(serverUrl);
    web::http::http_request req(web::http::methods::GET);
    req.set_request_uri(U("/projects"));
    req.headers().add(U("Accept"), U("application/json"));  // ← 加这行
    auto response = client.request(req).get();
    auto body = response.extract_json().get();
    std::vector<Project> projects;
    for (auto& item : body.as_array()) {
        Project p;
        p.id        = item.at(U("id")).as_integer();
        p.name      = item.at(U("name")).as_string();
        p.completed = item.at(U("completed")).as_bool();
        projects.push_back(p);
    }
    return projects;
}
    // 根据 ID 获取单个项目
    Project getProject(int projectId) {
        web::http::client::http_client client(serverUrl);
        auto path = utility::string_t(U("/projects/")) + utility::conversions::to_string_t(std::to_string(projectId));
        auto response = client.request(web::http::methods::GET, path).get();
        if (response.status_code() == 404) {
            throw std::runtime_error("Project not found");
        }
        auto body = response.extract_json().get();
        Project p;
        p.id = body.at(U("id")).as_integer();
        p.name = body.at(U("name")).as_string();
        p.completed = body.at(U("completed")).as_bool();
        return p;
    }
    // 创建新项目
    bool createProject(const std::string& name) {
        web::http::client::http_client client(serverUrl);
        nlohmann::json payload = {{"name", name}, {"completed", false}};
        web::http::http_request req(web::http::methods::POST);
        req.set_request_uri(U("/projects"));
        req.set_body(payload.dump(), web::http::details::mime_types::application_json);
        auto response = client.request(req).get();
        return response.status_code() == 201;
    }
};
```
### 消费者契约测试（生成 Contract）
```cpp
#include <gtest/gtest.h>
#include <consumer.h>
#include "../src/todo_client.hpp"
class TodoConsumerTest : public ::testing::Test {
protected:
    pact_consumer::Pact pact{"TodoClient", "TodoService"};
    void SetUp() override {
        // 指定契约文件输出目录（相对于可执行文件位置，即 build/ 的上一层）
        pact.pact_directory = "../pacts";
    }
};
TEST_F(TodoConsumerTest, GetProjectsList) {
    pact.uponReceiving("a request to get all projects")
        .withRequest("GET", "/projects")
        // .withHeaders({{"Accept", {"application/json"}}})
        .willRespondWith(200)
        .withResponseHeaders({{"Content-Type", {"application/json"}}})
        .withResponseBody(R"([
             {
                "id":        1,
                "name":      "Project Alpha",
                "completed": false
            }
        ])", "application/json");  // ← 加第二个参数
    auto result = pact.run_test([](const pact_consumer::MockServerHandle* mock_server) {
        TodoClient client;
        client.serverUrl = mock_server->get_url();
        auto projects = client.getProjects();
        EXPECT_FALSE(projects.empty());
        EXPECT_EQ(projects[0].id, 1);
        EXPECT_FALSE(projects[0].name.empty());
        return true;
    });
    EXPECT_TRUE(result.is_ok());  // ← 去掉 get_error()，is_ok() 失败时 pact_ffi 会打印到 stderr
}
TEST_F(TodoConsumerTest, GetSingleProject) {
    pact.uponReceiving("a request to get project with id 1")
        .given("project with id 1 exists")
        .withRequest("GET", "/projects/1")
        .willRespondWith(200)
        .withResponseBody(R"({
            "id":        1,
            "name":      "Project Alpha",
            "completed": false
        })", "application/json");  // ← 加第二个参数
    auto result = pact.run_test([](const pact_consumer::MockServerHandle* mock_server) {
        TodoClient client;
        client.serverUrl = mock_server->get_url();
        auto project = client.getProject(1);
        EXPECT_EQ(project.id, 1);
        EXPECT_EQ(project.name, "Project Alpha");
        EXPECT_FALSE(project.completed);
        return true;
    });
    EXPECT_TRUE(result.is_ok());
}
TEST_F(TodoConsumerTest, GetNonExistentProject) {
    pact.uponReceiving("a request to get a non-existent project")
        .given("project with id 999 does not exist")
        .withRequest("GET", "/projects/999")
        .willRespondWith(404)
        .withResponseBody(
            R"({"error": "Project not found"})",
            "application/json");  // ← 加第二个参数
    auto result = pact.run_test([](const pact_consumer::MockServerHandle* mock_server) {
        TodoClient client;
        client.serverUrl = mock_server->get_url();
        EXPECT_THROW(client.getProject(999), std::runtime_error);
        return true;
    });
    EXPECT_TRUE(result.is_ok());
}
TEST_F(TodoConsumerTest, CreateProject) {
    pact.uponReceiving("a request to create a new project")
        .withRequest("POST", "/projects")
        .withHeaders({{"Content-Type", {"application/json"}}})
        .withBody(R"({
            "name":      { "pact:matcher:type": "type", "value": "New Project" },
            "completed": false
        })", "application/json")  // ← 加第二个参数
        .willRespondWith(201);
    auto result = pact.run_test([](const pact_consumer::MockServerHandle* mock_server) {
        TodoClient client;
        client.serverUrl = mock_server->get_url();
        bool created = client.createProject("New Project");
        EXPECT_TRUE(created);
        return true;
    });
    EXPECT_TRUE(result.is_ok());
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```
### 生成的契约文件（JSON）
运行消费者测试后，自动生成 `./pacts/TodoClient-TodoService.json`：
```json
{
  "consumer": { "name": "TodoClient" },
  "provider": { "name": "TodoService" },
  "interactions": [
    {
      "description": "a request to get all projects",
      "request": {
        "method": "GET",
        "path": "/projects",
        "headers": { "Accept": "application/json" }
      },
      "response": {
        "status": 200,
        "headers": { "Content-Type": "application/json" },
        "body": [
          { "id": 1, "name": "Project Alpha", "completed": false }
        ],
        "matchingRules": {
          "body": {
            "$[0].id":        { "matchers": [{ "match": "integer" }] },
            "$[0].name":      { "matchers": [{ "match": "type" }] },
            "$[0].completed": { "matchers": [{ "match": "type" }] }
          }
        }
      }
    },
    {
      "description": "a request to get a non-existent project",
      "providerStates": [{ "name": "project with id 999 does not exist" }],
      "request": { "method": "GET", "path": "/projects/999" },
      "response": {
        "status": 404,
        "body": { "error": "Project not found" }
      }
    }
  ],
  "metadata": {
    "pactSpecification": { "version": "3.0.0" },
    "pact-cpp": { "version": "0.1.4" }
  }
}
```
### 提供者验证测试
```cpp
// provider_test/provider_test.cpp
#include <gtest/gtest.h>
extern "C"{
#include <pact.h>           // 来自 libpact_ffi/pact.h
}
#include <string>
#include <cstring>
#include <cstdio>
// Provider State 回调（C 函数签名匹配 pact_ffi 要求）
static void state_change_handler(const char* state, const char* action) {
    if (!state) return;
    std::string s(state);
    // action 是 "setup" 或 "teardown"
    if (std::string(action ? action : "") != "setup") return;
    if (s == "project with id 1 exists") {
        // db.insert({1, "Project Alpha", false});
    } else if (s == "project with id 999 does not exist") {
        // db.delete_if_exists(999);
    }
}
TEST(TodoProviderTest, VerifyConsumerContracts) {
    VerifierHandle* handle = pactffi_verifier_new_for_application("provider_test", "1.0.0");
    pactffi_verifier_set_provider_info(handle, "TodoService", "http", "localhost", 8080, "/");
    // 自动找 pacts 目录：先找 ./pacts，再找 ../pacts
    std::string pact_path;
    FILE* f = fopen("./pacts/TodoClient-TodoService.json", "r");
    if (f) {
        fclose(f);
        pact_path = "./pacts/TodoClient-TodoService.json";
    } else {
        pact_path = "../pacts/TodoClient-TodoService.json";
    }
    pactffi_verifier_add_file_source(handle, pact_path.c_str());
    pactffi_verifier_set_provider_state(
        handle,
        "http://localhost:8080/_pact/state-change",
        1, 1
    );
    int result = pactffi_verifier_execute(handle);
    if (result != 0) {
        ADD_FAILURE() << "Provider 验证失败，请查看上方输出";
    }
    pactffi_verifier_shutdown(handle);
    EXPECT_EQ(result, 0);
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```
### CI/CD 集成流程
```yaml
name: Contract Tests
on: [push, pull_request]
jobs:
  consumer-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install system dependencies
        run: |
          sudo apt-get update -q
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            libssl-dev libcurl4-openssl-dev \
            libboost-all-dev \
            nlohmann-json3-dev \
            libcpprest-dev \
            libgtest-dev \
            curl git
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
      - name: Build consumer tests
        run: cmake --build build --target consumer_test -j$(nproc)
      - name: Run consumer tests (generates pact files)
        run: |
          mkdir -p pacts
          cd build && ./consumer_test
          echo "── 生成的契约文件 ──"
          ls -lh ../pacts/
      - name: Publish contracts to Pact Broker
        if: success() && env.PACT_BROKER_URL != ''   # ← 改用 env
        env:
          PACT_BROKER_URL: ${{ secrets.PACT_BROKER_URL }}       # ← secret 映射到 env
          PACT_BROKER_BASE_URL: ${{ secrets.PACT_BROKER_URL }}
          PACT_BROKER_TOKEN: ${{ secrets.PACT_BROKER_TOKEN }}
        uses: pactflow/actions/publish-pact-files@v1.0.1
        with:
          pactfiles: pacts
          version: ${{ github.sha }}
          tags: ${{ github.ref_name }}
      - name: Upload pact contracts artifact
        uses: actions/upload-artifact@v4
        with:
          name: pact-contracts
          path: ./pacts/*.json
          retention-days: 7
  provider-verify:
    needs: consumer-test
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install system dependencies
        run: |
          sudo apt-get update -q
          sudo apt-get install -y \
            build-essential cmake ninja-build \
            libssl-dev libcurl4-openssl-dev \
            libboost-all-dev \
            nlohmann-json3-dev \
            libcpprest-dev \
            libgtest-dev \
            curl git
      - name: Download pact contracts
        uses: actions/download-artifact@v4
        with:
          name: pact-contracts
          path: ./pacts
      - name: Start provider service
        run: |
          if [ -f docker-compose.yml ]; then
            docker compose up -d todo-service
            timeout 60 bash -c \
              'until curl -sf http://localhost:8080/health; do sleep 2; done'
            echo "Provider 服务已就绪"
          else
            echo "  无 docker-compose.yml，跳过 Provider 启动"
          fi
      - name: Configure CMake
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
      - name: Build provider tests
        run: cmake --build build --target provider_test -j$(nproc)
      - name: Start mock provider service
        run: |
          python3 << 'EOF' &
          import json
          from http.server import HTTPServer, BaseHTTPRequestHandler
          class Handler(BaseHTTPRequestHandler):
              def log_message(self, fmt, *args): pass
              def do_GET(self):
                  if self.path == '/projects':
                      body = json.dumps([{"id":1,"name":"Project Alpha","completed":False}]).encode()
                      self.send_response(200)
                  elif self.path == '/projects/1':
                      body = json.dumps({"id":1,"name":"Project Alpha","completed":False}).encode()
                      self.send_response(200)
                  elif self.path == '/projects/999':
                      body = json.dumps({"error":"Project not found"}).encode()
                      self.send_response(404)
                  else:
                      body = b'{}'
                      self.send_response(404)
                  self.send_header('Content-Type','application/json')
                  self.end_headers()
                  self.wfile.write(body)
              def do_POST(self):
                  length = int(self.headers.get('Content-Length',0))
                  self.rfile.read(length)
                  if self.path == '/_pact/state-change':
                      self.send_response(200)
                      self.send_header('Content-Type','application/json')
                      self.end_headers()
                      self.wfile.write(b'{}')
                  elif self.path == '/projects':
                      self.send_response(201)
                      self.send_header('Content-Type','application/json')
                      self.end_headers()
                      self.wfile.write(b'{}')
                  else:
                      self.send_response(404)
                      self.end_headers()
          HTTPServer(('',8080),Handler).serve_forever()
          EOF
          sleep 1
          curl -sf http://localhost:8080/projects && echo "Mock provider ready"
      - name: Run provider tests
        run: ./build/provider_test
      - name: Can I Deploy
        if: always() && env.PACT_BROKER_URL != ''    # ← 改用 env
        env:
          PACT_BROKER_URL: ${{ secrets.PACT_BROKER_URL }}       # ← secret 映射到 env
          PACT_BROKER_BASE_URL: ${{ secrets.PACT_BROKER_URL }}
          PACT_BROKER_TOKEN: ${{ secrets.PACT_BROKER_TOKEN }}
        uses: pactflow/actions/can-i-deploy@v1.0.1
        with:
          pacticipant: TodoService
          version: ${{ github.sha }}
          to-environment: production
```
## 四、开源框架对比
Pact 是使用 CDC 时的事实标准，Spring Cloud Contract 是 Spring 提供的 CDC 测试实现，在 Spring 生态系统中易于集成。
----
| 框架 | 主导方 | 语言支持 | 测试方向 | 适用场景 |
|------|--------|---------|---------|---------|
| **Pact** | Pact Foundation | C++/Java/JS/Go/Python/Ruby/.NET/Rust | Consumer 驱动 | 微服务、多语言异构系统 |
| **Spring Cloud Contract** | Spring | Java/Groovy/YAML | Provider 驱动 | Spring Boot 项目 |
| **Karate** | Intuit | Java（DSL） | 双向 | Java 项目、BDD 风格 |
| **Dredd** | Apiary | 语言无关 | Provider 驱动 | OpenAPI/Swagger 规范验证 |
| **Hoverfly** | SpectoLabs | 语言无关 | Provider 驱动 | 轻量级 HTTP 服务模拟 |
----
### Pact 生态架构
```
┌──────────────────────────────────────────────────────┐
│                   Pact 生态系统                        │
│                                                        │
│  语言 DSL 层                                           │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐       │
│  │ C++  │ │ Java │ │  JS  │ │  Go  │ │ .NET │  ...  │
│  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘       │
│     └────────┴────────┴────────┴────────┘             │
│                        │                               │
│              ┌─────────▼─────────┐                    │
│              │   pact_ffi (Rust) │  ← 核心库          │
│              │   统一 FFI 接口    │    各语言都调它     │
│              └─────────┬─────────┘                    │
│                        │                               │
│              ┌─────────▼─────────┐                    │
│              │   Pact Broker     │  ← 契约仓库         │
│              │  （开源 / 自托管） │    存储/版本/分析   │
│              └───────────────────┘                    │
└──────────────────────────────────────────────────────┘
```
Pact 是一个代码优先的消费者驱动契约测试工具，契约在消费者自动化测试执行期间生成。一个主要优势是只有消费者实际使用的通信部分才会被测试，这意味着当前消费者未使用的任何提供者行为都可以自由更改而不会破坏测试。
# 本地编译依赖
sudo apt install nlohmann-json3-dev  
sudo apt install libcpprest-dev  
sudo apt install libgtest-dev  
libgmock-dev 
https://github.com/pact-foundation/pact-reference/releases 
头文件和库文件

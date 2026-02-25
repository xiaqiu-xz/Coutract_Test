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
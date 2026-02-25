// provider_test/provider_test.cpp
#include <gtest/gtest.h>
extern "C"{
#include <pact.h>           // 来自 libpact_ffi/pact.h
}
#include <string>
#include <cstring>

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
    // 1. 创建 verifier handle
    VerifierHandle* handle = pactffi_verifier_new_for_application("provider_test", "1.0.0");
    ASSERT_NE(handle, nullptr);

    // 2. 设置 Provider 信息
    pactffi_verifier_set_provider_info(
        handle,
        "TodoService",   // provider name
        "http",          // scheme
        "localhost",     // host
        8080,            // port
        "/"              // base path
    );

    // 3. 添加契约文件来源
    pactffi_verifier_add_file_source(handle, "../pacts/TodoClient-TodoService.json");

    // 4. 设置 Provider State 变更端点
    //    pact_ffi 通过 HTTP 回调来触发 provider state
    //    最简单做法：启动一个内嵌 state-change 服务，或用下面的 handler 注册
    pactffi_verifier_set_provider_state(
        handle,
        "http://localhost:8080/_pact/state-change",  // Provider 需实现此端点
        1,   // teardown: true
        1    // body: true (state 通过 POST body 传递)
    );

    // 5. 执行验证
    int result = pactffi_verifier_execute(handle);

    // 6. 打印错误信息（如果失败）
    if (result != 0) {
        // pact_ffi 会将结果打印到 stdout/stderr，也可通过 logs 获取
        ADD_FAILURE() << "Provider 验证失败，请查看上方输出";
    }

    // 7. 清理
    pactffi_verifier_shutdown(handle);

    EXPECT_EQ(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
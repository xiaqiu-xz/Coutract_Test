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
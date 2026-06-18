#include "error_system/core/result.h"

#include <gtest/gtest.h>

#include "error_system/core/error_registry.h"

namespace error_system::core {

    class result_test_t : public ::testing::Test {
    protected:
        void SetUp() override { error_registry_t::instance().unregister_all(); }

        void TearDown() override { error_registry_t::instance().unregister_all(); }
    };

    TEST_F(result_test_t, success_result_with_value) {
        result_t<int> result(42);
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_test_t, error_result_with_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Test error 1");

        error_context_t context(code, "test error");
        result_t<int> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test_t, error_result_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 100);
        error_registry_t::instance().register_error(code, "ERR_100", "Error 100");

        error_context_t context(code, "error message");
        result_t<int> result(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test_t, move_constructor_transfers_value) {
        result_t<std::string> result(std::string("moved value"));
        EXPECT_EQ(result.value(), "moved value");
    }

    TEST_F(result_test_t, and_then_chains_success) {
        result_t<int> result(5);
        auto new_result = std::move(result).and_then([](int val) -> result_t<int> { return result_t<int>(val * 2); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 10);
    }

    TEST_F(result_test_t, and_then_preserves_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<int> result(context);
        bool called = false;
        auto new_result = std::move(result).and_then([&called](int val) -> result_t<int> {
            called = true;
            return result_t<int>(val * 2);
        });
        EXPECT_TRUE(new_result.is_error());
        EXPECT_FALSE(called);
    }

    TEST_F(result_test_t, or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<int> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(42); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 42);
    }

    TEST_F(result_test_t, or_else_preserves_success) {
        result_t<int> result(100);
        bool called = false;
        auto new_result = std::move(result).or_else([&called](const error_context_t&) -> result_t<int> {
            called = true;
            return result_t<int>(42);
        });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 100);
        EXPECT_FALSE(called);
    }

    TEST_F(result_test_t, void_success_result) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
    }

    TEST_F(result_test_t, void_error_result) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<void> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
    }

    TEST_F(result_test_t, void_and_then_chains_success) {
        result_t<void> result;
        bool called = false;
        auto new_result = std::move(result).and_then([&called]() -> result_t<void> {
            called = true;
            return result_t<void>();
        });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_TRUE(called);
    }

    TEST_F(result_test_t, void_or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<void> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<void> { return result_t<void>(); });
        EXPECT_TRUE(new_result.is_success());
    }

    TEST_F(result_test_t, lvalue_and_then_works) {
        result_t<int> result(5);
        auto new_result = result.and_then([](int val) -> result_t<int> { return result_t<int>(val * 3); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 15);
    }

    TEST_F(result_test_t, lvalue_or_else_works) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<int> result(context);
        auto new_result = result.or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(99); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 99);
    }

    TEST_F(result_test_t, make_error_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code, "factory error");
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "factory error");
    }

    TEST_F(result_test_t, make_error_with_code_only) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "");
    }

    TEST_F(result_test_t, make_error_from_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "from context");
        auto result = result_t<int>::make_error(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "from context");
    }

    // ========== expect() 测试 ==========

    TEST_F(result_test_t, expect_on_success_returns_value) {
        result_t<int> result(42);
        EXPECT_EQ(result.expect(), 42);
    }

    TEST_F(result_test_t, void_expect_on_success_no_op) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        result.expect();
        EXPECT_TRUE(result.is_success());
    }

    TEST_F(result_test_t, expect_with_string_value) {
        result_t<std::string> result(std::string("hello"));
        EXPECT_EQ(result.expect(), "hello");
    }

    // ========== value_pointer() 测试 ==========

    TEST_F(result_test_t, value_pointer_on_success_returns_non_null) {
        auto result = result_t<int>(99);
        const int* ptr = result.value_pointer();
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(*ptr, 99);
    }

    TEST_F(result_test_t, value_pointer_on_error_returns_null) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_VP", "value_pointer error");
        auto result = result_t<int>::make_error(code, "fail");
        const int* ptr = result.value_pointer();
        EXPECT_EQ(ptr, nullptr);
    }

    // ========== value_or() 测试 ==========

    TEST_F(result_test_t, value_or_on_success_returns_value) {
        auto result = result_t<int>(42);
        EXPECT_EQ(result.value_or(100), 42);
    }

    TEST_F(result_test_t, value_or_on_error_returns_default) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_VO", "value_or error");
        auto result = result_t<int>::make_error(code, "fail");
        EXPECT_EQ(result.value_or(100), 100);
    }

    // ========== map() 测试 ==========

    TEST_F(result_test_t, map_on_success_transforms_value) {
        auto result = result_t<int>(21);
        auto mapped = result.map([](int v) noexcept { return v * 2; });
        EXPECT_TRUE(mapped.is_success());
        EXPECT_EQ(mapped.value_or(0), 42);
    }

    TEST_F(result_test_t, map_on_error_propagates_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_MAP", "map error");
        auto result = result_t<int>::make_error(code, "fail");
        auto mapped = result.map([](int v) noexcept { return v * 2; });
        EXPECT_TRUE(mapped.is_error());
        EXPECT_EQ(mapped.value_or(0), 0);
    }

    // ========== map_error() 测试 ==========

    TEST_F(result_test_t, map_error_on_success_preserves_value) {
        auto result = result_t<int>(42);
        auto mapped = result.map_error([](const error_context_t&) noexcept {
            auto code = error_code_t{error_level_t::fatal, domain::system_domain_t::none, 0x0001, 0x0001, 99};
            error_registry_t::instance().register_error(code, "ERR_ME99", "map_error 99");
            return error_context_t{code, "unused"};
        });
        EXPECT_TRUE(mapped.is_success());
        EXPECT_EQ(mapped.value_or(0), 42);
    }

    TEST_F(result_test_t, map_error_on_error_transforms_error) {
        auto original_code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(original_code, "ERR_ME1", "map_error 1");
        auto result = result_t<int>::make_error(original_code, "original");
        auto mapped = result.map_error([](const error_context_t&) noexcept {
            auto code = error_code_t{error_level_t::fatal, domain::system_domain_t::none, 0x0001, 0x0001, 99};
            error_registry_t::instance().register_error(code, "ERR_ME99", "map_error 99");
            return error_context_t{code, "transformed"};
        });
        EXPECT_TRUE(mapped.is_error());
        // 错误码应被替换
        EXPECT_EQ(mapped.error().get_code().get_number(), 99u);
    }

    // ========== result_t<void> 测试 ==========

    TEST_F(result_test_t, void_result_success) {
        auto result = result_t<void>();
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
    }

    TEST_F(result_test_t, void_result_make_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_VOID", "void error");
        auto result = result_t<void>::make_error(code, "void error");
        EXPECT_TRUE(result.is_error());
        EXPECT_FALSE(result.is_success());
    }

    // ========== 拷贝构造测试 ==========

    TEST_F(result_test_t, copy_constructor_preserves_success) {
        auto original = result_t<int>(42);
        auto copy = original;
        EXPECT_TRUE(copy.is_success());
        EXPECT_EQ(copy.value_or(0), 42);
    }

    TEST_F(result_test_t, copy_constructor_preserves_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_COPY", "copy error");
        auto original = result_t<int>::make_error(code, "copy me");
        auto copy = original;
        EXPECT_TRUE(copy.is_error());
        EXPECT_EQ(copy.value_or(0), 0);
    }

    // ========== 移动构造测试 ==========

    TEST_F(result_test_t, move_constructor_preserves_success) {
        auto original = result_t<int>(42);
        result_t<int> moved(std::move(original));
        EXPECT_TRUE(moved.is_success());
        EXPECT_EQ(moved.value_or(0), 42);
    }

    TEST_F(result_test_t, move_constructor_preserves_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1};
        error_registry_t::instance().register_error(code, "ERR_MOVE", "move error");
        auto original = result_t<int>::make_error(code, "move me");
        result_t<int> moved(std::move(original));
        EXPECT_TRUE(moved.is_error());
    }

    // ========== make_success() 测试 ==========

    TEST_F(result_test_t, make_success_creates_success_result) {
        auto result = result_t<int>::make_success(42);
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value_or(0), 42);
    }

    TEST_F(result_test_t, make_success_string_creates_success_result) {
        auto result = result_t<std::string>::make_success("hello");
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value_or(""), "hello");
    }

    // ========== unwrap() 测试 ==========

    TEST_F(result_test_t, unwrap_on_success_returns_value) {
        auto result = result_t<int>::make_success(42);
        EXPECT_EQ(result.unwrap(), 42);
    }

    TEST_F(result_test_t, unwrap_on_error_returns_default) {
        auto result = result_t<int>::make_error(
            error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1}, "fail");
        EXPECT_EQ(result.unwrap(), 0);
    }

    // ========== match() 测试 ==========

    TEST_F(result_test_t, match_on_success_calls_success_fn) {
        auto result = result_t<int>::make_success(42);
        auto output = result.match(
            [](int v) noexcept { return std::string("success: ") + std::to_string(v); },
            [](const error_context_t&) noexcept { return std::string("error"); }
        );
        EXPECT_EQ(output, "success: 42");
    }

    TEST_F(result_test_t, match_on_error_calls_error_fn) {
        auto result = result_t<int>::make_error(
            error_code_t{error_level_t::error, domain::system_domain_t::none, 0x0001, 0x0001, 1}, "fail");
        auto output = result.match(
            [](int) noexcept { return std::string("success"); },
            [](const error_context_t&) noexcept { return std::string("error"); }
        );
        EXPECT_EQ(output, "error");
    }

}  // namespace error_system::core
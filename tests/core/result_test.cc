#include "error_system/core/error_registry.h"
#include "error_system/core/result.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class result_test : public ::testing::Test {
        protected:
        void SetUp() override { error_registry_t::instance().unregister_all(); }

        void TearDown() override { error_registry_t::instance().unregister_all(); }
    };

    TEST_F(result_test, success_result_with_value) {
        result_t<int> result(42);
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_test, error_result_with_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Test error 1");

        error_context_t context(code, "test error");
        result_t<int> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test, error_result_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 100);
        error_registry_t::instance().register_error(code, "ERR_100", "Error 100");

        error_context_t context(code, "error message");
        result_t<int> result(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test, move_constructor_transfers_value) {
        result_t<std::string> result(std::string("moved value"));
        EXPECT_EQ(result.value(), "moved value");
    }

    TEST_F(result_test, and_then_chains_success) {
        result_t<int> result(5);
        auto new_result = std::move(result).and_then([](int val) -> result_t<int> { return result_t<int>(val * 2); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 10);
    }

    TEST_F(result_test, and_then_preserves_error) {
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

    TEST_F(result_test, or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<int> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(42); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 42);
    }

    TEST_F(result_test, or_else_preserves_success) {
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

    TEST_F(result_test, void_success_result) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
    }

    TEST_F(result_test, void_error_result) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<void> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
    }

    TEST_F(result_test, void_and_then_chains_success) {
        result_t<void> result;
        bool called = false;
        auto new_result = std::move(result).and_then([&called]() -> result_t<void> {
            called = true;
            return result_t<void>();
        });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_TRUE(called);
    }

    TEST_F(result_test, void_or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<void> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<void> { return result_t<void>(); });
        EXPECT_TRUE(new_result.is_success());
    }

    TEST_F(result_test, lvalue_and_then_works) {
        result_t<int> result(5);
        auto new_result = result.and_then([](int val) -> result_t<int> { return result_t<int>(val * 3); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 15);
    }

    TEST_F(result_test, lvalue_or_else_works) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        result_t<int> result(context);
        auto new_result = result.or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(99); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 99);
    }

    TEST_F(result_test, make_error_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code, "factory error");
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "factory error");
    }

    TEST_F(result_test, make_error_with_code_only) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "");
    }

    TEST_F(result_test, make_error_from_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "from context");
        auto result = result_t<int>::make_error(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "from context");
    }

    // ========== expect() 测试 ==========

    TEST_F(result_test, expect_on_success_returns_value) {
        result_t<int> result(42);
        EXPECT_EQ(result.expect(), 42);
    }

    TEST_F(result_test, void_expect_on_success_no_op) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        result.expect();
        EXPECT_TRUE(result.is_success());
    }

    TEST_F(result_test, expect_with_string_value) {
        result_t<std::string> result(std::string("hello"));
        EXPECT_EQ(result.expect(), "hello");
    }

    }  // namespace error_system::core
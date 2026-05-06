#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_config.h"
#include "error_system/core/error_level.h"
#include "error_system/core/result.h"
#include "error_system/domain/system_domain.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class result_t_test : public ::testing::Test {
        protected:
        void SetUp() override { error_config_t::set_enable_validation(false); }
    };

    TEST_F(result_t_test, construct_with_value) {
        result_t<int> result(42);
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_t_test, construct_with_value_move) {
        std::string str = "hello";
        result_t<std::string> result(std::move(str));
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value(), "hello");
    }

    TEST_F(result_t_test, construct_with_error_context) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 2, 100);
        error_context_t ctx(code, "test error");
        result_t<int> result(ctx);

        EXPECT_TRUE(result.is_error());
        EXPECT_FALSE(result.is_success());
        EXPECT_EQ(result.error().code.get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "test error");
    }

    TEST_F(result_t_test, construct_with_code_and_message) {
        auto code = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 0, 0, 50);
        result_t<int> result(code, "database warning");

        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().code.get_number(), 50);
        EXPECT_EQ(result.error().message, "database warning");
    }

    TEST_F(result_t_test, construct_void_success) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
    }

    TEST_F(result_t_test, construct_void_with_error_context) {
        auto code = error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::kernel, 0, 0, 1);
        error_context_t ctx(code, "fatal kernel error");
        result_t<void> result(ctx);

        EXPECT_TRUE(result.is_error());
        EXPECT_FALSE(result.is_success());
        EXPECT_EQ(result.error().message, "fatal kernel error");
    }

    TEST_F(result_t_test, construct_void_with_code_and_message) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::network, 0, 0, 404);
        result_t<void> result(code, "not found");

        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().code.get_number(), 404);
    }

    TEST_F(result_t_test, value_mutable_access) {
        result_t<int> result(10);
        result.value() = 20;
        EXPECT_EQ(result.value(), 20);
    }

    TEST_F(result_t_test, complex_value_type) {
        result_t<std::vector<int>> result(std::vector<int>{1, 2, 3});
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value().size(), 3);
        EXPECT_EQ(result.value()[0], 1);
    }

    TEST_F(result_t_test, error_context_with_cause) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::middleware, 0, 0, 2);

        error_context_t cause(code1, "root cause");
        error_context_t ctx(code2, "wrapped error");
        ctx = ctx.wrap(cause);

        result_t<int> result(ctx);
        EXPECT_TRUE(result.is_error());
        EXPECT_NE(result.error().cause, nullptr);
        EXPECT_EQ(result.error().cause->message, "root cause");
    }

    TEST_F(result_t_test, and_then_success_lvalue) {
        result_t<int> result(42);
        auto next = result.and_then([](int value) -> result_t<double> { return static_cast<double>(value) / 2.0; });

        EXPECT_TRUE(next.is_success());
        EXPECT_DOUBLE_EQ(next.value(), 21.0);
    }

    TEST_F(result_t_test, and_then_error_lvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "original error");
        auto next = result.and_then([](int value) -> result_t<double> { return static_cast<double>(value) / 2.0; });

        EXPECT_TRUE(next.is_error());
        EXPECT_EQ(next.error().message, "original error");
    }

    TEST_F(result_t_test, and_then_success_rvalue) {
        result_t<int> result(42);
        auto next =
            std::move(result).and_then([](int value) -> result_t<double> { return static_cast<double>(value) / 2.0; });

        EXPECT_TRUE(next.is_success());
        EXPECT_DOUBLE_EQ(next.value(), 21.0);
    }

    TEST_F(result_t_test, and_then_error_rvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "original error");
        auto next =
            std::move(result).and_then([](int value) -> result_t<double> { return static_cast<double>(value) / 2.0; });

        EXPECT_TRUE(next.is_error());
        EXPECT_EQ(next.error().message, "original error");
    }

    TEST_F(result_t_test, and_then_chained) {
        result_t<int> result(10);
        auto final_result =
            result.and_then([](int v) -> result_t<int> { return v * 2; }).and_then([](int v) -> result_t<int> {
                return v + 5;
            });

        EXPECT_TRUE(final_result.is_success());
        EXPECT_EQ(final_result.value(), 25);
    }

    TEST_F(result_t_test, and_then_chained_error_propagation) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "step 1 error");
        auto final_result =
            result.and_then([](int v) -> result_t<int> { return v * 2; }).and_then([](int v) -> result_t<int> {
                return v + 5;
            });

        EXPECT_TRUE(final_result.is_error());
        EXPECT_EQ(final_result.error().message, "step 1 error");
    }

    TEST_F(result_t_test, or_else_success_lvalue) {
        result_t<int> result(42);
        auto next = result.or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 0; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 42);
    }

    TEST_F(result_t_test, or_else_error_lvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "original error");
        auto next = result.or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 100; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 100);
    }

    TEST_F(result_t_test, or_else_success_rvalue) {
        result_t<int> result(42);
        auto next = std::move(result).or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 0; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 42);
    }

    TEST_F(result_t_test, or_else_error_rvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "original error");
        auto next = std::move(result).or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 100; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 100);
    }

    TEST_F(result_t_test, or_else_recover_with_error) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code1, "original error");
        auto code2 = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 0, 0, 2);
        auto next = result.or_else(
            [&](const error_context_t& /*ctx*/) -> result_t<int> { return result_t<int>(code2, "recovered error"); });

        EXPECT_TRUE(next.is_error());
        EXPECT_EQ(next.error().message, "recovered error");
    }

    TEST_F(result_t_test, and_then_void_success_lvalue) {
        result_t<void> result;
        auto next = result.and_then([]() -> result_t<int> { return 42; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 42);
    }

    TEST_F(result_t_test, and_then_void_error_lvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<void> result(code, "void error");
        auto next = result.and_then([]() -> result_t<int> { return 42; });

        EXPECT_TRUE(next.is_error());
        EXPECT_EQ(next.error().message, "void error");
    }

    TEST_F(result_t_test, and_then_void_success_rvalue) {
        result_t<void> result;
        auto next = std::move(result).and_then([]() -> result_t<int> { return 42; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 42);
    }

    TEST_F(result_t_test, and_then_void_error_rvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<void> result(code, "void error");
        auto next = std::move(result).and_then([]() -> result_t<int> { return 42; });

        EXPECT_TRUE(next.is_error());
        EXPECT_EQ(next.error().message, "void error");
    }

    TEST_F(result_t_test, or_else_void_success_lvalue) {
        result_t<void> result;
        auto next = result.or_else([](const error_context_t& /*ctx*/) -> result_t<void> { return result_t<void>(); });

        EXPECT_TRUE(next.is_success());
    }

    TEST_F(result_t_test, or_else_void_error_lvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<void> result(code, "void error");
        auto next = result.or_else([](const error_context_t& /*ctx*/) -> result_t<void> { return result_t<void>(); });

        EXPECT_TRUE(next.is_success());
    }

    TEST_F(result_t_test, or_else_void_success_rvalue) {
        result_t<void> result;
        auto next = std::move(result).or_else(
            [](const error_context_t& /*ctx*/) -> result_t<void> { return result_t<void>(); });

        EXPECT_TRUE(next.is_success());
    }

    TEST_F(result_t_test, or_else_void_error_rvalue) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<void> result(code, "void error");
        auto next = std::move(result).or_else(
            [](const error_context_t& /*ctx*/) -> result_t<void> { return result_t<void>(); });

        EXPECT_TRUE(next.is_success());
    }

    TEST_F(result_t_test, and_then_with_move_only_type) {
        result_t<std::unique_ptr<int>> result(std::make_unique<int>(42));
        auto next = std::move(result).and_then([](std::unique_ptr<int> ptr) -> result_t<int> { return *ptr; });

        EXPECT_TRUE(next.is_success());
        EXPECT_EQ(next.value(), 42);
    }

    TEST_F(result_t_test, combined_and_then_or_else) {
        result_t<int> result(5);
        auto final_result = result.and_then([](int v) -> result_t<int> { return v * 2; })
                                .or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 0; })
                                .and_then([](int v) -> result_t<int> { return v + 3; });

        EXPECT_TRUE(final_result.is_success());
        EXPECT_EQ(final_result.value(), 13);
    }

    TEST_F(result_t_test, combined_error_and_then_or_else) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        result_t<int> result(code, "initial error");
        auto final_result = result.and_then([](int v) -> result_t<int> { return v * 2; })
                                .or_else([](const error_context_t& /*ctx*/) -> result_t<int> { return 99; })
                                .and_then([](int v) -> result_t<int> { return v + 1; });

        EXPECT_TRUE(final_result.is_success());
        EXPECT_EQ(final_result.value(), 100);
    }

}  // namespace error_system::core

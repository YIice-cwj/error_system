#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_config.h"
#include "error_system/core/error_context.h"
#include "error_system/core/error_exception.h"
#include "error_system/core/error_level.h"
#include "error_system/domain/system_domain.h"

#include <gtest/gtest.h>

namespace error_system::core {

    class error_exception_test : public ::testing::Test {
        protected:
        void SetUp() override { error_config_t::set_enable_validation(false); }
    };

    // ─── 构造 ───

    TEST_F(error_exception_test, construct_with_valid_context) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "exception message");

        error_exception_t ex(std::move(ctx));

        EXPECT_EQ(ex.code().get_code(), code.get_code());
        EXPECT_NE(std::strlen(ex.what()), 0u);
    }

    TEST_F(error_exception_test, construct_moves_context) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "move test");
        ctx.with("key", "value");

        error_exception_t ex(std::move(ctx));

        const error_context_t& stored = ex.context();
        EXPECT_EQ(stored.message, "move test");
        EXPECT_EQ(stored.payload.at("key"), "value");
    }

    // ─── what() ───

    TEST_F(error_exception_test, what_contains_message) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "what test");

        error_exception_t ex(std::move(ctx));

        EXPECT_NE(std::string(ex.what()).find("what test"), std::string::npos);
    }

    TEST_F(error_exception_test, what_contains_code_info) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 42);
        error_context_t ctx(code, "code info");

        error_exception_t ex(std::move(ctx));
        std::string what_str = ex.what();

        EXPECT_NE(what_str.find("code info"), std::string::npos);
    }

    TEST_F(error_exception_test, what_is_cached) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "cached");

        error_exception_t ex(std::move(ctx));
        const char* first = ex.what();
        const char* second = ex.what();

        EXPECT_EQ(first, second);
    }

    // ─── context() ───

    TEST_F(error_exception_test, context_returns_reference) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "context ref");

        error_exception_t ex(std::move(ctx));
        const error_context_t& ref = ex.context();

        EXPECT_EQ(ref.message, "context ref");
        EXPECT_TRUE(static_cast<bool>(ref));
    }

    TEST_F(error_exception_test, context_preserves_payload) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "payload test");
        ctx.with("host", "127.0.0.1").with("port", "8080");

        error_exception_t ex(std::move(ctx));
        const error_context_t& ref = ex.context();

        EXPECT_EQ(ref.payload.size(), 2);
        EXPECT_EQ(ref.payload.at("host"), "127.0.0.1");
        EXPECT_EQ(ref.payload.at("port"), "8080");
    }

    TEST_F(error_exception_test, context_preserves_cause_chain) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 2);

        error_context_t root(code1, "root cause");
        error_context_t outer(code2, "outer error");
        outer = outer.wrap(root);

        error_exception_t ex(std::move(outer));
        const error_context_t& ref = ex.context();

        ASSERT_NE(ref.cause, nullptr);
        EXPECT_EQ(ref.cause->message, "root cause");
    }

    // ─── code() ───

    TEST_F(error_exception_test, code_returns_correct_value) {
        auto code = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 1, 2, 3);
        error_context_t ctx(code, "code test");

        error_exception_t ex(std::move(ctx));
        error_code_t returned = ex.code();

        EXPECT_EQ(returned.get_level(), error_level_t::warn);
        EXPECT_EQ(returned.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(returned.get_subsys(), 1);
        EXPECT_EQ(returned.get_module(), 2);
        EXPECT_EQ(returned.get_number(), 3);
    }

    TEST_F(error_exception_test, code_returns_zero_for_default_context) {
        error_context_t ctx;
        error_exception_t ex(std::move(ctx));

        EXPECT_EQ(ex.code().get_code(), 0);
    }

    // ─── std::exception 接口 ───

    TEST_F(error_exception_test, inherits_from_std_exception) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "inheritance");

        error_exception_t ex(std::move(ctx));
        const std::exception& base = ex;

        EXPECT_NE(std::string(base.what()).find("inheritance"), std::string::npos);
    }

    TEST_F(error_exception_test, can_be_caught_as_std_exception) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "caught as std");

        try {
            error_exception_t ex(std::move(ctx));
            throw ex;
        } catch (const std::exception& e) {
            EXPECT_NE(std::string(e.what()).find("caught as std"), std::string::npos);
        }
    }

    TEST_F(error_exception_test, can_be_caught_as_error_exception) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "caught as ex");

        try {
            error_exception_t ex(std::move(ctx));
            throw ex;
        } catch (const error_exception_t& e) {
            EXPECT_EQ(e.code().get_code(), code.get_code());
            EXPECT_NE(std::string(e.what()).find("caught as ex"), std::string::npos);
        }
    }

    // ─── noexcept 保证 ───

    TEST_F(error_exception_test, what_is_noexcept) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "noexcept test");
        error_exception_t ex(std::move(ctx));

        static_assert(noexcept(ex.what()), "what() must be noexcept");
        EXPECT_TRUE(noexcept(ex.what()));
    }

    TEST_F(error_exception_test, context_is_noexcept) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "noexcept test");
        error_exception_t ex(std::move(ctx));

        static_assert(noexcept(ex.context()), "context() must be noexcept");
        EXPECT_TRUE(noexcept(ex.context()));
    }

    TEST_F(error_exception_test, code_is_noexcept) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "noexcept test");
        error_exception_t ex(std::move(ctx));

        static_assert(noexcept(ex.code()), "code() must be noexcept");
        EXPECT_TRUE(noexcept(ex.code()));
    }

    // ─── 源位置追踪 ───

    TEST_F(error_exception_test, exception_preserves_source_location) {
        error_config_t::set_enable_source_location(true);

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "location test");

        error_exception_t ex(std::move(ctx));
        const error_context_t& ref = ex.context();

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        EXPECT_NE(ref.file_name, nullptr);
        EXPECT_NE(ref.function_name, nullptr);
        EXPECT_NE(ref.line_number, 0u);
#endif
    }

    TEST_F(error_exception_test, what_contains_source_location) {
        error_config_t::set_enable_source_location(true);

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        error_context_t ctx(code, "location in what");

        error_exception_t ex(std::move(ctx));
        std::string what_str = ex.what();

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        EXPECT_NE(what_str.find("Location:"), std::string::npos);
#endif
    }

    TEST_F(error_exception_test, exception_with_cause_chain_preserves_location) {
        error_config_t::set_enable_source_location(true);

        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 0, 0, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::kernel, 0, 0, 2);

        error_context_t root(code1, "root cause");
        error_context_t outer(code2, "outer error");
        outer = outer.wrap(root);

        error_exception_t ex(std::move(outer));
        const error_context_t& ref = ex.context();

#ifdef ERROR_SYSTEM_ENABLE_LOCATION
        EXPECT_NE(ref.file_name, nullptr);
        ASSERT_NE(ref.cause, nullptr);
        EXPECT_NE(ref.cause->file_name, nullptr);
#endif
    }

}  // namespace error_system::core

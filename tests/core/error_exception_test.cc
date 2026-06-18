#include "error_system/core/error_exception.h"

#include <gtest/gtest.h>

#include "error_system/core/error_registry.h"

namespace error_system::core {

    class error_exception_test_t : public ::testing::Test {
        protected:
        void SetUp() override { error_registry_t::instance().unregister_all(); }

        void TearDown() override { error_registry_t::instance().unregister_all(); }
    };

    TEST_F(error_exception_test_t, exception_stores_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 42);
        error_registry_t::instance().register_error(code, "ERR_42", "Test error 42");

        error_context_t context(code, "test message");
        error_exception_t ex(context);

        EXPECT_EQ(ex.code().get_code(), code.get_code());
    }

    TEST_F(error_exception_test_t, exception_inherits_from_std_exception) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(code, "error");
        error_exception_t ex(context);

        const std::exception* base = &ex;
        EXPECT_NE(base, nullptr);
        EXPECT_STREQ(base->what(), ex.what());
    }

    TEST_F(error_exception_test_t, context_returns_original_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 100);
        error_registry_t::instance().register_error(code, "ERR_100", "Error 100");

        error_context_t context(code, "original");
        error_exception_t ex(context);

        const auto& retrieved = ex.context();
        EXPECT_EQ(retrieved.get_code().get_code(), code.get_code());
    }

    TEST_F(error_exception_test_t, move_context_into_exception) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, 0, 0, 200);
        error_registry_t::instance().register_error(code, "ERR_200", "Error 200");

        error_context_t context(code, "moved");
        error_exception_t ex(std::move(context));

        EXPECT_EQ(ex.code().get_code(), code.get_code());
    }

    TEST_F(error_exception_test_t, noexcept_constructor) {
        static_assert(std::is_nothrow_constructible_v<error_exception_t, error_context_t>,
                      "constructor should be noexcept");
    }

}  // namespace error_system::core
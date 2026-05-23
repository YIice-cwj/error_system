#include "error_system/core/error_builder.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class error_builder_test : public ::testing::Test {};

    TEST_F(error_builder_test, make_error_code_with_raw_values) {
        auto code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 2, 0x1234);
        EXPECT_EQ(code.get_level(), error_level_t::error);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(code.get_subsys(), 1);
        EXPECT_EQ(code.get_module(), 2);
        EXPECT_EQ(code.get_number(), 0x1234);
        EXPECT_EQ(code.get_sign(), 1);
    }

    TEST_F(error_builder_test, make_error_code_with_enum_values) {
        enum class subsys_t : uint16_t { db_connection = 1 };
        enum class module_t : uint16_t { timeout = 2 };

        auto code = error_builder_t::make_error_code(error_level_t::warn,
                                                     domain::system_domain_t::application,
                                                     subsys_t::db_connection,
                                                     module_t::timeout,
                                                     0x0001);
        EXPECT_EQ(code.get_level(), error_level_t::warn);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::application);
        EXPECT_EQ(code.get_subsys(), 1);
        EXPECT_EQ(code.get_module(), 2);
        EXPECT_EQ(code.get_number(), 0x0001);
    }

    TEST_F(error_builder_test, make_error_code_from_raw_code) {
        code_t raw = 0x0000000000000001ULL;
        auto code = error_builder_t::make_error_code(raw);
        EXPECT_EQ(code.get_code(), raw);
        EXPECT_EQ(code.get_sign(), 0);
    }

    TEST_F(error_builder_test, constexpr_error_code_construction) {
        constexpr auto code =
            error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::system, 0, 0, 0xFFFF);
        static_assert(code.get_code() != 0, "constexpr failed");
        EXPECT_EQ(code.get_level(), error_level_t::fatal);
    }

    TEST_F(error_builder_test, success_code_has_zero_sign) {
        auto code = error_builder_t::make_error_code(error_level_t::debug, domain::system_domain_t::none, 0, 0, 0);
        EXPECT_EQ(code.get_sign(), 1);  // sign=1 表示错误，即使是 debug 级别
    }

    TEST_F(error_builder_test, different_levels_produce_different_codes) {
        auto debug_code =
            error_builder_t::make_error_code(error_level_t::debug, domain::system_domain_t::none, 0, 0, 1);
        auto error_code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::none, 0, 0, 1);

        EXPECT_NE(debug_code.get_code(), error_code.get_code());
        EXPECT_EQ(debug_code.get_level(), error_level_t::debug);
        EXPECT_EQ(error_code.get_level(), error_level_t::error);
    }

    TEST_F(error_builder_test, different_systems_produce_different_codes) {
        auto db_code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 0, 0, 1);
        auto app_code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::application, 0, 0, 1);

        EXPECT_NE(db_code.get_code(), app_code.get_code());
        EXPECT_EQ(db_code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(app_code.get_system(), domain::system_domain_t::application);
    }

}  // namespace error_system::core

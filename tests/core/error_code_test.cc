#include "error_system/core/error_code.h"

#include <gtest/gtest.h>

namespace error_system::core {

    class error_code_test_t : public ::testing::Test {};

    TEST_F(error_code_test_t, default_constructor_creates_success_code) {
        error_code_t code;
        EXPECT_EQ(code.get_code(), 0x8000000000000000ULL);
        EXPECT_EQ(code.get_sign(), 1);  // sign=1 = true = 成功
        EXPECT_EQ(code.get_level(), error_level_t::debug);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::none);
        EXPECT_EQ(code.get_subsys(), 0);
        EXPECT_EQ(code.get_module(), 0);
        EXPECT_EQ(code.get_number(), 0);
    }

    TEST_F(error_code_test_t, explicit_constructor_stores_raw_code) {
        code_t raw = 0x123456789ABCDEF0ULL;
        error_code_t code(raw);
        EXPECT_EQ(code.get_code(), raw);
    }

    TEST_F(error_code_test_t, explicit_conversion_to_code_t) {
        error_code_t code(0x1234);
        code_t raw = static_cast<code_t>(code);
        EXPECT_EQ(raw, 0x1234ULL);
    }

    TEST_F(error_code_test_t, get_module_group_id_extracts_correct_bits) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::system, 2, 3, 4);
        auto group_id = code.get_module_group_id();
        EXPECT_NE(group_id, 0ULL);
        EXPECT_EQ(group_id & 0xFFFFULL, 0ULL);
    }

    TEST_F(error_code_test_t, fields_are_correctly_extracted) {
        error_code_t code(0x0000000000000001ULL);
        EXPECT_EQ(code.get_sign(), 0);
    }

    TEST_F(error_code_test_t, constexpr_evaluation_works) {
        constexpr error_code_t code(0x1234);
        static_assert(code.get_code() == 0x1234ULL, "constexpr evaluation failed");
        EXPECT_EQ(code.get_code(), 0x1234ULL);
    }

    TEST_F(error_code_test_t, zero_code_represents_success) {
        constexpr error_code_t code(0);
        EXPECT_EQ(code.get_sign(), 0);
        EXPECT_EQ(code.get_level(), error_level_t::debug);
    }

    TEST_F(error_code_test_t, five_parameter_constructor_builds_correct_code) {
        constexpr error_code_t code(
            error_level_t::error,
            domain::system_domain_t::database,
            0x0102,  // subsys
            0x0304,  // module
            0x0506   // number
        );

        EXPECT_EQ(code.get_sign(), 0);  // sign=0 = false = 错误
        EXPECT_EQ(code.get_level(), error_level_t::error);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(code.get_subsys(), 0x0102);
        EXPECT_EQ(code.get_module(), 0x0304);
        EXPECT_EQ(code.get_number(), 0x0506);
    }

    TEST_F(error_code_test_t, constexpr_five_parameter_constructor) {
        constexpr error_level_t level = error_level_t::warn;
        constexpr domain::system_domain_t sys = domain::system_domain_t::application;
        constexpr uint16_t subsys = 101;
        constexpr uint16_t module = 1;
        constexpr uint16_t number = 1;

        constexpr error_code_t code(level, sys, subsys, module, number);

        static_assert(code.get_level() == level, "");
        static_assert(code.get_system() == sys, "");
        static_assert(code.get_subsys() == subsys, "");
        static_assert(code.get_module() == module, "");
        static_assert(code.get_number() == number, "");
        EXPECT_TRUE(true);
    }

}  // namespace error_system::core

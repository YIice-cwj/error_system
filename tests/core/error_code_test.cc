#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class error_code_test : public ::testing::Test {};

    TEST_F(error_code_test, default_constructor_creates_success_code) {
        error_code_t code;
        EXPECT_EQ(code.get_code(), 0ULL);
        EXPECT_EQ(code.get_sign(), 0);
        EXPECT_EQ(code.get_level(), error_level_t::debug);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::none);
        EXPECT_EQ(code.get_subsys(), 0);
        EXPECT_EQ(code.get_module(), 0);
        EXPECT_EQ(code.get_number(), 0);
    }

    TEST_F(error_code_test, explicit_constructor_stores_raw_code) {
        code_t raw = 0x123456789ABCDEF0ULL;
        error_code_t code(raw);
        EXPECT_EQ(code.get_code(), raw);
    }

    TEST_F(error_code_test, implicit_conversion_to_code_t) {
        error_code_t code(0x1234);
        code_t raw = code;
        EXPECT_EQ(raw, 0x1234ULL);
    }

    TEST_F(error_code_test, get_module_group_id_extracts_correct_bits) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 2, 3, 4);
        auto group_id = code.get_module_group_id();
        EXPECT_NE(group_id, 0ULL);
        EXPECT_EQ(group_id & 0xFFFFULL, 0ULL);
    }

    TEST_F(error_code_test, fields_are_correctly_extracted) {
        error_code_t code(0x8000000000000001ULL);
        EXPECT_EQ(code.get_sign(), 1);
    }

    TEST_F(error_code_test, constexpr_evaluation_works) {
        constexpr error_code_t code(0x1234);
        static_assert(code.get_code() == 0x1234ULL, "constexpr evaluation failed");
        EXPECT_EQ(code.get_code(), 0x1234ULL);
    }

    TEST_F(error_code_test, zero_code_represents_success) {
        constexpr error_code_t code(0);
        EXPECT_EQ(code.get_sign(), 0);
        EXPECT_EQ(code.get_level(), error_level_t::debug);
    }

}  // namespace error_system::core

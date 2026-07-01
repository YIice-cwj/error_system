#include "error_system/core/error_builder.h"

#include <gtest/gtest.h>

namespace error_system::core {

    /** error_code_t 构造函数测试（替代 error_builder_t 原始值重载） */
    TEST(error_builder_test, error_code_constructor_with_uint16) {
        error_code_t code(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{2}, error_number_t{0x1234});
        EXPECT_EQ(code.get_level(), error_level_t::error);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(code.get_subsys(), 1);
        EXPECT_EQ(code.get_module(), 2);
        EXPECT_EQ(code.get_number(), 0x1234);
        /** sign=0 = false = 错误 */
        EXPECT_EQ(code.get_sign(), 0);
    }

    /** error_builder_t 枚举模板版本测试（编译期类型安全） */
    TEST(error_builder_test, make_error_code_with_enum_values) {
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
        EXPECT_EQ(code.get_sign(), 0);
    }

    TEST(error_builder_test, from_raw_restores_code) {
        code_t raw = 0x8000000000000001ULL;
        auto code = error_builder_t::from_raw(raw);
        EXPECT_EQ(code.get_code(), raw);
        EXPECT_EQ(code.get_sign(), 1);
    }

    TEST(error_builder_test, constexpr_error_code_construction) {
        constexpr error_code_t code(error_level_t::fatal, domain::system_domain_t::system, subsystem_id_t{0}, module_id_t{0}, error_number_t{0xFFFF});
        static_assert(code.get_code() != 0, "constexpr failed");
        EXPECT_EQ(code.get_level(), error_level_t::fatal);
    }

    TEST(error_builder_test, error_code_has_sign_zero) {
        error_code_t code(error_level_t::debug, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{0});
        EXPECT_EQ(code.get_sign(), 0);
    }

    TEST(error_builder_test, different_levels_produce_different_codes) {
        error_code_t debug_code(error_level_t::debug, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_code_t error_code(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});

        EXPECT_NE(debug_code.get_code(), error_code.get_code());
        EXPECT_EQ(debug_code.get_level(), error_level_t::debug);
        EXPECT_EQ(error_code.get_level(), error_level_t::error);
    }

    TEST(error_builder_test, different_systems_produce_different_codes) {
        error_code_t db_code(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_code_t app_code(error_level_t::error, domain::system_domain_t::application, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});

        EXPECT_NE(db_code.get_code(), app_code.get_code());
        EXPECT_EQ(db_code.get_system(), domain::system_domain_t::database);
        EXPECT_EQ(app_code.get_system(), domain::system_domain_t::application);
    }

}  // namespace error_system::core

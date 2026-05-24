#include "error_system/core/error_registry.h"
#include <gtest/gtest.h>

namespace error_system::core {

    class error_registry_test : public ::testing::Test {
        protected:
        void SetUp() override {
            error_registry_t::instance().unregister_all();
            error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        }

        void TearDown() override {
            error_registry_t::instance().unregister_all();
            error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        }
    };

    TEST_F(error_registry_test, register_and_retrieve_error) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_DB_CONN", "Database connection failed");

        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->get().name, "ERR_DB_CONN");
        EXPECT_EQ(info->get().description, "Database connection failed");
    }

    TEST_F(error_registry_test, register_multiple_errors) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::application, 2, 2, 2);

        std::vector<error_code_t> codes = {code1, code2};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1", "Desc 2"};

        error_registry_t::instance().register_errors(codes, names, descs);

        EXPECT_TRUE(error_registry_t::instance().is_registered(code1));
        EXPECT_TRUE(error_registry_t::instance().is_registered(code2));
    }

    TEST_F(error_registry_test, unregister_error_by_code) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_TEST", "Test error");
        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        error_registry_t::instance().unregister_error(code);
        EXPECT_FALSE(error_registry_t::instance().is_registered(code));
    }

    TEST_F(error_registry_test, unregister_error_by_name) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_BY_NAME", "Test");
        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        error_registry_t::instance().unregister_error("ERR_BY_NAME");
        EXPECT_FALSE(error_registry_t::instance().is_registered(code));
    }

    TEST_F(error_registry_test, unregister_module_group) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");

        auto module_id = code1.get_module_group_id();
        error_registry_t::instance().unregister_module(module_id);

        EXPECT_FALSE(error_registry_t::instance().is_registered(code1));
        EXPECT_FALSE(error_registry_t::instance().is_registered(code2));
    }

    TEST_F(error_registry_test, get_info_returns_nullopt_for_unregistered) {
        auto code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 99, 99, 99);

        auto info = error_registry_t::instance().get_info(code);
        EXPECT_FALSE(info.has_value());
    }

    TEST_F(error_registry_test, get_errors_by_module) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);
        auto code3 =
            error_builder_t::make_error_code(error_level_t::info, domain::system_domain_t::application, 2, 2, 1);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");
        error_registry_t::instance().register_error(code3, "ERR_3", "Error 3");

        auto module_id = code1.get_module_group_id();
        auto errors = error_registry_t::instance().get_errors_by_module(module_id);

        EXPECT_EQ(errors.size(), 2);
        EXPECT_EQ(errors[0].get().name, "ERR_1");
        EXPECT_EQ(errors[1].get().name, "ERR_2");
        EXPECT_EQ(errors[0].get().name, "ERR_1");
        EXPECT_EQ(errors[1].get().name, "ERR_2");
    }

    TEST_F(error_registry_test, singleton_returns_same_instance) {
        auto& instance1 = error_registry_t::instance();
        auto& instance2 = error_registry_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_registry_test, register_duplicate_keeps_first_metadata) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "OLD_NAME", "Old description");
        error_registry_t::instance().register_error(code, "NEW_NAME", "New description");

        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        // Duplicate registration should keep the first registered metadata
        EXPECT_EQ(info->get().name, "OLD_NAME");
        EXPECT_EQ(info->get().description, "Old description");
    }

    TEST_F(error_registry_test, duplicate_policy_skip) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        // First registration should be kept
        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->get().name, "FIRST");
    }

    TEST_F(error_registry_test, duplicate_policy_overwrite) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->get().name, "SECOND");
        EXPECT_EQ(info->get().description, "Second description");
    }

    TEST_F(error_registry_test, duplicate_policy_get_set) {
        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::skip);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::overwrite);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::warn);
    }

    TEST_F(error_registry_test, register_errors_returns_count) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::application, 2, 2, 2);

        std::vector<error_code_t> codes = {code1, code2};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1", "Desc 2"};

        size_t count = error_registry_t::instance().register_errors(codes, names, descs);
        EXPECT_EQ(count, 2);
    }

    TEST_F(error_registry_test, register_errors_returns_zero_for_mismatched_arrays) {
        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        std::vector<error_code_t> codes = {code1};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1"};

        size_t count = error_registry_t::instance().register_errors(codes, names, descs);
        EXPECT_EQ(count, 0);
    }

    TEST_F(error_registry_test, register_errors_with_overwrite_policy) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ORIGINAL", "Original desc");

        std::vector<error_code_t> codes = {code};
        std::vector<std::string_view> names = {"UPDATED"};
        std::vector<std::string_view> descs = {"Updated desc"};

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        size_t count = error_registry_t::instance().register_errors(codes, names, descs);

        EXPECT_EQ(count, 1);
        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->get().name, "UPDATED");
        EXPECT_EQ(info->get().description, "Updated desc");
    }

    TEST_F(error_registry_test, duplicate_policy_warn_triggers_callback) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        bool callback_called = false;
        code_t captured_code = 0;
        std::string captured_name;

        error_registry_t::instance().set_duplicate_warn_callback(
            [&callback_called, &captured_code, &captured_name](code_t raw_code, const error_metadata_t& meta) {
                callback_called = true;
                captured_code = raw_code;
                captured_name = meta.name;
            });

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        EXPECT_TRUE(callback_called);
        EXPECT_EQ(captured_code, code.get_identity_code());
        EXPECT_EQ(captured_name, "FIRST");

        // 恢复默认回调
        error_registry_t::instance().set_duplicate_warn_callback(nullptr);
    }

    TEST_F(error_registry_test, duplicate_warn_callback_can_be_set_and_get) {
        auto& registry = error_registry_t::instance();

        // 默认应该有回调
        EXPECT_TRUE(registry.get_duplicate_warn_callback());

        // 设置为 nullptr
        registry.set_duplicate_warn_callback(nullptr);
        EXPECT_FALSE(registry.get_duplicate_warn_callback());

        // 设置自定义回调
        bool called = false;
        registry.set_duplicate_warn_callback([&called](code_t, const error_metadata_t&) { called = true; });
        EXPECT_TRUE(registry.get_duplicate_warn_callback());
    }

    TEST_F(error_registry_test, duplicate_warn_callback_not_called_on_first_registration) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        bool callback_called = false;
        error_registry_t::instance().set_duplicate_warn_callback(
            [&callback_called](code_t, const error_metadata_t&) { callback_called = true; });
        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);

        // 首次注册不应触发回调
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        EXPECT_FALSE(callback_called);

        error_registry_t::instance().set_duplicate_warn_callback(nullptr);
    }

    TEST_F(error_registry_test, duplicate_warn_keeps_original_metadata) {
        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);
        error_registry_t::instance().register_error(code, "ORIGINAL", "Original description");
        error_registry_t::instance().register_error(code, "NEW", "New description");

        auto info = error_registry_t::instance().get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->get().name, "ORIGINAL");
        EXPECT_EQ(info->get().description, "Original description");
    }

}  // namespace error_system::core
    
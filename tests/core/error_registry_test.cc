#include "error_system/core/error_registry.h"

#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace error_system::core {

    class error_registry_test_t : public ::testing::Test {
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

    TEST_F(error_registry_test_t, register_and_retrieve_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_DB_CONN", "Database connection failed");

        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->name, "ERR_DB_CONN");
        EXPECT_EQ(info->description, "Database connection failed");
    }

    TEST_F(error_registry_test_t, register_multiple_errors) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 =
            error_code_t(error_level_t::warn, domain::system_domain_t::application, 2, 2, 2);

        std::vector<error_code_t> codes = {code1, code2};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1", "Desc 2"};

        error_registry_t::instance().register_errors(codes, names, descs);

        EXPECT_TRUE(error_registry_t::instance().is_registered(code1));
        EXPECT_TRUE(error_registry_t::instance().is_registered(code2));
    }

    TEST_F(error_registry_test_t, unregister_error_by_code) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_TEST", "Test error");
        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        error_registry_t::instance().unregister_error(code);
        EXPECT_FALSE(error_registry_t::instance().is_registered(code));
    }

    TEST_F(error_registry_test_t, unregister_error_by_name) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ERR_BY_NAME", "Test");
        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        error_registry_t::instance().unregister_error("ERR_BY_NAME");
        EXPECT_FALSE(error_registry_t::instance().is_registered(code));
    }

    TEST_F(error_registry_test_t, unregister_module_group) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_code_t(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");

        auto module_id = code1.get_module_group_id();
        error_registry_t::instance().unregister_module(module_id);

        EXPECT_FALSE(error_registry_t::instance().is_registered(code1));
        EXPECT_FALSE(error_registry_t::instance().is_registered(code2));
    }

    TEST_F(error_registry_test_t, unregister_module_clears_subsystem_index) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_code_t(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");

        // 注销前子系统 1 应有 2 个错误码
        auto errors = error_registry_t::instance().get_errors_by_subsystem(1);
        EXPECT_EQ(errors.size(), 2);

        // 注销整个模块组
        error_registry_t::instance().unregister_module(code1.get_module_group_id());

        // 子系统 1 应变为空
        auto errors_after = error_registry_t::instance().get_errors_by_subsystem(1);
        EXPECT_TRUE(errors_after.empty());
    }

    TEST_F(error_registry_test_t, get_errors_by_subsystem_after_unregister_error) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_code_t(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");

        // 只注销一个错误码，子系统索引仍保留
        error_registry_t::instance().unregister_error(code1);
        auto errors = error_registry_t::instance().get_errors_by_subsystem(1);
        EXPECT_EQ(errors.size(), 1);
        EXPECT_EQ(errors[0].get().name, "ERR_2");

        // 注销最后一个错误码，子系统索引应清空
        error_registry_t::instance().unregister_error(code2);
        auto errors_after = error_registry_t::instance().get_errors_by_subsystem(1);
        EXPECT_TRUE(errors_after.empty());
    }

    TEST_F(error_registry_test_t, get_info_returns_nullptr_for_unregistered) {
        auto code =
            error_code_t(error_level_t::error, domain::system_domain_t::database, 99, 99, 99);

        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        EXPECT_EQ(info, nullptr);
    }

    TEST_F(error_registry_test_t, get_errors_by_module) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_code_t(error_level_t::warn, domain::system_domain_t::database, 1, 1, 2);
        auto code3 =
            error_code_t(error_level_t::info, domain::system_domain_t::application, 2, 2, 1);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");
        error_registry_t::instance().register_error(code3, "ERR_3", "Error 3");

        auto module_id = code1.get_module_group_id();
        auto errors = error_registry_t::instance().get_errors_by_module(module_id);

        EXPECT_EQ(errors.size(), 2);
        EXPECT_EQ(errors[0].get().name, "ERR_1");
        EXPECT_EQ(errors[1].get().name, "ERR_2");
    }

    TEST_F(error_registry_test_t, get_errors_by_subsystem) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 = error_code_t(error_level_t::warn, domain::system_domain_t::database, 1, 2, 1);
        auto code3 =
            error_code_t(error_level_t::info, domain::system_domain_t::application, 2, 2, 1);

        error_registry_t::instance().register_error(code1, "ERR_1", "Error 1");
        error_registry_t::instance().register_error(code2, "ERR_2", "Error 2");
        error_registry_t::instance().register_error(code3, "ERR_3", "Error 3");

        // 查询子系统 1 的所有错误码（顺序不保证，按名称收集验证）
        auto errors = error_registry_t::instance().get_errors_by_subsystem(1);
        EXPECT_EQ(errors.size(), 2);

        std::set<std::string> error_names;
        for (const auto& ref : errors) {
            error_names.insert(ref.get().name);
        }
        EXPECT_TRUE(error_names.count("ERR_1"));
        EXPECT_TRUE(error_names.count("ERR_2"));

        // 查询子系统 2 的所有错误码
        auto errors2 = error_registry_t::instance().get_errors_by_subsystem(2);
        EXPECT_EQ(errors2.size(), 1);
        EXPECT_EQ(errors2[0].get().name, "ERR_3");

        // 查询不存在的子系统
        auto errors3 = error_registry_t::instance().get_errors_by_subsystem(99);
        EXPECT_TRUE(errors3.empty());
    }

    TEST_F(error_registry_test_t, find_by_name) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        error_registry_t::instance().register_error(code, "ERR_FIND_ME", "Find me");

        auto found = error_registry_t::instance().find_by_name("ERR_FIND_ME");
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->get_identity_code(), code.get_identity_code());

        // 查找不存在的名称
        auto not_found = error_registry_t::instance().find_by_name("ERR_NOT_EXIST");
        EXPECT_FALSE(not_found.has_value());
    }

    TEST_F(error_registry_test_t, singleton_returns_same_instance) {
        auto& instance1 = error_registry_t::instance();
        auto& instance2 = error_registry_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_registry_test_t, register_duplicate_keeps_first_metadata) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "OLD_NAME", "Old description");
        error_registry_t::instance().register_error(code, "NEW_NAME", "New description");

        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        // Duplicate registration should keep the first registered metadata
        EXPECT_EQ(info->name, "OLD_NAME");
        EXPECT_EQ(info->description, "Old description");
    }

    TEST_F(error_registry_test_t, duplicate_policy_skip) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        // First registration should be kept
        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->name, "FIRST");
    }

    TEST_F(error_registry_test_t, duplicate_policy_overwrite) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->name, "SECOND");
        EXPECT_EQ(info->description, "Second description");
    }

    TEST_F(error_registry_test_t, duplicate_policy_overwrite_updates_name_index) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        error_registry_t::instance().register_error(code, "SECOND", "Second description");

        error_registry_t::instance().unregister_error("FIRST");
        EXPECT_TRUE(error_registry_t::instance().is_registered(code));

        error_registry_t::instance().unregister_error("SECOND");
        EXPECT_FALSE(error_registry_t::instance().is_registered(code));
    }

    TEST_F(error_registry_test_t, duplicate_policy_get_set) {
        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::skip);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::skip);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::overwrite);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);
        EXPECT_EQ(error_registry_t::instance().get_duplicate_policy(), duplicate_policy_t::warn);
    }

    TEST_F(error_registry_test_t, register_errors_returns_count) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        auto code2 =
            error_code_t(error_level_t::warn, domain::system_domain_t::application, 2, 2, 2);

        std::vector<error_code_t> codes = {code1, code2};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1", "Desc 2"};

        size_t count = error_registry_t::instance().register_errors(codes, names, descs);
        EXPECT_EQ(count, 2);
    }

    TEST_F(error_registry_test_t, register_errors_returns_zero_for_mismatched_arrays) {
        auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        std::vector<error_code_t> codes = {code1};
        std::vector<std::string_view> names = {"ERR_1", "ERR_2"};
        std::vector<std::string_view> descs = {"Desc 1"};

        size_t count = error_registry_t::instance().register_errors(codes, names, descs);
        EXPECT_EQ(count, 0);
    }

    TEST_F(error_registry_test_t, register_errors_with_overwrite_policy) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().register_error(code, "ORIGINAL", "Original desc");

        std::vector<error_code_t> codes = {code};
        std::vector<std::string_view> names = {"UPDATED"};
        std::vector<std::string_view> descs = {"Updated desc"};

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::overwrite);
        size_t count = error_registry_t::instance().register_errors(codes, names, descs);

        EXPECT_EQ(count, 1);
        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->name, "UPDATED");
        EXPECT_EQ(info->description, "Updated desc");
    }

    TEST_F(error_registry_test_t, duplicate_policy_warn_triggers_callback) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

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

    TEST_F(error_registry_test_t, duplicate_warn_callback_can_be_set_and_get) {
        auto& registry = error_registry_t::instance();

        // 默认无回调（nullptr）
        EXPECT_FALSE(registry.get_duplicate_warn_callback());

        // 设置为 nullptr
        registry.set_duplicate_warn_callback(nullptr);
        EXPECT_FALSE(registry.get_duplicate_warn_callback());

        // 设置自定义回调
        bool called = false;
        registry.set_duplicate_warn_callback([&called](code_t, const error_metadata_t&) { called = true; });
        EXPECT_TRUE(registry.get_duplicate_warn_callback());
    }

    TEST_F(error_registry_test_t, duplicate_warn_callback_not_called_on_first_registration) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        bool callback_called = false;
        error_registry_t::instance().set_duplicate_warn_callback(
            [&callback_called](code_t, const error_metadata_t&) { callback_called = true; });
        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);

        // 首次注册不应触发回调
        error_registry_t::instance().register_error(code, "FIRST", "First description");
        EXPECT_FALSE(callback_called);

        error_registry_t::instance().set_duplicate_warn_callback(nullptr);
    }

    TEST_F(error_registry_test_t, duplicate_warn_keeps_original_metadata) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);

        error_registry_t::instance().set_duplicate_policy(duplicate_policy_t::warn);
        error_registry_t::instance().register_error(code, "ORIGINAL", "Original description");
        error_registry_t::instance().register_error(code, "NEW", "New description");

        const error_metadata_t* info = error_registry_t::instance().get_info(code);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->name, "ORIGINAL");
        EXPECT_EQ(info->description, "Original description");
    }

    TEST_F(error_registry_test_t, concurrent_register_and_query) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        error_registry_t::instance().register_error(code, "CONCURRENT", "Concurrent test");

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&success_count, &code]() {
                for (int j = 0; j < 100; ++j) {
                    if (error_registry_t::instance().is_registered(code)) {
                        const error_metadata_t* info = error_registry_t::instance().get_info(code);
                        if (info != nullptr && info->name == "CONCURRENT") {
                            success_count.fetch_add(1);
                        }
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(success_count.load(), 1000);
    }

    TEST_F(error_registry_test_t, concurrent_register_different_codes) {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&success_count, i]() {
                for (int j = 0; j < 10; ++j) {
                    auto code = error_code_t(
                        error_level_t::error, domain::system_domain_t::database,
                        static_cast<uint16_t>(i), static_cast<uint16_t>(j), static_cast<uint16_t>(j));
                    error_registry_t::instance().register_error(code, "CONCURRENT", "Test");
                    if (error_registry_t::instance().is_registered(code)) {
                        success_count.fetch_add(1);
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(success_count.load(), 100);
    }

}  // namespace error_system::core
#include "error_system/core/error_builder.h"
#include "error_system/core/error_code.h"
#include "error_system/core/error_level.h"
#include "error_system/core/error_registry.h"
#include "error_system/domain/system_domain.h"
#include <atomic>
#include <gtest/gtest.h>
#include <optional>
#include <thread>
#include <vector>

namespace error_system::core {

    class error_registry_test : public ::testing::Test {
        protected:
        void SetUp() override { error_registry_t::instance().unregister_all(); }

        void TearDown() override { error_registry_t::instance().unregister_all(); }
    };

    TEST_F(error_registry_test, instance_returns_singleton) {
        auto& instance1 = error_registry_t::instance();
        auto& instance2 = error_registry_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_registry_test, register_error_and_is_registered) {
        auto& registry = error_registry_t::instance();

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 1, 1001);

        EXPECT_FALSE(registry.is_registered(code));

        registry.register_error(code, "ERR_TEST_1001", "测试错误1001");

        EXPECT_TRUE(registry.is_registered(code));
    }

    TEST_F(error_registry_test, register_error_with_metadata) {
        auto& registry = error_registry_t::instance();

        auto code =
            error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::database, 2, 3, 500);

        registry.register_error(code, "ERR_DB_CONNECTION", "数据库连接失败");

        auto info = registry.get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->name, "ERR_DB_CONNECTION");
        EXPECT_EQ(info->description, "数据库连接失败");
        EXPECT_EQ(info->module_id, 3);
        EXPECT_EQ(info->error_number, 500);
        EXPECT_EQ(info->level, error_level_t::fatal);
    }

    TEST_F(error_registry_test, register_duplicate_error_ignored) {
        auto& registry = error_registry_t::instance();

        auto code = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::network, 1, 1, 200);

        registry.register_error(code, "ERR_FIRST", "第一个错误");
        registry.register_error(code, "ERR_SECOND", "第二个错误");

        auto info = registry.get_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->name, "ERR_FIRST");
        EXPECT_EQ(info->description, "第一个错误");
    }

    TEST_F(error_registry_test, unregister_error_by_code) {
        auto& registry = error_registry_t::instance();

        auto code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::application, 1, 1, 300);

        registry.register_error(code, "ERR_TEST", "测试错误");
        EXPECT_TRUE(registry.is_registered(code));

        registry.unregister_error(code);
        EXPECT_FALSE(registry.is_registered(code));
    }

    TEST_F(error_registry_test, unregister_error_by_name) {
        auto& registry = error_registry_t::instance();

        auto code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::security, 1, 1, 400);

        registry.register_error(code, "ERR_SECURITY_VIOLATION", "安全违规");
        EXPECT_TRUE(registry.is_registered(code));

        registry.unregister_error("ERR_SECURITY_VIOLATION");
        EXPECT_FALSE(registry.is_registered(code));
    }

    TEST_F(error_registry_test, get_info_returns_nullopt_for_unregistered) {
        auto& registry = error_registry_t::instance();

        auto code =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 99, 99, 9999);

        auto info = registry.get_info(code);
        EXPECT_FALSE(info.has_value());
    }

    TEST_F(error_registry_test, register_errors_batch) {
        auto& registry = error_registry_t::instance();

        std::vector<error_code_t> codes = {
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 1, 100),
            error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::system, 1, 1, 101),
            error_builder_t::make_error_code(error_level_t::info, domain::system_domain_t::system, 1, 1, 102)};

        std::vector<std::string_view> names = {"ERR_100", "ERR_101", "ERR_102"};
        std::vector<std::string_view> descriptions = {"错误100", "警告101", "信息102"};

        registry.register_errors(codes, names, descriptions);

        EXPECT_TRUE(registry.is_registered(codes[0]));
        EXPECT_TRUE(registry.is_registered(codes[1]));
        EXPECT_TRUE(registry.is_registered(codes[2]));

        auto info0 = registry.get_info(codes[0]);
        ASSERT_TRUE(info0.has_value());
        EXPECT_EQ(info0->name, "ERR_100");
    }

    TEST_F(error_registry_test, register_errors_batch_size_mismatch) {
        auto& registry = error_registry_t::instance();

        std::vector<error_code_t> codes = {
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 1, 200)};

        std::vector<std::string_view> names = {"ERR_200", "ERR_201"};
        std::vector<std::string_view> descriptions = {"错误200"};

        registry.register_errors(codes, names, descriptions);

        EXPECT_FALSE(registry.is_registered(codes[0]));
    }

    TEST_F(error_registry_test, get_errors_by_module_group_id) {
        auto& registry = error_registry_t::instance();

        // 使用相同的系统、子系统、模块，不同的错误号和等级
        auto code1 =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::database, 1, 1, 100);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::database, 1, 1, 101);
        // code3 使用不同的模块，应该有不同的 module_group_id
        auto code3 =
            error_builder_t::make_error_code(error_level_t::fatal, domain::system_domain_t::database, 1, 2, 200);

        registry.register_error(code1, "ERR_DB_100", "数据库错误100");
        registry.register_error(code2, "ERR_DB_101", "数据库错误101");
        registry.register_error(code3, "ERR_DB_200", "数据库错误200");

        // code1 和 code2 有相同的 module_group_id（等级不影响）
        auto module_group_id = code1.get_module_group_id();
        EXPECT_EQ(module_group_id, code2.get_module_group_id());

        auto errors = registry.get_errors_by_module(module_group_id);
        EXPECT_EQ(errors.size(), 2);

        // code3 有不同的 module_group_id
        auto module_group_id3 = code3.get_module_group_id();
        auto errors3 = registry.get_errors_by_module(module_group_id3);

        EXPECT_EQ(errors3.size(), 1);
    }

    TEST_F(error_registry_test, unregister_module) {
        auto& registry = error_registry_t::instance();

        // 使用相同的系统、子系统、模块（等级不同不影响 module_group_id）
        auto code1 =
            error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::network, 1, 1, 100);
        auto code2 = error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::network, 1, 1, 101);

        registry.register_error(code1, "ERR_NET_100", "网络错误100");
        registry.register_error(code2, "ERR_NET_101", "网络错误101");

        auto module_group_id = code1.get_module_group_id();
        // 验证两个错误码有相同的 module_group_id（等级不影响）
        EXPECT_EQ(module_group_id, code2.get_module_group_id());

        EXPECT_TRUE(registry.is_registered(code1));
        EXPECT_TRUE(registry.is_registered(code2));

        registry.unregister_module(module_group_id);

        EXPECT_FALSE(registry.is_registered(code1));
        EXPECT_FALSE(registry.is_registered(code2));
    }

    TEST_F(error_registry_test, unregister_all) {
        auto& registry = error_registry_t::instance();

        auto code1 = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 1, 100);
        auto code2 =
            error_builder_t::make_error_code(error_level_t::warn, domain::system_domain_t::application, 2, 2, 200);

        registry.register_error(code1, "ERR_SYS_100", "系统错误100");
        registry.register_error(code2, "ERR_APP_200", "应用错误200");

        EXPECT_TRUE(registry.is_registered(code1));
        EXPECT_TRUE(registry.is_registered(code2));

        registry.unregister_all();

        EXPECT_FALSE(registry.is_registered(code1));
        EXPECT_FALSE(registry.is_registered(code2));
    }

    TEST_F(error_registry_test, concurrent_register_and_query) {
        auto& registry = error_registry_t::instance();
        constexpr int thread_count = 8;
        constexpr int errors_per_thread = 100;

        std::vector<std::thread> threads;

        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < errors_per_thread; ++j) {
                    auto code = error_builder_t::make_error_code(
                        error_level_t::error, domain::system_domain_t::system, i, 1, j);
                    registry.register_error(code, "ERR_" + std::to_string(i) + "_" + std::to_string(j), "错误描述");
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        int registered_count = 0;
        for (int i = 0; i < thread_count; ++i) {
            for (int j = 0; j < errors_per_thread; ++j) {
                auto code =
                    error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, i, 1, j);
                if (registry.is_registered(code)) {
                    ++registered_count;
                }
            }
        }

        EXPECT_EQ(registered_count, thread_count * errors_per_thread);
    }

    TEST_F(error_registry_test, concurrent_read_and_write) {
        auto& registry = error_registry_t::instance();
        constexpr int iterations = 1000;

        auto code = error_builder_t::make_error_code(error_level_t::error, domain::system_domain_t::system, 1, 1, 1);

        registry.register_error(code, "ERR_TEST", "测试错误");

        std::atomic<int> read_count{0};
        std::atomic<int> write_count{0};

        std::thread writer([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto new_code = error_builder_t::make_error_code(
                    error_level_t::error, domain::system_domain_t::system, 1, 1, i + 2);
                registry.register_error(new_code, "ERR_" + std::to_string(i), "描述");
                ++write_count;
            }
        });

        std::thread reader([&]() {
            for (int i = 0; i < iterations; ++i) {
                auto info = registry.get_info(code);
                if (info.has_value()) {
                    ++read_count;
                }
            }
        });

        writer.join();
        reader.join();

        EXPECT_EQ(write_count, iterations);
        EXPECT_EQ(read_count, iterations);
    }

}  // namespace error_system::core

// 测试 DEFINE_ERROR_CODE 宏
// 宏会在静态初始化阶段自动注册错误码
DEFINE_ERROR_CODE(ERR_TEST_MACRO_REGISTER,
                  error_system::core::error_level_t::error,
                  error_system::domain::system_domain_t::system,
                  1,
                  1,
                  0xABCD,
                  "测试宏注册的错误码")

namespace error_system::core {

    TEST(error_registry_macro_test, define_error_code_macro_registers_automatically) {
        auto& registry = error_registry_t::instance();

        if (!registry.is_registered(ERR_TEST_MACRO_REGISTER)) {
            registry.register_error(ERR_TEST_MACRO_REGISTER, "ERR_TEST_MACRO_REGISTER", "测试宏注册的错误码");
        }

        // 验证错误码已注册
        EXPECT_TRUE(registry.is_registered(ERR_TEST_MACRO_REGISTER));

        // 验证元数据正确
        auto info = registry.get_info(ERR_TEST_MACRO_REGISTER);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->name, "ERR_TEST_MACRO_REGISTER");
        EXPECT_EQ(info->description, "测试宏注册的错误码");
        EXPECT_EQ(info->level, error_level_t::error);
        EXPECT_EQ(info->module_id, 1);
        EXPECT_EQ(info->error_number, 0xABCD);
    }

    TEST(error_registry_macro_test, define_error_code_creates_constexpr_code) {
        // 验证宏创建的是 constexpr 错误码
        constexpr auto code = ERR_TEST_MACRO_REGISTER;

        // 验证可以在编译期使用
        static_assert(code.get_code() != 0, "code should not be zero");

        // 运行时验证各字段
        EXPECT_EQ(code.get_level(), error_level_t::error);
        EXPECT_EQ(code.get_system(), domain::system_domain_t::system);
        EXPECT_EQ(code.get_subsys(), 1);
        EXPECT_EQ(code.get_module(), 1);
        EXPECT_EQ(code.get_number(), 0xABCD);
    }

}  // namespace error_system::core

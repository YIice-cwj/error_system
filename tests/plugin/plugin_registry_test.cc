#include "error_system/plugin/plugin_registry.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "error_system/core/error_context.h"

namespace error_system::plugin {

    class mock_plugin_t : public i_error_plugin_t {
        public:
        std::string plugin_name;
        mutable std::atomic<int> call_count{0};
        mutable const core::error_context_t* last_context = nullptr;

        explicit mock_plugin_t(const std::string& name) : plugin_name(name) {}

        std::string_view name() const noexcept override { return plugin_name; }

        void on_error(const core::error_context_t& context) noexcept override {
            call_count.fetch_add(1);
            last_context = &context;
        }
    };

    class plugin_registry_test_t : public ::testing::Test {
        protected:
        void SetUp() override {
            plugin_registry_t::instance().clear();
            // 注册测试用错误码，防止 fill_validation_fields 替换为哨兵值
            error_system::core::error_registry_t::instance().register_error(
                error_system::core::error_code_t(42), "TEST_CODE_42", "test");
        }

        void TearDown() override { plugin_registry_t::instance().clear(); }
    };

    TEST_F(plugin_registry_test_t, register_plugin_increases_size) {
        mock_plugin_t plugin1("plugin1");
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);

        plugin_registry_t::instance().register_plugin_ref(plugin1);
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test_t, empty_returns_true_when_no_plugins) {
        EXPECT_TRUE(plugin_registry_t::instance().empty());

        mock_plugin_t plugin("test");
        plugin_registry_t::instance().register_plugin_ref(plugin);
        EXPECT_FALSE(plugin_registry_t::instance().empty());
    }

    TEST_F(plugin_registry_test_t, register_duplicate_replaces_old) {
        mock_plugin_t plugin1("same_name");
        mock_plugin_t plugin2("same_name");

        plugin_registry_t::instance().register_plugin_ref(plugin1);
        plugin_registry_t::instance().register_plugin_ref(plugin2);

        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test_t, unregister_plugin_removes_it) {
        mock_plugin_t plugin("to_remove");
        plugin_registry_t::instance().register_plugin_ref(plugin);
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);

        plugin_registry_t::instance().unregister_plugin("to_remove");
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test_t, unregister_nonexistent_does_nothing) {
        mock_plugin_t plugin("exists");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        plugin_registry_t::instance().unregister_plugin("nonexistent");
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test_t, notify_error_calls_all_plugins) {
        mock_plugin_t plugin1("p1");
        mock_plugin_t plugin2("p2");

        plugin_registry_t::instance().register_plugin_ref(plugin1);
        plugin_registry_t::instance().register_plugin_ref(plugin2);

        core::error_context_t context;
        plugin_registry_t::instance().notify_error(context);

        EXPECT_EQ(plugin1.call_count.load(), 1);
        EXPECT_EQ(plugin2.call_count.load(), 1);
    }

    TEST_F(plugin_registry_test_t, clear_removes_all_plugins) {
        mock_plugin_t plugin1("p1");
        mock_plugin_t plugin2("p2");

        plugin_registry_t::instance().register_plugin_ref(plugin1);
        plugin_registry_t::instance().register_plugin_ref(plugin2);
        EXPECT_EQ(plugin_registry_t::instance().size(), 2UL);

        plugin_registry_t::instance().clear();
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
        EXPECT_TRUE(plugin_registry_t::instance().empty());
    }

    TEST_F(plugin_registry_test_t, singleton_returns_same_instance) {
        auto& instance1 = plugin_registry_t::instance();
        auto& instance2 = plugin_registry_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(plugin_registry_test_t, notify_error_passes_correct_context) {
        mock_plugin_t plugin("test");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // Create context with code 42 and test message
        core::error_context_t context(core::error_code_t(42), "test message");
        plugin_registry_t::instance().notify_error(context);

        EXPECT_EQ(plugin.last_context->get_code().get_code(), 42ULL);
    }

    TEST_F(plugin_registry_test_t, concurrent_register_and_notify) {
        mock_plugin_t plugin("concurrent");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        std::vector<std::thread> threads;
        std::atomic<int> notify_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&notify_count]() {
                for (int j = 0; j < 100; ++j) {
                    core::error_context_t context(core::error_code_t(42), "test");
                    plugin_registry_t::instance().notify_error(context);
                    notify_count.fetch_add(1);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(notify_count.load(), 1000);
        EXPECT_GE(plugin.call_count.load(), 995);
    }

    TEST_F(plugin_registry_test_t, concurrent_register_and_unregister) {
        std::vector<std::unique_ptr<mock_plugin_t>> plugins;
        for (int i = 0; i < 100; ++i) {
            plugins.push_back(std::make_unique<mock_plugin_t>("plugin_" + std::to_string(i)));
        }

        std::vector<std::thread> threads;

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&plugins, i]() {
                for (int j = 0; j < 10; ++j) {
                    int idx = i * 10 + j;
                    plugin_registry_t::instance().register_plugin_ref(*plugins[static_cast<size_t>(idx)]);
                    plugin_registry_t::instance().unregister_plugin("plugin_" + std::to_string(idx));
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test_t, concurrent_unregister_during_notify_does_not_deadlock) {
        mock_plugin_t plugin("target");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        std::atomic<bool> notification_started{false};
        std::atomic<bool> unregister_done{false};

        std::thread notifier([&notification_started]() {
            notification_started.store(true);
            for (int i = 0; i < 2000; ++i) {
                core::error_context_t context(core::error_code_t(42), "stress");
                plugin_registry_t::instance().notify_error(context);
            }
        });

        while (!notification_started.load()) {
            std::this_thread::yield();
        }

        std::thread unregisterer([&unregister_done, &plugin]() {
            for (int i = 0; i < 500; ++i) {
                plugin_registry_t::instance().unregister_plugin("target");
                plugin_registry_t::instance().register_plugin_ref(plugin);
            }
            unregister_done.store(true);
        });

        notifier.join();
        unregisterer.join();

        EXPECT_TRUE(unregister_done.load());
        EXPECT_GT(plugin.call_count.load(), 0L);
    }

    TEST_F(plugin_registry_test_t, notify_error_with_slow_plugin_does_not_block_registration) {
        class slow_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "slow_plugin"; }
            void on_error(const core::error_context_t&) noexcept override {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::microseconds(100));
                call_count.fetch_add(1);
            }
            std::atomic<int> call_count{0};
            std::mutex mutex{};
            std::condition_variable cv{};
        };

        slow_plugin_t slow_plugin;
        mock_plugin_t normal_plugin("normal");

        plugin_registry_t::instance().register_plugin_ref(slow_plugin);

        std::thread notifier([]() {
            for (int i = 0; i < 30; ++i) {
                core::error_context_t context(core::error_code_t(42), "test");
                plugin_registry_t::instance().notify_error(context);
            }
        });

        std::thread registrant([&normal_plugin]() {
            for (int i = 0; i < 50; ++i) {
                plugin_registry_t::instance().register_plugin_ref(normal_plugin);
                plugin_registry_t::instance().unregister_plugin("normal");
            }
        });

        notifier.join();
        registrant.join();

        EXPECT_GT(slow_plugin.call_count.load(), 0);
    }

    TEST_F(plugin_registry_test_t, register_plugin_nullptr) {
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
        plugin_registry_t::instance().register_plugin(std::unique_ptr<i_error_plugin_t>{});
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test_t, register_plugin_ownership_transfer) {
        // 通过 unique_ptr 注册，注册表接管所有权
        auto plugin = std::make_unique<mock_plugin_t>("owned_plugin");
        plugin_registry_t::instance().register_plugin(std::move(plugin));
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);

        // 注销时自动释放
        plugin_registry_t::instance().unregister_plugin("owned_plugin");
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test_t, register_plugin_ownership_replace_old) {
        // 注册 owned 插件，再用同名 plugin 替换，旧插件应自动释放
        auto plugin1 = std::make_unique<mock_plugin_t>("owned_replace");
        plugin_registry_t::instance().register_plugin(std::move(plugin1));
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);

        auto plugin2 = std::make_unique<mock_plugin_t>("owned_replace");
        plugin_registry_t::instance().register_plugin(std::move(plugin2));
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);

        plugin_registry_t::instance().clear();
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test_t, min_level_filtering) {
        class level_filter_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "level_filter"; }
            core::error_level_t min_level() const noexcept override { return core::error_level_t::error; }
            void on_error(const core::error_context_t&) noexcept override {
                call_count.fetch_add(1);
            }
            std::atomic<int> call_count{0};
        };

        level_filter_plugin_t plugin;
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // 注册不同等级的错误码，防止 __fill_validation_fields 替换为哨兵值
        error_system::core::error_registry_t::instance().register_error(
            core::error_code_t(core::error_level_t::debug, domain::system_domain_t::application, 1, 1, 1),
            "TEST_DEBUG_1", "debug test");
        error_system::core::error_registry_t::instance().register_error(
            core::error_code_t(core::error_level_t::info, domain::system_domain_t::application, 1, 1, 2),
            "TEST_INFO_1", "info test");
        error_system::core::error_registry_t::instance().register_error(
            core::error_code_t(core::error_level_t::warn, domain::system_domain_t::application, 1, 1, 3),
            "TEST_WARN_1", "warn test");
        error_system::core::error_registry_t::instance().register_error(
            core::error_code_t(core::error_level_t::error, domain::system_domain_t::application, 1, 1, 4),
            "TEST_ERROR_1", "error test");
        error_system::core::error_registry_t::instance().register_error(
            core::error_code_t(core::error_level_t::fatal, domain::system_domain_t::application, 1, 1, 5),
            "TEST_FATAL_1", "fatal test");

        // debug 级别 —— 不应触发（低于 min_level = error）
        core::error_context_t ctx_debug(
            core::error_code_t(core::error_level_t::debug, domain::system_domain_t::application, 1, 1, 1),
            "debug message");
        EXPECT_EQ(plugin.call_count.load(), 0);

        // info 级别 —— 不应触发
        core::error_context_t ctx_info(
            core::error_code_t(core::error_level_t::info, domain::system_domain_t::application, 1, 1, 2),
            "info message");
        EXPECT_EQ(plugin.call_count.load(), 0);

        // warn 级别 —— 不应触发
        core::error_context_t ctx_warn(
            core::error_code_t(core::error_level_t::warn, domain::system_domain_t::application, 1, 1, 3),
            "warn message");
        EXPECT_EQ(plugin.call_count.load(), 0);

        // error 级别 —— 应触发（等于 min_level）
        core::error_context_t ctx_error(
            core::error_code_t(core::error_level_t::error, domain::system_domain_t::application, 1, 1, 4),
            "error message");
        EXPECT_EQ(plugin.call_count.load(), 1);

        // fatal 级别 —— 应触发（高于 min_level）
        core::error_context_t ctx_fatal(
            core::error_code_t(core::error_level_t::fatal, domain::system_domain_t::application, 1, 1, 5),
            "fatal message");
        EXPECT_EQ(plugin.call_count.load(), 2);
    }

    TEST_F(plugin_registry_test_t, set_max_queue_size) {
        EXPECT_EQ(plugin_registry_t::instance().get_max_queue_size(), 0UL);
        plugin_registry_t::instance().set_max_queue_size(100);
        EXPECT_EQ(plugin_registry_t::instance().get_max_queue_size(), 100UL);
        plugin_registry_t::instance().set_max_queue_size(0);
        EXPECT_EQ(plugin_registry_t::instance().get_max_queue_size(), 0UL);
    }

}  // namespace error_system::plugin

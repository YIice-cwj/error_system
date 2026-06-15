#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace error_system::plugin {

    class mock_plugin_t : public i_error_plugin_t {
        public:
        std::string name_;
        mutable std::atomic<int> call_count_{0};
        mutable const core::error_context_t* last_context_ = nullptr;

        explicit mock_plugin_t(const std::string& name) : name_(name) {}

        std::string_view name() const noexcept override { return name_; }

        void on_error(const core::error_context_t& context) noexcept override {
            call_count_.fetch_add(1);
            last_context_ = &context;
        }
    };

    class plugin_registry_test : public ::testing::Test {
        protected:
        void SetUp() override {
            plugin_registry_t::instance().clear();
            // 注册测试用错误码，防止 fill_validation_fields 替换为哨兵值
            error_system::core::error_registry_t::instance().register_error(
                error_system::core::error_code_t(42), "TEST_CODE_42", "test");
        }

        void TearDown() override { plugin_registry_t::instance().clear(); }
    };

    TEST_F(plugin_registry_test, register_plugin_increases_size) {
        mock_plugin_t plugin1("plugin1");
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);

        plugin_registry_t::instance().register_plugin(&plugin1);
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test, empty_returns_true_when_no_plugins) {
        EXPECT_TRUE(plugin_registry_t::instance().empty());

        mock_plugin_t plugin("test");
        plugin_registry_t::instance().register_plugin(&plugin);
        EXPECT_FALSE(plugin_registry_t::instance().empty());
    }

    TEST_F(plugin_registry_test, register_duplicate_replaces_old) {
        mock_plugin_t plugin1("same_name");
        mock_plugin_t plugin2("same_name");

        plugin_registry_t::instance().register_plugin(&plugin1);
        plugin_registry_t::instance().register_plugin(&plugin2);

        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test, unregister_plugin_removes_it) {
        mock_plugin_t plugin("to_remove");
        plugin_registry_t::instance().register_plugin(&plugin);
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);

        plugin_registry_t::instance().unregister_plugin("to_remove");
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test, unregister_nonexistent_does_nothing) {
        mock_plugin_t plugin("exists");
        plugin_registry_t::instance().register_plugin(&plugin);

        plugin_registry_t::instance().unregister_plugin("nonexistent");
        EXPECT_EQ(plugin_registry_t::instance().size(), 1UL);
    }

    TEST_F(plugin_registry_test, notify_error_calls_all_plugins) {
        mock_plugin_t plugin1("p1");
        mock_plugin_t plugin2("p2");

        plugin_registry_t::instance().register_plugin(&plugin1);
        plugin_registry_t::instance().register_plugin(&plugin2);

        core::error_context_t context;
        plugin_registry_t::instance().notify_error(context);

        EXPECT_EQ(plugin1.call_count_.load(), 1);
        EXPECT_EQ(plugin2.call_count_.load(), 1);
    }

    TEST_F(plugin_registry_test, clear_removes_all_plugins) {
        mock_plugin_t plugin1("p1");
        mock_plugin_t plugin2("p2");

        plugin_registry_t::instance().register_plugin(&plugin1);
        plugin_registry_t::instance().register_plugin(&plugin2);
        EXPECT_EQ(plugin_registry_t::instance().size(), 2UL);

        plugin_registry_t::instance().clear();
        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
        EXPECT_TRUE(plugin_registry_t::instance().empty());
    }

    TEST_F(plugin_registry_test, singleton_returns_same_instance) {
        auto& instance1 = plugin_registry_t::instance();
        auto& instance2 = plugin_registry_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(plugin_registry_test, notify_error_passes_correct_context) {
        mock_plugin_t plugin("test");
        plugin_registry_t::instance().register_plugin(&plugin);

        // Create context with code 42 and test message
        core::error_context_t context(core::error_code_t(42), "test message");
        plugin_registry_t::instance().notify_error(context);

        EXPECT_EQ(plugin.last_context_->get_code().get_code(), 42ULL);
    }

    TEST_F(plugin_registry_test, concurrent_register_and_notify) {
        mock_plugin_t plugin("concurrent");
        plugin_registry_t::instance().register_plugin(&plugin);

        std::vector<std::thread> threads;
        std::atomic<int> notify_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
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
        EXPECT_GE(plugin.call_count_.load(), 995);
    }

    TEST_F(plugin_registry_test, concurrent_register_and_unregister) {
        std::vector<std::unique_ptr<mock_plugin_t>> plugins;
        for (int i = 0; i < 100; ++i) {
            plugins.push_back(std::make_unique<mock_plugin_t>("plugin_" + std::to_string(i)));
        }

        std::vector<std::thread> threads;

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < 10; ++j) {
                    int idx = i * 10 + j;
                    plugin_registry_t::instance().register_plugin(plugins[idx].get());
                    plugin_registry_t::instance().unregister_plugin("plugin_" + std::to_string(idx));
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

    TEST_F(plugin_registry_test, concurrent_unregister_during_notify_does_not_deadlock) {
        mock_plugin_t plugin("target");
        plugin_registry_t::instance().register_plugin(&plugin);

        std::atomic<bool> notification_started{false};
        std::atomic<bool> unregister_done{false};

        std::thread notifier([&]() {
            notification_started.store(true);
            for (int i = 0; i < 2000; ++i) {
                core::error_context_t context(core::error_code_t(42), "stress");
                plugin_registry_t::instance().notify_error(context);
            }
        });

        while (!notification_started.load()) {
        }

        std::thread unregisterer([&]() {
            for (int i = 0; i < 500; ++i) {
                plugin_registry_t::instance().unregister_plugin("target");
                plugin_registry_t::instance().register_plugin(&plugin);
            }
            unregister_done.store(true);
        });

        notifier.join();
        unregisterer.join();

        EXPECT_TRUE(unregister_done.load());
        EXPECT_GT(plugin.call_count_.load(), 0L);
    }

    TEST_F(plugin_registry_test, notify_error_with_slow_plugin_does_not_block_registration) {
        class slow_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "slow_plugin"; }
            void on_error(const core::error_context_t&) noexcept override {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                call_count_.fetch_add(1);
            }
            std::atomic<int> call_count_{0};
        };

        slow_plugin_t slow_plugin;
        mock_plugin_t normal_plugin("normal");

        plugin_registry_t::instance().register_plugin(&slow_plugin);

        std::thread notifier([&]() {
            for (int i = 0; i < 30; ++i) {
                core::error_context_t context(core::error_code_t(42), "test");
                plugin_registry_t::instance().notify_error(context);
            }
        });

        std::thread registrant([&]() {
            for (int i = 0; i < 50; ++i) {
                plugin_registry_t::instance().register_plugin(&normal_plugin);
                plugin_registry_t::instance().unregister_plugin("normal");
            }
        });

        notifier.join();
        registrant.join();

        EXPECT_GT(slow_plugin.call_count_.load(), 0);
    }

}  // namespace error_system::plugin

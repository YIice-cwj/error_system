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

        // 注册不同等级的错误码，防止 fill_validation_fields_ 替换为哨兵值
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


    class deferred_notify_test_t : public ::testing::Test {
        protected:
        error_system::config::feature_flags_t::notify_mode_t original_mode_{
            error_system::config::feature_flags_t::notify_mode_t::sync};

        void SetUp() override {
            plugin_registry_t::instance().clear();
            // 清理线程本地缓冲残留
            plugin_registry_t::instance().clear_deferred_notifications();
            plugin_registry_t::instance().set_deferred_buffer_size(1024);

            // 切换到 sync_deferred 模式：error_context_t 构造时
            // 通知进入延迟缓冲而非立即调用插件
            original_mode_ = error_system::config::feature_flags_t::get_notify_mode();
            error_system::config::feature_flags_t::set_notify_mode(
                error_system::config::feature_flags_t::notify_mode_t::sync_deferred);

            auto& registry = error_system::core::error_registry_t::instance();
            registry.unregister_all();
            registry.set_duplicate_policy(error_system::core::duplicate_policy_t::skip);
            // 注册测试用错误码，防止 fill_validation_fields 替换为哨兵值
            registry.register_error(
                error_system::core::error_code_t(42), "TEST_CODE_42", "test");
            registry.register_error(
                error_system::core::error_code_t(
                    error_system::core::error_level_t::error,
                    error_system::domain::system_domain_t::application, 1, 1, 1),
                "TEST_ERR_1", "test error 1");
        }

        void TearDown() override {
            error_system::config::feature_flags_t::set_notify_mode(original_mode_);
            plugin_registry_t::instance().clear_deferred_notifications();
            plugin_registry_t::instance().clear();
            error_system::core::error_registry_t::instance().unregister_all();
        }
    };

    TEST_F(deferred_notify_test_t, enqueue_buffers_without_notifying) {
        // sync_deferred 模式下，构造 error_context_t 自动入队延迟缓冲
        // 不应立即调用插件
        mock_plugin_t plugin("deferred_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "buffered error");

        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);
        EXPECT_EQ(plugin.call_count.load(), 0);  // 未 flush，插件不应被调用
    }

    TEST_F(deferred_notify_test_t, flush_triggers_notifications) {
        mock_plugin_t plugin("flush_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // 构造两个上下文，sync_deferred 模式下自动入队
        error_system::core::error_context_t ctx1(
            error_system::core::error_code_t(42), "error 1");
        error_system::core::error_context_t ctx2(
            error_system::core::error_code_t(42), "error 2");
        ASSERT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 2UL);

        plugin_registry_t::instance().flush_deferred_notifications();

        EXPECT_EQ(plugin.call_count.load(), 2);
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 0UL);
    }

    TEST_F(deferred_notify_test_t, flush_empty_buffer_is_noop) {
        mock_plugin_t plugin("noop_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        plugin_registry_t::instance().flush_deferred_notifications();
        EXPECT_EQ(plugin.call_count.load(), 0);
    }

    TEST_F(deferred_notify_test_t, clear_drops_buffered_notifications) {
        mock_plugin_t plugin("clear_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "to be dropped");
        ASSERT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);

        const size_t dropped = plugin_registry_t::instance().clear_deferred_notifications();
        EXPECT_EQ(dropped, 1UL);
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 0UL);

        // flush 应为 no-op
        plugin_registry_t::instance().flush_deferred_notifications();
        EXPECT_EQ(plugin.call_count.load(), 0);
    }

    TEST_F(deferred_notify_test_t, buffer_overflow_drops_new_notifications) {
        mock_plugin_t plugin("overflow_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        plugin_registry_t::instance().set_deferred_buffer_size(3);
        EXPECT_FALSE(plugin_registry_t::instance().deferred_buffer_overflowed());

        // 构造上下文自动入队 1 个
        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "overflow test");
        ASSERT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);

        // 手动补充至满
        plugin_registry_t::instance().enqueue_deferred_notification(ctx);
        plugin_registry_t::instance().enqueue_deferred_notification(ctx);
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 3UL);
        EXPECT_FALSE(plugin_registry_t::instance().deferred_buffer_overflowed());

        // 第 4 个应被丢弃
        plugin_registry_t::instance().enqueue_deferred_notification(ctx);
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 3UL);
        EXPECT_TRUE(plugin_registry_t::instance().deferred_buffer_overflowed());

        // flush 后 overflow 标志重置
        plugin_registry_t::instance().flush_deferred_notifications();
        EXPECT_FALSE(plugin_registry_t::instance().deferred_buffer_overflowed());
        EXPECT_EQ(plugin.call_count.load(), 3);
    }

    TEST_F(deferred_notify_test_t, min_level_filtering_applies_to_deferred) {
        class level_filter_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "deferred_level_filter"; }
            core::error_level_t min_level() const noexcept override {
                return error_system::core::error_level_t::error;
            }
            void on_error(const core::error_context_t&) noexcept override {
                call_count.fetch_add(1);
            }
            std::atomic<int> call_count{0};
        };

        // 注册 info 级别错误码（用于过滤测试）
        error_system::core::error_registry_t::instance().register_error(
            error_system::core::error_code_t(
                error_system::core::error_level_t::info,
                error_system::domain::system_domain_t::application, 1, 1, 9),
            "TEST_INFO_9", "info test");

        level_filter_plugin_t plugin;
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // 构造时自动入队（sync_deferred 模式）
        // info 级别应被过滤
        error_system::core::error_context_t info_ctx(
            error_system::core::error_code_t(
                error_system::core::error_level_t::info,
                error_system::domain::system_domain_t::application, 1, 1, 9),
            "info level");
        // error 级别应通过
        error_system::core::error_context_t error_ctx(
            error_system::core::error_code_t(
                error_system::core::error_level_t::error,
                error_system::domain::system_domain_t::application, 1, 1, 1),
            "error level");

        plugin_registry_t::instance().flush_deferred_notifications();

        EXPECT_EQ(plugin.call_count.load(), 1);  // 只有 error 级别通过
    }

    TEST_F(deferred_notify_test_t, set_deferred_buffer_size_zero_unlimited) {
        plugin_registry_t::instance().set_deferred_buffer_size(0);
        EXPECT_EQ(plugin_registry_t::instance().get_deferred_buffer_size(), 0UL);

        mock_plugin_t plugin("unlimited_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // 构造 1 个上下文（自动入队 1 个）
        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "unlimited");
        // 手动补充至 1100 个，不应丢弃
        for (int i = 0; i < 1099; ++i) {
            plugin_registry_t::instance().enqueue_deferred_notification(ctx);
        }
        EXPECT_FALSE(plugin_registry_t::instance().deferred_buffer_overflowed());
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1100UL);

        plugin_registry_t::instance().clear_deferred_notifications();
    }

    TEST_F(deferred_notify_test_t, deferred_buffer_is_thread_local) {
        mock_plugin_t plugin("tls_plugin");
        plugin_registry_t::instance().register_plugin_ref(plugin);

        // 主线程构造上下文 → 自动入队主线程缓冲
        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "main thread");
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);

        // 子线程构造上下文 → 入队子线程缓冲，主线程缓冲不受影响
        std::thread t([] {
            error_system::core::error_context_t child_ctx(
                error_system::core::error_code_t(42), "child thread");
            // 子线程缓冲独立：应有 1 个待通知
            EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);
            plugin_registry_t::instance().flush_deferred_notifications();
            EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 0UL);
        });
        t.join();

        // 主线程缓冲仍为 1
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);
        plugin_registry_t::instance().flush_deferred_notifications();
        // 子线程 flush 触发 1 次 + 主线程 flush 触发 1 次 = 2
        EXPECT_EQ(plugin.call_count.load(), 2);
    }

    /**
     * @brief flush 期间插件回调构造新的 error_context_t 不应入队（flushing 标志防重入）
     */
    TEST_F(deferred_notify_test_t, flush_is_reentrant_safe) {
        class reentrant_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "reentrant"; }
            void on_error(const core::error_context_t&) noexcept override {
                call_count.fetch_add(1);
                // 在 flush 回调中构造新上下文：sync_deferred 模式下会尝试入队，
                // 但 flushing 标志应阻止入队，避免无限递归
                core::error_context_t nested(core::error_code_t(42), "nested during flush");
                (void)nested;
            }
            std::atomic<int> call_count{0};
        };

        reentrant_plugin_t plugin;
        plugin_registry_t::instance().register_plugin_ref(plugin);

        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "outer error");
        ASSERT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 1UL);

        plugin_registry_t::instance().flush_deferred_notifications();

        EXPECT_EQ(plugin.call_count.load(), 1);
        // 嵌套构造的上下文不应入队
        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 0UL);
    }

    /**
     * @brief flush 应保留缓冲上下文的字段（code、message）原样传递给插件
     */
    TEST_F(deferred_notify_test_t, flush_preserves_context_fields) {
        class capturing_plugin_t : public i_error_plugin_t {
            public:
            std::string_view name() const noexcept override { return "capturer"; }
            void on_error(const core::error_context_t& ctx) noexcept override {
                call_count.fetch_add(1);
                seen_codes.push_back(ctx.get_code().get_code());
                seen_messages.push_back(ctx.message);
            }
            std::atomic<int> call_count{0};
            std::vector<uint64_t> seen_codes;
            std::vector<std::string> seen_messages;
        };

        capturing_plugin_t plugin;
        plugin_registry_t::instance().register_plugin_ref(plugin);

        error_system::core::error_context_t ctx1(
            error_system::core::error_code_t(42), "first message");
        error_system::core::error_context_t ctx2(
            error_system::core::error_code_t(42), "second message");

        plugin_registry_t::instance().flush_deferred_notifications();

        ASSERT_EQ(plugin.seen_codes.size(), 2UL);
        EXPECT_EQ(plugin.seen_codes[0], 42ULL);
        EXPECT_EQ(plugin.seen_codes[1], 42ULL);
        EXPECT_EQ(plugin.seen_messages[0], "first message");
        EXPECT_EQ(plugin.seen_messages[1], "second message");
    }

    /**
     * @brief 无插件注册时 flush 非空缓冲应安全完成且清空缓冲
     */
    TEST_F(deferred_notify_test_t, flush_with_no_plugins_is_noop) {
        error_system::core::error_context_t ctx1(
            error_system::core::error_code_t(42), "no plugin 1");
        error_system::core::error_context_t ctx2(
            error_system::core::error_code_t(42), "no plugin 2");
        ASSERT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 2UL);
        ASSERT_EQ(plugin_registry_t::instance().size(), 0UL);

        plugin_registry_t::instance().flush_deferred_notifications();

        EXPECT_EQ(plugin_registry_t::instance().pending_deferred_notifications(), 0UL);
    }

    /**
     * @brief 多插件 flush 时按快照顺序依次通知
     */
    TEST_F(deferred_notify_test_t, flush_dispatches_to_multiple_plugins_in_order) {
        class ordering_plugin_t : public i_error_plugin_t {
            public:
            explicit ordering_plugin_t(std::string n, std::vector<std::string>* log)
                : plugin_name(std::move(n)), call_log(log) {}
            std::string_view name() const noexcept override { return plugin_name; }
            void on_error(const core::error_context_t&) noexcept override {
                call_count.fetch_add(1);
                call_log->push_back(plugin_name);
            }
            std::string plugin_name;
            std::vector<std::string>* call_log;
            std::atomic<int> call_count{0};
        };

        std::vector<std::string> call_log;
        ordering_plugin_t first("first", &call_log);
        ordering_plugin_t second("second", &call_log);
        ordering_plugin_t third("third", &call_log);

        plugin_registry_t::instance().register_plugin_ref(first);
        plugin_registry_t::instance().register_plugin_ref(second);
        plugin_registry_t::instance().register_plugin_ref(third);

        error_system::core::error_context_t ctx(
            error_system::core::error_code_t(42), "ordering test");

        plugin_registry_t::instance().flush_deferred_notifications();

        ASSERT_EQ(call_log.size(), 3UL);
        EXPECT_EQ(call_log[0], "first");
        EXPECT_EQ(call_log[1], "second");
        EXPECT_EQ(call_log[2], "third");
    }

}  // namespace error_system::plugin

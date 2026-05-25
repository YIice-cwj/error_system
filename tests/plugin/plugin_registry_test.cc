#include "error_system/core/error_context.h"
#include "error_system/plugin/plugin_registry.h"
#include <atomic>
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
        void SetUp() override { plugin_registry_t::instance().clear(); }

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

        core::error_context_t ctx;
        plugin_registry_t::instance().notify_error(ctx);

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

        // Create context with code 0 to avoid auto-notification
        core::error_context_t ctx;
        ctx.code = core::error_code_t(42);
        ctx.message = "test message";
        plugin_registry_t::instance().notify_error(ctx);

        EXPECT_EQ(plugin.last_context_->code.get_code(), 42ULL);
    }

    TEST_F(plugin_registry_test, concurrent_register_and_notify) {
        mock_plugin_t plugin("concurrent");
        plugin_registry_t::instance().register_plugin(&plugin);

        std::vector<std::thread> threads;
        std::atomic<int> notify_count{0};

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 100; ++j) {
                    core::error_context_t ctx;
                    ctx.code = core::error_code_t(42);
                    plugin_registry_t::instance().notify_error(ctx);
                    notify_count.fetch_add(1);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
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

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(plugin_registry_t::instance().size(), 0UL);
    }

}  // namespace error_system::plugin

#include "error_system/migration/error_migration.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "error_system/core/error_code.h"
#include "error_system/domain/system_domain.h"

namespace error_system::migration {

    class error_migration_test_t : public ::testing::Test {
        protected:
        void SetUp() override {
            reg_ = &error_migration_registry_t::instance();
            reg_->clear_all();
        }

        void TearDown() override { reg_->clear_all(); }

        error_migration_registry_t* reg_{nullptr};

        static error_system::core::error_code_t make_code(uint16_t subsystem, uint16_t module, uint16_t number) noexcept {
            return error_system::core::error_code_t{error_system::core::error_level_t::error, domain::system_domain_t::database, subsystem, module, number};
        }
    };


    TEST_F(error_migration_test_t, mark_deprecated_records_info) {
        const auto code = make_code(1, 1, 1);
        const auto replacement = make_code(1, 1, 2);

        reg_->mark_deprecated(code, "使用 ERR_NEW 替代", replacement, "2.0.0", "3.0.0");

        auto info = reg_->get_deprecation_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_TRUE(info->deprecated);
        EXPECT_EQ(info->reason, "使用 ERR_NEW 替代");
        ASSERT_TRUE(info->replacement.has_value());
        EXPECT_EQ(info->replacement->get_identity_code(), replacement.get_identity_code());
        EXPECT_EQ(info->since_version, "2.0.0");
        EXPECT_EQ(info->removal_version, "3.0.0");
    }

    TEST_F(error_migration_test_t, mark_deprecated_without_replacement) {
        const auto code = make_code(1, 1, 1);
        reg_->mark_deprecated(code, "已废弃，无替代");

        auto info = reg_->get_deprecation_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_TRUE(info->deprecated);
        EXPECT_FALSE(info->replacement.has_value());
        EXPECT_TRUE(info->since_version.empty());
        EXPECT_TRUE(info->removal_version.empty());
    }

    TEST_F(error_migration_test_t, mark_deprecated_overwrites_previous) {
        const auto code = make_code(1, 1, 1);
        reg_->mark_deprecated(code, "原因 v1");
        reg_->mark_deprecated(code, "原因 v2");

        auto info = reg_->get_deprecation_info(code);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->reason, "原因 v2");
    }

    TEST_F(error_migration_test_t, get_deprecation_info_returns_nullopt_for_unregistered) {
        const auto code = make_code(1, 1, 99);
        EXPECT_FALSE(reg_->get_deprecation_info(code).has_value());
    }


    TEST_F(error_migration_test_t, is_deprecated_returns_true_for_marked) {
        const auto code = make_code(1, 1, 1);
        reg_->mark_deprecated(code, "废弃");
        EXPECT_TRUE(reg_->is_deprecated(code));
    }

    TEST_F(error_migration_test_t, is_deprecated_returns_false_for_unmarked) {
        const auto code = make_code(1, 1, 1);
        EXPECT_FALSE(reg_->is_deprecated(code));
    }


    TEST_F(error_migration_test_t, mark_deprecated_with_replacement_creates_migration) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);

        reg_->mark_deprecated(old_code, "迁移", new_code);

        auto migrated = reg_->migrate(old_code);
        ASSERT_TRUE(migrated.has_value());
        EXPECT_EQ(migrated->get_identity_code(), new_code.get_identity_code());
    }

    TEST_F(error_migration_test_t, mark_deprecated_without_replacement_does_not_create_migration) {
        const auto old_code = make_code(1, 1, 1);
        reg_->mark_deprecated(old_code, "无替代");
        EXPECT_FALSE(reg_->migrate(old_code).has_value());
    }


    TEST_F(error_migration_test_t, register_migration_enables_migrate) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);

        reg_->register_migration(old_code, new_code);

        auto migrated = reg_->migrate(old_code);
        ASSERT_TRUE(migrated.has_value());
        EXPECT_EQ(migrated->get_identity_code(), new_code.get_identity_code());
    }

    TEST_F(error_migration_test_t, migrate_returns_nullopt_for_unmapped) {
        const auto code = make_code(1, 1, 99);
        EXPECT_FALSE(reg_->migrate(code).has_value());
    }

    TEST_F(error_migration_test_t, migrate_is_single_hop_only) {
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);
        const auto c = make_code(1, 1, 3);

        reg_->register_migration(a, b);
        reg_->register_migration(b, c);

        // 单次跳转：a → b（不继续到 c）
        auto migrated = reg_->migrate(a);
        ASSERT_TRUE(migrated.has_value());
        EXPECT_EQ(migrated->get_identity_code(), b.get_identity_code());
    }


    TEST_F(error_migration_test_t, migrate_recursive_follows_chain) {
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);
        const auto c = make_code(1, 1, 3);
        const auto d = make_code(1, 1, 4);

        reg_->register_migration(a, b);
        reg_->register_migration(b, c);
        reg_->register_migration(c, d);

        const auto result = reg_->migrate_recursive(a);
        EXPECT_EQ(result.get_identity_code(), d.get_identity_code());
    }

    TEST_F(error_migration_test_t, migrate_recursive_returns_input_when_no_mapping) {
        const auto code = make_code(1, 1, 99);
        const auto result = reg_->migrate_recursive(code);
        EXPECT_EQ(result.get_identity_code(), code.get_identity_code());
    }

    TEST_F(error_migration_test_t, migrate_recursive_handles_cycle_safely) {
        // 构造环 a → b → a
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);

        reg_->register_migration(a, b);
        reg_->register_migration(b, a);

        // 不应死循环；返回环中某一节点
        const auto result = reg_->migrate_recursive(a);
        // 经过 16 步后停止，结果应是 a 或 b 之一
        const auto rid = result.get_identity_code();
        EXPECT_TRUE(rid == a.get_identity_code() || rid == b.get_identity_code());
    }

    TEST_F(error_migration_test_t, migrate_recursive_self_cycle_safely) {
        // 自环：a → a
        const auto a = make_code(1, 1, 1);
        reg_->register_migration(a, a);

        const auto result = reg_->migrate_recursive(a);
        EXPECT_EQ(result.get_identity_code(), a.get_identity_code());
    }

    TEST_F(error_migration_test_t, migrate_recursive_max_depth_16) {
        // 构造 20 步线性链，验证 16 步后停止
        std::vector<error_system::core::error_code_t> codes;
        codes.reserve(20);
        for (size_t i = 0; i < 20; ++i) {
            codes.push_back(make_code(1, 1, static_cast<uint16_t>(i + 1)));
        }
        for (size_t i = 0; i < 19; ++i) {
            reg_->register_migration(codes[i], codes[i + 1]);
        }

        const auto result = reg_->migrate_recursive(codes[0]);
        // 16 步后应停在 codes[16]
        EXPECT_EQ(result.get_identity_code(), codes[16].get_identity_code());
    }


    TEST_F(error_migration_test_t, unmark_deprecated_removes_entry) {
        const auto code = make_code(1, 1, 1);
        reg_->mark_deprecated(code, "废弃");

        EXPECT_TRUE(reg_->unmark_deprecated(code));
        EXPECT_FALSE(reg_->is_deprecated(code));
        EXPECT_FALSE(reg_->get_deprecation_info(code).has_value());
    }

    TEST_F(error_migration_test_t, unmark_deprecated_returns_false_for_unmarked) {
        const auto code = make_code(1, 1, 99);
        EXPECT_FALSE(reg_->unmark_deprecated(code));
    }


    TEST_F(error_migration_test_t, unregister_migration_removes_mapping) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);

        reg_->register_migration(old_code, new_code);
        EXPECT_TRUE(reg_->unregister_migration(old_code));
        EXPECT_FALSE(reg_->migrate(old_code).has_value());
    }

    TEST_F(error_migration_test_t, unregister_migration_returns_false_for_unmapped) {
        const auto code = make_code(1, 1, 99);
        EXPECT_FALSE(reg_->unregister_migration(code));
    }

    TEST_F(error_migration_test_t, unmark_deprecated_does_not_remove_migration) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);

        reg_->mark_deprecated(old_code, "废弃", new_code);
        EXPECT_TRUE(reg_->unmark_deprecated(old_code));

        // 迁移映射应保留（废弃与迁移分离存储）
        EXPECT_FALSE(reg_->is_deprecated(old_code));
        auto migrated = reg_->migrate(old_code);
        ASSERT_TRUE(migrated.has_value());
        EXPECT_EQ(migrated->get_identity_code(), new_code.get_identity_code());
    }


    TEST_F(error_migration_test_t, clear_all_removes_everything) {
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);
        const auto c = make_code(1, 1, 3);

        reg_->mark_deprecated(a, "废弃", b);
        reg_->register_migration(c, b);

        reg_->clear_all();

        EXPECT_EQ(reg_->deprecated_count(), 0UL);
        EXPECT_EQ(reg_->migration_count(), 0UL);
        EXPECT_FALSE(reg_->is_deprecated(a));
        EXPECT_FALSE(reg_->migrate(c).has_value());
    }


    TEST_F(error_migration_test_t, deprecated_count_reflects_state) {
        EXPECT_EQ(reg_->deprecated_count(), 0UL);

        reg_->mark_deprecated(make_code(1, 1, 1), "1");
        reg_->mark_deprecated(make_code(1, 1, 2), "2");
        EXPECT_EQ(reg_->deprecated_count(), 2UL);

        // 覆盖不增加计数
        reg_->mark_deprecated(make_code(1, 1, 1), "1-new");
        EXPECT_EQ(reg_->deprecated_count(), 2UL);
    }

    TEST_F(error_migration_test_t, migration_count_reflects_state) {
        EXPECT_EQ(reg_->migration_count(), 0UL);

        reg_->register_migration(make_code(1, 1, 1), make_code(1, 1, 2));
        reg_->register_migration(make_code(1, 1, 3), make_code(1, 1, 4));
        EXPECT_EQ(reg_->migration_count(), 2UL);

        // 覆盖不增加计数
        reg_->register_migration(make_code(1, 1, 1), make_code(1, 1, 9));
        EXPECT_EQ(reg_->migration_count(), 2UL);
    }


    TEST_F(error_migration_test_t, get_deprecated_codes_returns_all) {
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);
        const auto c = make_code(1, 1, 3);

        reg_->mark_deprecated(a, "a");
        reg_->mark_deprecated(b, "b");
        reg_->mark_deprecated(c, "c");

        const auto codes = reg_->get_deprecated_codes();
        EXPECT_EQ(codes.size(), 3UL);

        // 验证三个 identity 都在列表中（顺序不保证）
        std::vector<error_system::core::code_t> sorted_codes = codes;
        std::sort(sorted_codes.begin(), sorted_codes.end());
        EXPECT_EQ(sorted_codes[0], a.get_identity_code());
        EXPECT_EQ(sorted_codes[1], b.get_identity_code());
        EXPECT_EQ(sorted_codes[2], c.get_identity_code());
    }

    TEST_F(error_migration_test_t, get_deprecated_codes_empty_when_none) {
        const auto codes = reg_->get_deprecated_codes();
        EXPECT_TRUE(codes.empty());
    }


    TEST_F(error_migration_test_t, deprecation_keyed_by_identity_code) {
        // 同一 identity 的不同实例（retryable/transient 位不同）应识别为同一码
        const auto code = make_code(1, 1, 1);
        error_system::core::error_code_t code_with_retryable = code;
        code_with_retryable.set_retryable(true);

        reg_->mark_deprecated(code, "废弃");

        // 使用 code_with_retryable 查询应同样命中
        EXPECT_TRUE(reg_->is_deprecated(code_with_retryable));
        auto info = reg_->get_deprecation_info(code_with_retryable);
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->reason, "废弃");
    }

    TEST_F(error_migration_test_t, migration_keyed_by_identity_code) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);
        reg_->register_migration(old_code, new_code);

        // 不同实例但同一 identity
        error_system::core::error_code_t old_with_transient = old_code;
        old_with_transient.set_transient(true);

        auto migrated = reg_->migrate(old_with_transient);
        ASSERT_TRUE(migrated.has_value());
        EXPECT_EQ(migrated->get_identity_code(), new_code.get_identity_code());
    }


    TEST_F(error_migration_test_t, singleton_returns_same_instance) {
        auto& a = error_migration_registry_t::instance();
        auto& b = error_migration_registry_t::instance();
        EXPECT_EQ(&a, &b);
    }


    TEST_F(error_migration_test_t, concurrent_mark_and_query_is_safe) {
        const auto code = make_code(1, 1, 1);
        const auto replacement = make_code(1, 1, 2);

        std::vector<std::thread> threads;
        std::atomic<int> mark_count{0};
        std::atomic<int> query_count{0};

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < 100; ++j) {
                    if (i % 2 == 0) {
                        reg_->mark_deprecated(code, "并发废弃", replacement);
                        mark_count.fetch_add(1);
                    } else {
                        // 两个查询接口都被调用，验证并发安全性（不关心返回值）
                        if (reg_->is_deprecated(code) || reg_->get_deprecation_info(code)) {
                            query_count.fetch_add(1);
                        } else {
                            query_count.fetch_add(1);
                        }
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(mark_count.load(), 400);
        EXPECT_EQ(query_count.load(), 400);
        EXPECT_TRUE(reg_->is_deprecated(code));
    }

    TEST_F(error_migration_test_t, concurrent_migrate_recursive_is_safe) {
        // 构造链 a → b → c
        const auto a = make_code(1, 1, 1);
        const auto b = make_code(1, 1, 2);
        const auto c = make_code(1, 1, 3);
        reg_->register_migration(a, b);
        reg_->register_migration(b, c);

        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 100; ++j) {
                    auto result = reg_->migrate_recursive(a);
                    if (result.get_identity_code() == c.get_identity_code()) {
                        success_count.fetch_add(1);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(success_count.load(), 800);
    }

    TEST_F(error_migration_test_t, concurrent_register_unregister_is_safe) {
        const auto old_code = make_code(1, 1, 1);
        const auto new_code = make_code(1, 1, 2);

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 100; ++j) {
                    reg_->register_migration(old_code, new_code);
                    reg_->unregister_migration(old_code);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // 最终状态不确定，但不应崩溃
        SUCCEED();
    }

}  // namespace error_system::migration

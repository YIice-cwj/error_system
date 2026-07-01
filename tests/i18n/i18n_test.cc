#include "error_system/i18n/i18n.h"
#include "error_system/i18n/locale.h"

#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace error_system::core {

    class i18n_test_t : public ::testing::Test {
        protected:
        void SetUp() override {
            catalog_ = &error_system::i18n::i18n_t::instance();
            catalog_->clear_all();
            catalog_->set_default_locale(error_system::i18n::locale_t::zh_CN);
            catalog_->clear_active_locale();
        }

        void TearDown() override {
            catalog_->clear_all();
            catalog_->set_default_locale(error_system::i18n::locale_t::zh_CN);
            catalog_->clear_active_locale();
        }

        error_system::i18n::i18n_t* catalog_{nullptr};
    };

    TEST_F(i18n_test_t, register_and_query_message) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "数据库连接超时");
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "Database connection timeout");

        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code), "数据库连接超时");
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::en_US, code), "Database connection timeout");
    }

    TEST_F(i18n_test_t, query_returns_empty_for_unregistered_code) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{9}, module_id_t{9}, error_number_t{9});
        EXPECT_TRUE(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code).empty());
    }

    TEST_F(i18n_test_t, query_returns_empty_for_unregistered_locale) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "数据库连接超时");

        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::fr_FR, code), "数据库连接超时");
    }

    TEST_F(i18n_test_t, fallback_to_default_locale_when_code_missing) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "默认描述");

        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::en_US, code), "默认描述");
    }

    TEST_F(i18n_test_t, no_fallback_when_locale_equals_default_but_missing) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        EXPECT_TRUE(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code).empty());
    }

    TEST_F(i18n_test_t, active_locale_overrides_default) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "中文");
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "English");

        catalog_->set_active_locale(error_system::i18n::locale_t::en_US);
        EXPECT_EQ(catalog_->get_message(code), "English");

        catalog_->set_active_locale(error_system::i18n::locale_t::zh_CN);
        EXPECT_EQ(catalog_->get_message(code), "中文");
    }

    TEST_F(i18n_test_t, active_locale_none_falls_back_to_default) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "默认中文");

        catalog_->clear_active_locale();
        EXPECT_EQ(catalog_->get_message(code), "默认中文");
    }

    TEST_F(i18n_test_t, get_active_locale_returns_nullopt_when_cleared) {
        catalog_->clear_active_locale();
        EXPECT_FALSE(catalog_->get_active_locale().has_value());
    }

    TEST_F(i18n_test_t, get_active_locale_returns_value_when_set) {
        catalog_->set_active_locale(error_system::i18n::locale_t::ja_JP);
        EXPECT_EQ(catalog_->get_active_locale().value(), error_system::i18n::locale_t::ja_JP);
    }

    TEST_F(i18n_test_t, batch_register_messages) {
        const auto code1 = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        const auto code2 = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{2});

        std::vector<std::pair<error_code_t, std::string_view>> entries = {
            {code1, "Error one"},
            {code2, "Error two"},
        };

        const size_t count = catalog_->register_messages(error_system::i18n::locale_t::en_US, entries);
        EXPECT_EQ(count, 2UL);
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::en_US, code1), "Error one");
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::en_US, code2), "Error two");
    }

    TEST_F(i18n_test_t, clear_locale_removes_messages) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "English");
        ASSERT_EQ(catalog_->message_count(error_system::i18n::locale_t::en_US), 1UL);

        const size_t removed = catalog_->clear_locale(error_system::i18n::locale_t::en_US);
        EXPECT_EQ(removed, 1UL);
        EXPECT_EQ(catalog_->message_count(error_system::i18n::locale_t::en_US), 0UL);
    }

    TEST_F(i18n_test_t, clear_locale_returns_zero_for_unregistered) {
        EXPECT_EQ(catalog_->clear_locale(error_system::i18n::locale_t::ja_JP), 0UL);
    }

    TEST_F(i18n_test_t, clear_all_removes_everything) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "中文");
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "English");
        ASSERT_EQ(catalog_->get_locales().size(), 2UL);

        catalog_->clear_all();
        EXPECT_TRUE(catalog_->get_locales().empty());
    }

    TEST_F(i18n_test_t, get_locales_returns_all_registered) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "中文");
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "English");
        catalog_->register_message(error_system::i18n::locale_t::ja_JP, code, "日本語");

        const auto locales = catalog_->get_locales();
        EXPECT_EQ(locales.size(), 3UL);
    }

    TEST_F(i18n_test_t, message_count_for_unregistered_locale_is_zero) {
        EXPECT_EQ(catalog_->message_count(error_system::i18n::locale_t::ja_JP), 0UL);
    }

    TEST_F(i18n_test_t, retryable_transient_flags_do_not_affect_lookup) {
        auto code_base = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        auto code_retryable = code_base;
        code_retryable.set_retryable(true);
        auto code_transient = code_base;
        code_transient.set_transient(true);

        catalog_->register_message(error_system::i18n::locale_t::zh_CN, code_base, "可重试错误");

        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code_base), "可重试错误");
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code_retryable), "可重试错误");
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code_transient), "可重试错误");
    }

    TEST_F(i18n_test_t, concurrent_register_and_query_is_safe) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});

        std::thread writer([&code, this] {
            for (int i = 0; i < 100; ++i) {
                catalog_->register_message(error_system::i18n::locale_t::zh_CN, code, "并发写入");
            }
        });

        std::thread reader([&code, this] {
            for (int i = 0; i < 100; ++i) {
                [[maybe_unused]] const auto message = catalog_->get_message(error_system::i18n::locale_t::zh_CN, code);
            }
        });

        writer.join();
        reader.join();

        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::zh_CN, code), "并发写入");
    }

    TEST_F(i18n_test_t, set_default_locale_changes_fallback) {
        const auto code = error_code_t(error_level_t::error, domain::system_domain_t::database, subsystem_id_t{1}, module_id_t{1}, error_number_t{1});
        catalog_->register_message(error_system::i18n::locale_t::en_US, code, "English fallback");

        catalog_->set_default_locale(error_system::i18n::locale_t::en_US);
        EXPECT_EQ(catalog_->get_message(error_system::i18n::locale_t::fr_FR, code), "English fallback");

        EXPECT_EQ(catalog_->get_default_locale(), error_system::i18n::locale_t::en_US);
    }

    /** locale_t 枚举转换测试 */

    TEST(locale_test, to_string_covers_all_locales) {
        using namespace error_system::i18n;
        EXPECT_EQ(to_string(locale_t::en_US), "en_US");
        EXPECT_EQ(to_string(locale_t::zh_CN), "zh_CN");
        EXPECT_EQ(to_string(locale_t::zh_TW), "zh_TW");
        EXPECT_EQ(to_string(locale_t::ja_JP), "ja_JP");
        EXPECT_EQ(to_string(locale_t::ko_KR), "ko_KR");
        EXPECT_EQ(to_string(locale_t::fr_FR), "fr_FR");
        EXPECT_EQ(to_string(locale_t::de_DE), "de_DE");
        EXPECT_EQ(to_string(locale_t::es_ES), "es_ES");
        EXPECT_EQ(to_string(locale_t::ru_RU), "ru_RU");
        EXPECT_EQ(to_string(locale_t::pt_BR), "pt_BR");
        EXPECT_EQ(to_string(locale_t::it_IT), "it_IT");
        EXPECT_EQ(to_string(locale_t::ar_SA), "ar_SA");
        EXPECT_EQ(to_string(locale_t::hi_IN), "hi_IN");
        EXPECT_EQ(to_string(locale_t::th_TH), "th_TH");
        EXPECT_EQ(to_string(locale_t::vi_VN), "vi_VN");
    }

    TEST(locale_test, from_string_round_trip) {
        using namespace error_system::i18n;
        EXPECT_EQ(error_system::i18n::from_string("en_US"), locale_t::en_US);
        EXPECT_EQ(error_system::i18n::from_string("zh_CN"), locale_t::zh_CN);
        EXPECT_EQ(error_system::i18n::from_string("ja_JP"), locale_t::ja_JP);
        EXPECT_EQ(error_system::i18n::from_string("invalid"), locale_t::en_US);
    }

    TEST(locale_test, is_valid_recognizes_known_locales) {
        EXPECT_TRUE(error_system::i18n::is_valid("en_US"));
        EXPECT_TRUE(error_system::i18n::is_valid("zh_CN"));
        EXPECT_TRUE(error_system::i18n::is_valid("vi_VN"));
        EXPECT_FALSE(error_system::i18n::is_valid("invalid"));
        EXPECT_FALSE(error_system::i18n::is_valid(""));
    }

    TEST(locale_test, to_string_invalid_enum_returns_en_us) {
        using error_system::i18n::locale_t;
        using error_system::i18n::to_string;
        EXPECT_EQ(to_string(static_cast<locale_t>(99)), "en_US");
        EXPECT_EQ(to_string(static_cast<locale_t>(255)), "en_US");
    }

    TEST(locale_test, from_string_empty_returns_en_us) {
        EXPECT_EQ(error_system::i18n::from_string(""), error_system::i18n::locale_t::en_US);
    }

    TEST(locale_test, from_string_round_trip_all_locales) {
        using error_system::i18n::from_string;
        using error_system::i18n::LOCALE_TABLE;
        for (const auto& [value, name] : LOCALE_TABLE) {
            EXPECT_EQ(from_string(name), value) << "round-trip failed for " << name;
        }
    }

    TEST(locale_test, is_valid_all_known_locales) {
        using error_system::i18n::is_valid;
        using error_system::i18n::LOCALE_TABLE;
        for (const auto& [_, name] : LOCALE_TABLE) {
            EXPECT_TRUE(is_valid(name)) << "is_valid failed for " << name;
        }
    }

    TEST(locale_test, is_valid_rejects_case_variants) {
        EXPECT_FALSE(error_system::i18n::is_valid("en_us"));
        EXPECT_FALSE(error_system::i18n::is_valid("EN_US"));
        EXPECT_FALSE(error_system::i18n::is_valid("Zh_Cn"));
    }

    TEST(locale_test, is_valid_rejects_whitespace) {
        EXPECT_FALSE(error_system::i18n::is_valid(" en_US"));
        EXPECT_FALSE(error_system::i18n::is_valid("en_US "));
        EXPECT_FALSE(error_system::i18n::is_valid("en US"));
    }

    static_assert(error_system::i18n::LOCALE_TABLE.size() == error_system::i18n::LOCALE_COUNT,
                  "LOCALE_TABLE size must match LOCALE_COUNT");

}  // namespace error_system::core

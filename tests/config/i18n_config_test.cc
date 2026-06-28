#include "error_system/config/i18n_config.h"

#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "error_system/i18n/locale.h"

namespace error_system::config {

    using error_system::i18n::locale_t;

    class i18n_config_test_t : public ::testing::Test {
    protected:
        void SetUp() override {
            i18n_config_t::set_enable_i18n(true);
            i18n_config_t::set_default_locale(locale_t::zh_CN);
            i18n_config_t::clear_output_locale();
        }

        void TearDown() override {
            i18n_config_t::set_enable_i18n(true);
            i18n_config_t::set_default_locale(locale_t::zh_CN);
            i18n_config_t::clear_output_locale();
        }
    };

    TEST_F(i18n_config_test_t, enable_i18n_defaults_to_true) {
        EXPECT_TRUE(i18n_config_t::is_i18n_enabled());
    }

    TEST_F(i18n_config_test_t, set_enable_i18n_toggles_flag) {
        i18n_config_t::set_enable_i18n(false);
        EXPECT_FALSE(i18n_config_t::is_i18n_enabled());

        i18n_config_t::set_enable_i18n(true);
        EXPECT_TRUE(i18n_config_t::is_i18n_enabled());
    }

    TEST_F(i18n_config_test_t, default_locale_defaults_to_zh_CN) {
        EXPECT_EQ(i18n_config_t::get_default_locale(), locale_t::zh_CN);
    }

    TEST_F(i18n_config_test_t, set_default_locale_persists) {
        i18n_config_t::set_default_locale(locale_t::en_US);
        EXPECT_EQ(i18n_config_t::get_default_locale(), locale_t::en_US);

        i18n_config_t::set_default_locale(locale_t::ja_JP);
        EXPECT_EQ(i18n_config_t::get_default_locale(), locale_t::ja_JP);
    }

    TEST_F(i18n_config_test_t, output_locale_initially_unset) {
        EXPECT_FALSE(i18n_config_t::get_output_locale().has_value());
    }

    TEST_F(i18n_config_test_t, set_output_locale_marks_as_set) {
        i18n_config_t::set_output_locale(locale_t::en_US);
        const auto opt = i18n_config_t::get_output_locale();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(*opt, locale_t::en_US);
    }

    TEST_F(i18n_config_test_t, clear_output_locale_unsets_flag) {
        i18n_config_t::set_output_locale(locale_t::en_US);
        ASSERT_TRUE(i18n_config_t::get_output_locale().has_value());

        i18n_config_t::clear_output_locale();
        EXPECT_FALSE(i18n_config_t::get_output_locale().has_value());
    }

    TEST_F(i18n_config_test_t, resolve_output_locale_uses_output_when_set) {
        i18n_config_t::set_default_locale(locale_t::zh_CN);
        i18n_config_t::set_output_locale(locale_t::en_US);

        EXPECT_EQ(i18n_config_t::resolve_output_locale(), locale_t::en_US);
    }

    TEST_F(i18n_config_test_t, resolve_output_locale_falls_back_to_default) {
        i18n_config_t::set_default_locale(locale_t::ja_JP);
        i18n_config_t::clear_output_locale();

        EXPECT_EQ(i18n_config_t::resolve_output_locale(), locale_t::ja_JP);
    }

    TEST_F(i18n_config_test_t, resolve_output_locale_after_clear_uses_default) {
        i18n_config_t::set_default_locale(locale_t::zh_CN);
        i18n_config_t::set_output_locale(locale_t::en_US);
        ASSERT_EQ(i18n_config_t::resolve_output_locale(), locale_t::en_US);

        i18n_config_t::clear_output_locale();
        EXPECT_EQ(i18n_config_t::resolve_output_locale(), locale_t::zh_CN);
    }

    TEST_F(i18n_config_test_t, set_output_locale_overrides_previous_value) {
        i18n_config_t::set_output_locale(locale_t::en_US);
        i18n_config_t::set_output_locale(locale_t::ja_JP);

        const auto opt = i18n_config_t::get_output_locale();
        ASSERT_TRUE(opt.has_value());
        EXPECT_EQ(*opt, locale_t::ja_JP);
        EXPECT_EQ(i18n_config_t::resolve_output_locale(), locale_t::ja_JP);
    }

    TEST_F(i18n_config_test_t, all_locales_round_trip_through_config) {
        for (const auto locale : {locale_t::en_US, locale_t::zh_CN, locale_t::zh_TW,
                                   locale_t::ja_JP, locale_t::ko_KR, locale_t::fr_FR,
                                   locale_t::de_DE, locale_t::es_ES, locale_t::ru_RU,
                                   locale_t::pt_BR, locale_t::it_IT, locale_t::ar_SA,
                                   locale_t::hi_IN, locale_t::th_TH, locale_t::vi_VN}) {
            i18n_config_t::set_default_locale(locale);
            EXPECT_EQ(i18n_config_t::get_default_locale(), locale);

            i18n_config_t::set_output_locale(locale);
            const auto opt = i18n_config_t::get_output_locale();
            ASSERT_TRUE(opt.has_value());
            EXPECT_EQ(*opt, locale);
            EXPECT_EQ(i18n_config_t::resolve_output_locale(), locale);

            i18n_config_t::clear_output_locale();
        }
    }

    TEST_F(i18n_config_test_t, concurrent_set_and_read_output_locale) {
        std::vector<std::thread> threads;
        std::atomic<int> mismatches{0};

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&mismatches]() {
                for (int j = 0; j < 1000; ++j) {
                    const auto locale = static_cast<locale_t>(j % 15);
                    i18n_config_t::set_output_locale(locale);
                    const auto resolved = i18n_config_t::resolve_output_locale();
                    if (resolved != locale) {
                        mismatches.fetch_add(1);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_EQ(mismatches.load(), 0);
    }

    TEST_F(i18n_config_test_t, concurrent_enable_toggle_is_safe) {
        std::vector<std::thread> threads;
        std::atomic<bool> error_occurred{false};

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&error_occurred]() {
                for (int j = 0; j < 500; ++j) {
                    i18n_config_t::set_enable_i18n(j % 2 == 0);
                    if (!i18n_config_t::is_i18n_enabled() && j % 2 == 0) {
                        error_occurred.store(true);
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        EXPECT_FALSE(error_occurred.load());
    }

}  // namespace error_system::config

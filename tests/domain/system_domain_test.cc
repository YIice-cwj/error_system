#include "error_system/domain/system_domain.h"

#include <gtest/gtest.h>

namespace error_system::domain {

    TEST(system_domain_test, to_int_converts_correctly) {
        EXPECT_EQ(to_int(system_domain_t::none), 0);
        EXPECT_EQ(to_int(system_domain_t::system), 1);
        EXPECT_EQ(to_int(system_domain_t::middleware), 2);
        EXPECT_EQ(to_int(system_domain_t::database), 3);
        EXPECT_EQ(to_int(system_domain_t::application), 4);
        EXPECT_EQ(to_int(system_domain_t::third_party), 5);
    }

    TEST(system_domain_test, to_string_converts_correctly) {
        EXPECT_STREQ(to_string(system_domain_t::none), "none");
        EXPECT_STREQ(to_string(system_domain_t::system), "system");
        EXPECT_STREQ(to_string(system_domain_t::middleware), "middleware");
        EXPECT_STREQ(to_string(system_domain_t::database), "database");
        EXPECT_STREQ(to_string(system_domain_t::application), "application");
        EXPECT_STREQ(to_string(system_domain_t::third_party), "third_party");
    }

    TEST(system_domain_test, is_valid_checks_correctly) {
        EXPECT_TRUE(is_valid(0));
        EXPECT_TRUE(is_valid(1));
        EXPECT_TRUE(is_valid(2));
        EXPECT_TRUE(is_valid(3));
        EXPECT_TRUE(is_valid(4));
        EXPECT_TRUE(is_valid(5));
        EXPECT_FALSE(is_valid(6));
        EXPECT_FALSE(is_valid(255));
    }

    TEST(system_domain_test, from_int_converts_correctly) {
        EXPECT_EQ(from_int(0), system_domain_t::none);
        EXPECT_EQ(from_int(1), system_domain_t::system);
        EXPECT_EQ(from_int(2), system_domain_t::middleware);
        EXPECT_EQ(from_int(3), system_domain_t::database);
        EXPECT_EQ(from_int(4), system_domain_t::application);
        EXPECT_EQ(from_int(5), system_domain_t::third_party);
    }

    TEST(system_domain_test, from_int_invalid_returns_none) {
        EXPECT_EQ(from_int(6), system_domain_t::none);
        EXPECT_EQ(from_int(100), system_domain_t::none);
    }

    TEST(system_domain_test, from_string_converts_correctly) {
        EXPECT_EQ(from_string("none"), system_domain_t::none);
        EXPECT_EQ(from_string("system"), system_domain_t::system);
        EXPECT_EQ(from_string("middleware"), system_domain_t::middleware);
        EXPECT_EQ(from_string("database"), system_domain_t::database);
        EXPECT_EQ(from_string("application"), system_domain_t::application);
        EXPECT_EQ(from_string("third_party"), system_domain_t::third_party);
    }

    TEST(system_domain_test, from_string_invalid_returns_none) {
        EXPECT_EQ(from_string("unknown"), system_domain_t::none);
        EXPECT_EQ(from_string(""), system_domain_t::none);
        EXPECT_EQ(from_string("invalid"), system_domain_t::none);
    }

    TEST(system_domain_test, constexpr_evaluation_works) {
        constexpr auto domain = system_domain_t::database;
        static_assert(to_int(domain) == 3, "constexpr evaluation failed");
        static_assert(is_valid(3), "constexpr evaluation failed");
        EXPECT_EQ(to_int(domain), 3);
    }

    TEST(system_domain_test, count_is_last_value) {
        constexpr int count = static_cast<int>(system_domain_t::count);
        EXPECT_EQ(count, 6);
    }

}  // namespace error_system::domain

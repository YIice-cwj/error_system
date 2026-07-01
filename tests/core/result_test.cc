#include "error_system/core/result.h"

#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "error_system/core/error_registry.h"

namespace error_system::core {

    class result_test_t : public ::testing::Test {
    protected:
        void SetUp() override { error_registry_t::instance().unregister_all(); }

        void TearDown() override { error_registry_t::instance().unregister_all(); }
    };

    TEST_F(result_test_t, success_result_with_value) {
        result_t<int> result(42);
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_test_t, error_result_with_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Test error 1");

        error_context_t context(located_code_t{code}, "test error");
        result_t<int> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test_t, error_result_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{100});
        error_registry_t::instance().register_error(code, "ERR_100", "Error 100");

        error_context_t context(located_code_t{code}, "error message");
        result_t<int> result(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
    }

    TEST_F(result_test_t, move_constructor_transfers_value) {
        result_t<std::string> result(std::string("moved value"));
        EXPECT_EQ(result.value(), "moved value");
    }

    TEST_F(result_test_t, and_then_chains_success) {
        result_t<int> result(5);
        auto new_result = std::move(result).and_then([](int value) -> result_t<int> { return result_t<int>(value * 2); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 10);
    }

    TEST_F(result_test_t, and_then_preserves_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "error");
        result_t<int> result(context);
        bool called = false;
        auto new_result = std::move(result).and_then([&called](int value) -> result_t<int> {
            called = true;
            return result_t<int>(value * 2);
        });
        EXPECT_TRUE(new_result.is_error());
        EXPECT_FALSE(called);
    }

    TEST_F(result_test_t, or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "error");
        result_t<int> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(42); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 42);
    }

    TEST_F(result_test_t, or_else_preserves_success) {
        result_t<int> result(100);
        bool called = false;
        auto new_result = std::move(result).or_else([&called](const error_context_t&) -> result_t<int> {
            called = true;
            return result_t<int>(42);
        });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 100);
        EXPECT_FALSE(called);
    }

    TEST_F(result_test_t, void_success_result) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        EXPECT_FALSE(result.is_error());
    }

    TEST_F(result_test_t, void_error_result) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "error");
        result_t<void> result(context);
        EXPECT_FALSE(result.is_success());
        EXPECT_TRUE(result.is_error());
    }

    TEST_F(result_test_t, void_and_then_chains_success) {
        result_t<void> result;
        bool called = false;
        auto new_result = std::move(result).and_then([&called]() -> result_t<void> {
            called = true;
            return result_t<void>();
        });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_TRUE(called);
    }

    TEST_F(result_test_t, void_or_else_handles_error) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "error");
        result_t<void> result(context);
        auto new_result =
            std::move(result).or_else([](const error_context_t&) -> result_t<void> { return result_t<void>(); });
        EXPECT_TRUE(new_result.is_success());
    }

    TEST_F(result_test_t, lvalue_and_then_works) {
        result_t<int> result(5);
        auto new_result = result.and_then([](int value) -> result_t<int> { return result_t<int>(value * 3); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 15);
    }

    TEST_F(result_test_t, lvalue_or_else_works) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "error");
        result_t<int> result(context);
        auto new_result = result.or_else([](const error_context_t&) -> result_t<int> { return result_t<int>(99); });
        EXPECT_TRUE(new_result.is_success());
        EXPECT_EQ(new_result.value(), 99);
    }

    TEST_F(result_test_t, make_error_with_code_and_message) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code, "factory error");
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "factory error");
    }

    TEST_F(result_test_t, make_error_with_code_only) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        auto result = result_t<int>::make_error(code);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "");
    }

    TEST_F(result_test_t, make_error_from_context) {
        auto code = error_code_t(error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0}, module_id_t{0}, error_number_t{1});
        error_registry_t::instance().register_error(code, "ERR_1", "Error 1");

        error_context_t context(located_code_t{code}, "from context");
        auto result = result_t<int>::make_error(context);
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().get_code().get_code(), code.get_code());
        EXPECT_EQ(result.error().message, "from context");
    }


    TEST_F(result_test_t, expect_on_success_returns_value) {
        result_t<int> result(42);
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_test_t, void_expect_on_success_no_op) {
        result_t<void> result;
        EXPECT_TRUE(result.is_success());
        EXPECT_TRUE(result.is_success());
    }

    TEST_F(result_test_t, expect_with_string_value) {
        result_t<std::string> result(std::string("hello"));
        EXPECT_EQ(result.value(), "hello");
    }


    TEST_F(result_test_t, value_pointer_on_success_returns_non_null) {
        auto result = result_t<int>(99);
        const int* ptr = result.value_pointer();
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(*ptr, 99);
    }

    TEST_F(result_test_t, value_pointer_on_error_returns_null) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(code, "ERR_VP", "value_pointer error");
        auto result = result_t<int>::make_error(code, "fail");
        const int* ptr = result.value_pointer();
        EXPECT_EQ(ptr, nullptr);
    }


    TEST_F(result_test_t, value_or_on_success_returns_value) {
        auto result = result_t<int>(42);
        EXPECT_EQ(result.value_or(100), 42);
    }

    TEST_F(result_test_t, value_or_on_error_returns_default) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(code, "ERR_VO", "value_or error");
        auto result = result_t<int>::make_error(code, "fail");
        EXPECT_EQ(result.value_or(100), 100);
    }


    TEST_F(result_test_t, map_on_success_transforms_value) {
        auto result = result_t<int>(21);
        auto mapped = result.map([](int value) noexcept { return value * 2; });
        EXPECT_TRUE(mapped.is_success());
        EXPECT_EQ(mapped.value_or(0), 42);
    }

    TEST_F(result_test_t, map_on_error_propagates_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(code, "ERR_MAP", "map error");
        auto result = result_t<int>::make_error(code, "fail");
        auto mapped = result.map([](int value) noexcept { return value * 2; });
        EXPECT_TRUE(mapped.is_error());
        EXPECT_EQ(mapped.value_or(0), 0);
    }


    TEST_F(result_test_t, map_error_on_success_preserves_value) {
        auto result = result_t<int>(42);
        auto mapped = result.map_error([](const error_context_t&) noexcept {
            auto code = error_code_t{error_level_t::fatal, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{99}};
            error_registry_t::instance().register_error(code, "ERR_ME99", "map_error 99");
            return error_context_t{located_code_t{code}, "unused"};
        });
        EXPECT_TRUE(mapped.is_success());
        EXPECT_EQ(mapped.value_or(0), 42);
    }

    TEST_F(result_test_t, map_error_on_error_transforms_error) {
        auto original_code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(original_code, "ERR_ME1", "map_error 1");
        auto result = result_t<int>::make_error(original_code, "original");
        auto mapped = result.map_error([](const error_context_t&) noexcept {
            auto code = error_code_t{error_level_t::fatal, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{99}};
            error_registry_t::instance().register_error(code, "ERR_ME99", "map_error 99");
            return error_context_t{located_code_t{code}, "transformed"};
        });
        EXPECT_TRUE(mapped.is_error());
        EXPECT_EQ(mapped.error().get_code().get_level(), error_level_t::fatal);
    }

    TEST_F(result_test_t, dereference_operator_returns_value_on_success) {
        auto result = result_t<int>(42);
        EXPECT_EQ(*result, 42);
    }

    TEST_F(result_test_t, dereference_operator_returns_mutable_value) {
        auto result = result_t<int>(42);
        *result = 100;
        EXPECT_EQ(result.value(), 100);
    }

    TEST_F(result_test_t, arrow_operator_accesses_members) {
        auto result = result_t<std::string>("hello");
        EXPECT_EQ(result->size(), 5);
    }

    TEST_F(result_test_t, arrow_operator_returns_null_on_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(code, "ERR_ARROW", "arrow error");
        auto result = result_t<std::string>::make_error(code, "fail");
        EXPECT_EQ(result.operator->(), nullptr);
    }

    TEST_F(result_test_t, transform_alias_matches_map) {
        auto result = result_t<int>(21);
        auto transformed = result.transform([](int value) noexcept { return value * 2; });
        EXPECT_TRUE(transformed.is_success());
        EXPECT_EQ(transformed.value_or(0), 42);
    }

    TEST_F(result_test_t, transform_error_alias_matches_map_error) {
        auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
        error_registry_t::instance().register_error(code, "ERR_TE1", "transform_error 1");
        auto result = result_t<int>::make_error(code, "original");
        auto transformed = result.transform_error([](const error_context_t&) noexcept {
            auto new_code = error_code_t{error_level_t::fatal, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{99}};
            error_registry_t::instance().register_error(new_code, "ERR_TE99", "transform_error 99");
            return error_context_t{located_code_t{new_code}, "transformed"};
        });
        EXPECT_TRUE(transformed.is_error());
        EXPECT_EQ(transformed.error().get_code().get_level(), error_level_t::fatal);
    }

    namespace {
        result_t<int> try_helper_success() {
            ERROR_SYSTEM_TRY(r, result_t<int>(21));
            return result_t<int>((*r) * 2);
        }

        result_t<int> try_helper_error() {
            auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{1}};
            error_registry_t::instance().register_error(code, "ERR_TRY1", "try error 1");
            ERROR_SYSTEM_TRY(r, result_t<int>::make_error(code, "inner failure"));
            return result_t<int>((*r) * 2);
        }

        result_t<int> try_discard_helper() {
            ERROR_SYSTEM_TRY_DISCARD(result_t<int>(7));
            return result_t<int>(14);
        }

        result_t<int> try_discard_error_helper() {
            auto code = error_code_t{error_level_t::error, domain::system_domain_t::none, subsystem_id_t{0x0001}, module_id_t{0x0001}, error_number_t{2}};
            error_registry_t::instance().register_error(code, "ERR_TRY2", "try error 2");
            ERROR_SYSTEM_TRY_DISCARD(result_t<int>::make_error(code, "discarded failure"));
            return result_t<int>(14);
        }
    }  // namespace

    TEST_F(result_test_t, try_macro_returns_value_when_success) {
        auto result = try_helper_success();
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value(), 42);
    }

    TEST_F(result_test_t, try_macro_returns_error_when_inner_fails) {
        auto result = try_helper_error();
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().message, "inner failure");
    }

    TEST_F(result_test_t, try_discard_macro_returns_value_when_success) {
        auto result = try_discard_helper();
        EXPECT_TRUE(result.is_success());
        EXPECT_EQ(result.value(), 14);
    }

    TEST_F(result_test_t, try_discard_macro_returns_error_when_inner_fails) {
        auto result = try_discard_error_helper();
        EXPECT_TRUE(result.is_error());
        EXPECT_EQ(result.error().message, "discarded failure");
    }

}  // namespace error_system::core
#include "error_dict.h"
#include "error_system/translator/error_translator.h"
#include <gtest/gtest.h>

namespace error_system::translator {

    class error_translator_test : public ::testing::Test {
        protected:
        void SetUp() override {
            // 每个测试前清理动态注册的内容
            // 注意：静态注册的内容无法清理
        }
    };

    TEST_F(error_translator_test, instance_returns_singleton) {
        auto& instance1 = error_translator_t::instance();
        auto& instance2 = error_translator_t::instance();
        EXPECT_EQ(&instance1, &instance2);
    }

    TEST_F(error_translator_test, register_subsystem_stores_name) {
        auto& translator = error_translator_t::instance();
        translator.register_subsystem(1001, "测试子系统");

        std::string result = translator.translate(1001, 1);
        EXPECT_NE(result.find("测试子系统"), std::string::npos);
    }

    TEST_F(error_translator_test, register_module_stores_name) {
        auto& translator = error_translator_t::instance();
        translator.register_subsystem(1002, "测试子系统");
        translator.register_module(1002, 10, "测试模块");

        std::string result = translator.translate(1002, 10);
        EXPECT_NE(result.find("测试模块"), std::string::npos);
    }

    TEST_F(error_translator_test, translate_unknown_subsystem_returns_id) {
        auto& translator = error_translator_t::instance();
        std::string result = translator.translate(9999, 1);

        // 未注册的子系统应该返回数字ID
        EXPECT_NE(result.find("9999"), std::string::npos);
    }

    TEST_F(error_translator_test, translate_unknown_module_returns_id) {
        auto& translator = error_translator_t::instance();
        translator.register_subsystem(1003, "已知子系统");

        std::string result = translator.translate(1003, 8888);

        // 未注册的模块应该返回数字ID
        EXPECT_NE(result.find("8888"), std::string::npos);
    }

    TEST_F(error_translator_test, get_static_subsys_name_returns_known_name) {
        // 测试已知的静态子系统名称
        auto name = get_static_subsys_name(101);
        EXPECT_EQ(name, "交易服务");

        name = get_static_subsys_name(102);
        EXPECT_EQ(name, "用户服务");
    }

    TEST_F(error_translator_test, get_static_subsys_name_returns_unknown_for_invalid) {
        auto name = get_static_subsys_name(9999);
        EXPECT_EQ(name, "未知子系统");
    }

    TEST_F(error_translator_test, get_static_module_name_returns_known_name) {
        // 测试已知的静态模块名称
        auto name = get_static_module_name(101, 1);
        EXPECT_EQ(name, "订单模块");

        name = get_static_module_name(103, 1);
        EXPECT_EQ(name, "支付网关");
    }

    TEST_F(error_translator_test, get_static_module_name_returns_unknown_for_invalid) {
        auto name = get_static_module_name(9999, 9999);
        EXPECT_EQ(name, "未知模块");
    }

    TEST_F(error_translator_test, translate_uses_static_names_when_available) {
        auto& translator = error_translator_t::instance();

        // 101:1 是静态注册的
        std::string result = translator.translate(101, 1);
        EXPECT_NE(result.find("交易服务"), std::string::npos);
        EXPECT_NE(result.find("订单模块"), std::string::npos);
    }

    TEST_F(error_translator_test, register_overrides_existing) {
        auto& translator = error_translator_t::instance();

        translator.register_subsystem(2000, "原始名称");
        translator.register_subsystem(2000, "新名称");

        std::string result = translator.translate(2000, 1);
        EXPECT_NE(result.find("新名称"), std::string::npos);
    }

}  // namespace error_system::translator

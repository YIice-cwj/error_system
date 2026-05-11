#include "error_system/utils/stack_trace_utils.h"

#include <gtest/gtest.h>
#include <string>

namespace error_system::utils {

    class stack_trace_utils_test : public ::testing::Test {};

    /**
     * @brief 辅助函数：用于验证栈帧中包含当前函数名
     * @note 禁止内联：Release 模式下内联会让该函数从栈帧中消失，导致 Windows 上测试失败
     */
#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    static void helper_for_stacktrace(std::vector<std::string>& out) {
        out = stack_trace_utils_t::generate(0);
    }

    TEST_F(stack_trace_utils_test, generate_returns_non_empty) {
        auto trace = stack_trace_utils_t::generate(0);
        EXPECT_FALSE(trace.empty());
    }

    TEST_F(stack_trace_utils_test, generate_with_skip_reduces_frames) {
        auto trace_no_skip = stack_trace_utils_t::generate(0);
        auto trace_skip_1 = stack_trace_utils_t::generate(1);

        EXPECT_FALSE(trace_no_skip.empty());
        EXPECT_FALSE(trace_skip_1.empty());
        EXPECT_LE(trace_skip_1.size(), trace_no_skip.size());
    }

    TEST_F(stack_trace_utils_test, generate_with_zero_max_returns_empty) {
        auto trace = stack_trace_utils_t::generate(1, 0);
        EXPECT_TRUE(trace.empty());
    }

    TEST_F(stack_trace_utils_test, generate_contains_current_function) {
        std::vector<std::string> trace;
        helper_for_stacktrace(trace);

        bool found = false;
        for (const auto& frame : trace) {
            if (frame.find("helper_for_stacktrace") != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "栈帧中应包含 helper_for_stacktrace 函数名";
    }

    TEST_F(stack_trace_utils_test, generate_contains_test_function) {
        auto trace = stack_trace_utils_t::generate(0);

        bool found_test = false;
        for (const auto& frame : trace) {
            if (frame.find("stack_trace_utils_test") != std::string::npos ||
                frame.find("generate_contains_test_function") != std::string::npos) {
                found_test = true;
                break;
            }
        }
        EXPECT_TRUE(found_test) << "栈帧中应包含测试函数相关信息";
    }

    TEST_F(stack_trace_utils_test, generate_frames_are_readable) {
        auto trace = stack_trace_utils_t::generate(0, 8);
        for (const auto& frame : trace) {
            EXPECT_FALSE(frame.empty());
        }
    }

    TEST_F(stack_trace_utils_test, generate_respects_max_frames) {
        constexpr int max_frames = 4;
        auto trace = stack_trace_utils_t::generate(0, max_frames);
        EXPECT_LE(static_cast<int>(trace.size()), max_frames);
    }

    TEST_F(stack_trace_utils_test, generate_skip_more_than_available_returns_empty) {
        auto trace = stack_trace_utils_t::generate(1000, 64);
        EXPECT_TRUE(trace.empty());
    }

}  // namespace error_system::utils

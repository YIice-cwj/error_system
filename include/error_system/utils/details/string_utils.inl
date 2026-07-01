#pragma once
#include "error_system/utils/string_utils.h"

namespace error_system::utils {

    /**
     * @brief 将字符串解析为数字
     * @details 使用 std::from_chars 进行高效解析
     * @tparam T 目标数字类型
     * @param string 输入字符串
     * @return std::optional<T> 解析后的数字，失败返回 nullopt
     */
    template <typename T>
    inline std::optional<T> string_utils_t::parse_number(std::string_view string) noexcept {
        T value{};
        auto [pointer, error] = std::from_chars(string.data(), string.data() + string.size(), value);
        if (error == std::errc{}) {
            return value;
        }
        return std::nullopt;
    }

}  // namespace error_system::utils

#include "error_system/utils/stack_trace_utils.h"

namespace error_system::utils {

#ifdef ERROR_SYSTEM_ENABLE_STACKTRACE

    namespace {
#if defined(__APPLE__) || defined(__linux__)
        /**
         * @brief 抓取当前线程的函数调用栈
         * @param buffer 存储栈帧的缓冲区
         * @param max_frames 最大抓取深度
         * @return int 实际抓取的栈帧数
         */
        int capture_os_frames(void** buffer, int max_frames) noexcept {
            return backtrace(buffer, max_frames);
        }

        /**
         * @brief 解析函数调用栈
         * @param callstack 函数调用栈
         * @param frames 函数调用栈深度
         * @param skip_frames 需要跳过的顶部栈帧数（比如跳过抓取函数自身）
         * @return std::vector<std::string> 每一层调用栈的可读字符串
         */
        std::vector<std::string> resolve_os_symbols(void** callstack, int frames, int skip_frames) noexcept {
            std::vector<std::string> trace;
            char** symbols = backtrace_symbols(callstack, frames);
            if (!symbols)
                return trace;

            for (int i = skip_frames; i < frames; ++i) {
                std::string symbol_str = symbols[i];
                auto begin_name = symbol_str.find("_Z");

                if (begin_name != std::string::npos) {
                    auto end_name = symbol_str.find_first_of(" +)", begin_name);
                    if (end_name == std::string::npos) {
                        end_name = symbol_str.length();
                    }
                    std::string mangled_name = symbol_str.substr(begin_name, end_name - begin_name);
                    int status = -1;
                    char* demangled = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
                    if (status == 0 && demangled) {
                        symbol_str.replace(begin_name, end_name - begin_name, demangled);
                        free(demangled);
                    }
                }

                auto first_space = symbol_str.find_first_not_of("0123456789 ");
                if (first_space != std::string::npos && first_space > 0 && first_space < 10) {
                    symbol_str = symbol_str.substr(first_space);
                }

                trace.push_back(symbol_str);
            }
            free(symbols);
            return trace;
        }

#elif defined(_WIN32)

        struct dbghelp_manager_t {
            HANDLE process = GetCurrentProcess();
            std::mutex mutex;

            dbghelp_manager_t() noexcept {
                SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
                SymInitialize(process, nullptr, TRUE);
            }

            ~dbghelp_manager_t() noexcept { SymCleanup(process); }

            dbghelp_manager_t(const dbghelp_manager_t&) = delete;
            dbghelp_manager_t& operator=(const dbghelp_manager_t&) = delete;
            dbghelp_manager_t(dbghelp_manager_t&&) = delete;
            dbghelp_manager_t& operator=(dbghelp_manager_t&&) = delete;

            static dbghelp_manager_t& instance() noexcept {
                static dbghelp_manager_t instance;
                return instance;
            }
        };

        /**
         * @brief 格式化不可解析的地址
         * @param address 地址
         * @return std::string 格式化后的字符串
         */
        std::string format_fallback_address(void* address) noexcept {
            try {
                std::ostringstream oss;
                oss << "[Unknown Symbol] at " << address;
                return oss.str();
            } catch (...) {
                return "[Unknown Symbol]";
            }
        }

        /**
         * @brief 抓取当前线程的函数调用栈
         * @param buffer 存储栈帧的缓冲区
         * @param max_frames 最大抓取深度
         * @return int 实际抓取的栈帧数
         */
        int capture_os_frames(void** buffer, int max_frames) noexcept {
            return CaptureStackBackTrace(0, max_frames, buffer, nullptr);
        }

        /**
         * @brief 解析函数调用栈
         * @param callstack 函数调用栈
         * @param frames 函数调用栈深度
         * @param skip_frames 需要跳过的顶部栈帧数（比如跳过抓取函数自身）
         * @return std::vector<std::string> 每一层调用栈的可读字符串
         */
        std::vector<std::string> resolve_os_symbols(void** callstack, int frames, int skip_frames) noexcept {
            std::vector<std::string> trace;

            auto& dbghelp = dbghelp_manager_t::instance();

            std::lock_guard<std::mutex> lock(dbghelp.mutex);

            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            for (int i = skip_frames; i < frames; i++) {
                DWORD64 address = reinterpret_cast<DWORD64>(callstack[i]);
                DWORD64 displacement = 0;

                if (SymFromAddr(dbghelp.process, address, &displacement, symbol)) {
                    std::string symbol_str = symbol->Name;

#if defined(__MINGW32__) || defined(__MINGW64__)
                    std::string mangled_name = symbol_str;

                    if (mangled_name.rfind("Z", 0) == 0) {
                        mangled_name = "_" + mangled_name;
                    }

                    if (mangled_name.rfind("_Z", 0) == 0) {
                        int status = -1;
                        char* demangled = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
                        if (status == 0 && demangled) {
                            symbol_str = std::string(demangled);
                            free(demangled);
                        }
                    }
#endif
                    trace.push_back(symbol_str);
                } else {
                    trace.push_back(format_fallback_address(callstack[i]));
                }
            }

            return trace;
        }
#endif
    }  // namespace

#endif  // ERROR_SYSTEM_ENABLE_STACKTRACE

    /**
     * @brief 抓取当前线程的函数调用栈
     * @param skip_frames 需要跳过的顶部栈帧数（比如跳过抓取函数自身）
     * @param max_frames 最大抓取深度
     * @return std::vector<std::string> 每一层调用栈的可读字符串
     */
    std::vector<std::string> stack_trace_utils_t::generate(int skip_frames, int max_frames) noexcept {
#ifndef ERROR_SYSTEM_ENABLE_STACKTRACE
        (void)skip_frames;
        (void)max_frames;
        return {};

#else
        constexpr int HARD_MAX_FRAMES = 32;
        if (max_frames <= 0) {
            return {};
        }
        if (max_frames > HARD_MAX_FRAMES) {
            max_frames = HARD_MAX_FRAMES;
        }
        std::vector<void*> callstack(static_cast<size_t>(max_frames));
        int frames = capture_os_frames(callstack.data(), max_frames);

        if (frames <= skip_frames) {
            return {};
        }

        return resolve_os_symbols(callstack.data(), frames, skip_frames);
#endif
    }

}  // namespace error_system::utils
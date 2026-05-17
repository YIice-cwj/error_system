#!/usr/bin/env python3
import json
import os
import sys
import glob

def generate_dict(json_dir, out_file):
    subsystems = {}
    modules = {}

    # 1. 扫描目录下所有的 json 文件
    for filepath in glob.glob(os.path.join(json_dir, '*.json')):
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
            subsys_id = data.get('subsystem_id')
            subsys_name = data.get('service_name', f'Unknown_{subsys_id}') 
            if subsys_id is not None:
                subsystems[subsys_id] = subsys_name
            for mod_key, mod_info in data.get('modules', {}).items():
                mod_id = mod_info.get('id')
                mod_desc = mod_info.get('desc')
                if mod_id is not None and subsys_id is not None:
                    modules[(subsys_id, mod_id)] = mod_desc

    # 2. 开始生成 C++ 代码 (字典部分)
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "#include \"error_system/config/error_config.h\"",  # 👈 新增引入 config
        "#include <string>",
        "",
        "// 🚀 自动生成的极速字典表，请勿手动修改",
        "// 该文件利用 C++ 的 constexpr 和 Jump Table 实现了 O(1) 的无锁查询",
        "namespace error_system::dict {",
        "",
        "    /** @brief 极速获取子系统名称 */",
        "    constexpr const char* get_subsys_name(uint16_t subsys_id) noexcept {",
        "        switch(subsys_id) {"
    ]
    
    for sid, name in subsystems.items():
        lines.append(f"            case {sid}: return \"{name}\";")
    lines.append("            default: return \"未知子系统\";")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    
    lines.append("    /** @brief 极速获取模块名称 */")
    lines.append("    constexpr const char* get_module_name(uint16_t subsys_id, uint16_t mod_id) noexcept {")
    lines.append("        uint32_t combined = (static_cast<uint32_t>(subsys_id) << 16) | mod_id;")
    lines.append("        switch(combined) {")
    for (sid, mid), desc in modules.items():
        lines.append(f"            case ({sid}U << 16) | {mid}: return \"{desc}\";")
    lines.append("            default: return \"未知模块\";")
    lines.append("        }")
    lines.append("    }")
    lines.append("} // namespace error_system::dict")

    lines.append("")
    lines.append("namespace error_system::dict::detail {")
    lines.append("    struct auto_registrar_t {")
    lines.append("        auto_registrar_t() {")
    lines.append("            config::error_config_t::set_translator([](uint16_t subsys_id, uint16_t module_id) -> std::string {")
    lines.append("                const char* subsys = get_subsys_name(subsys_id);")
    lines.append("                const char* module = get_module_name(subsys_id, module_id);")
    lines.append("                std::string res;")
    lines.append("                res.reserve(64);")
    lines.append("                res += \"SubSys: \"; res += subsys;")
    lines.append("                res += \", Module: \"; res += module;")
    lines.append("                return res;")
    lines.append("            });")
    lines.append("        }")
    lines.append("    };")
    lines.append("    inline auto_registrar_t g_registrar_instance{};")
    lines.append("} // namespace error_system::dict::detail")

    # 4. 写入文件
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"✅ 成功生成全局字典与 C++17 注册器: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_dict.py <json_dir> <out_file>")
        sys.exit(1)
    generate_dict(sys.argv[1], sys.argv[2])
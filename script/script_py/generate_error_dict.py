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

    # 2. 开始生成大一统融合字典代码
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "#include <string_view>",
        "#include \"error_system/config/error_config.h\"",
        "#include \"error_system/translator/error_translator.h\"",
        "",
        "// 🚀 自动生成的极速静态数据源，请勿手动修改",
        "// 该文件利用 C++ 开源特化桩函数，将 JSON 编译期数据无缝喂给 error_translator_t",
        "namespace error_system::translator {", # 💡 核心修正：桩函数本身必须也定义在 translator 命名空间内
        "",
        "    /** @brief 供大一统翻译器调用的静态子系统查询（第一轨） */",
        "    std::string_view get_static_subsys_name(uint16_t subsys_id) noexcept {",
        "        switch(subsys_id) {"
    ]
    
    # 填充子系统静态分支
    for sid, name in subsystems.items():
        lines.append(f"            case {sid}: return \"{name}\";")
    lines.append("            default: return \"未知子系统\";")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    
    lines.append("    /** @brief 供大一统翻译器调用的静态模块查询（第一轨） */")
    lines.append("    std::string_view get_static_module_name(uint16_t subsys_id, uint16_t mod_id) noexcept {")
    lines.append("        uint32_t combined = (static_cast<uint32_t>(subsys_id) << 16) | mod_id;")
    lines.append("        switch(combined) {")
    
    # 填充模块静态分支
    for (sid, mid), desc in modules.items():
        lines.append(f"            case ({sid}U << 16) | {mid}: return \"{desc}\";")
    lines.append("            default: return \"未知模块\";")
    lines.append("        }")
    lines.append("    }")
    lines.append("} // namespace error_system::translator") # 💡 核心修正：对齐闭合

    # 3. 核心桥梁：C++17 全局全自动绑定闭包
    lines.append("")
    lines.append("namespace error_system::translator::detail {") # 💡 保持高度一致统一
    lines.append("    struct translator_binder_t {")
    lines.append("        translator_binder_t() {")
    lines.append("            // 💡 核心桥梁：自动把具有静动双轨合并能力的单例 translate() 路由挂载到全局配置上")
    lines.append("            config::error_config_t::set_translator([](uint16_t sid, uint16_t mid) -> std::string {")
    lines.append("                return error_translator_t::instance().translate(sid, mid);")
    lines.append("            });")
    lines.append("        }")
    lines.append("    };")
    lines.append("    inline translator_binder_t g_translator_binder_instance{};")
    lines.append("} // namespace error_system::translator::detail")

    # 4. 写入文件
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"✅ 成功生成大一统静动融合数据源与 C++17 绑定器: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_dict.py <json_dir> <out_file>")
        sys.exit(1)
    generate_dict(sys.argv[1], sys.argv[2])
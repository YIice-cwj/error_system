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

    # 2. 生成静态注册宏
    lines = [
        "#pragma once",
        "#include \"error_system/core/error_registry.h\"",
        "",
        "// 自动生成的子系统/模块名称注册宏，请勿手动修改",
        "// 通过 DEFINE_ERROR_CODE 宏自动注册到 error_registry_t 的映射表中",
        "namespace error_system::core {",
        "",
        "    /** @brief 静态注册所有子系统和模块名称到 error_registry_t */",
        "    struct subsystem_module_registrar_t {",
        "        subsystem_module_registrar_t() {",
        "            auto& registry = error_registry_t::instance();",
    ]

    # 填充子系统/模块注册
    for (sid, mid), mod_name in modules.items():
        subsys_name = subsystems.get(sid, "未知子系统")
        lines.append(f'            registry.register_subsystem_module({sid}, {mid}, "{subsys_name}", "{mod_name}");')

    lines.append("        }")
    lines.append("    };")
    lines.append("    inline subsystem_module_registrar_t global_subsystem_module_registrar_{};")
    lines.append("} // namespace error_system::core")

    # 4. 写入文件
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"✅ 成功生成子系统/模块名称注册宏: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_dict.py <json_dir> <out_file>")
        sys.exit(1)
    generate_dict(sys.argv[1], sys.argv[2])

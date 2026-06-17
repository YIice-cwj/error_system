#!/usr/bin/env python3
import json
import os
import sys

def __validate_safe_path(path, base_dir):
    """校验路径在 base_dir 内，防止路径穿越"""
    resolved = os.path.realpath(path)
    base_real = os.path.realpath(base_dir)
    if not resolved.startswith(base_real + os.sep) and resolved != base_real:
        print(f"[错误] 路径越界: {resolved} 不在 {base_real} 内", file=sys.stderr)
        sys.exit(1)
    return resolved

def generate_header(json_file, out_dir):
    script_dir = os.path.dirname(os.path.realpath(__file__))
    project_root = os.path.realpath(os.path.dirname(os.path.dirname(script_dir)))
    json_file = __validate_safe_path(json_file, project_root)
    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
        
    service_name = data.get('service_name', 'unknown')
    domain = data.get('domain', 'application')
    subsystem_id = data.get('subsystem_id', 0)
    namespace = data.get('namespace', 'errors')
    
    base_name = os.path.basename(json_file).replace('.json', '')
    out_file = os.path.join(out_dir, f"{base_name}.h")
    out_file = __validate_safe_path(out_file, out_dir)
    
    lines = [
        "#pragma once",
        "/**",
        f" * @file {base_name}.h",
        " * @brief 🚀 自动生成的错误码定义，请勿手动修改！",
        f" * @note Service: {service_name}, Subsystem ID: {subsystem_id}",
        " */",
        "",
        '#include "error_system/core/error_registry.h"',
        '#include "error_system/domain/system_domain.h"',
        "",
        f"namespace {namespace} {{",
        ""
    ]
    
    for err in data.get('errors', []):
        mod_info = data['modules'].get(err['module'])
        if not mod_info:
            print(f"⚠️ Warning: Module '{err['module']}' not found for error {err['name']}")
            continue
            
        module_id = mod_info['id']
        module_desc = mod_info['desc']
        
        lines.append(f"    // 模块: {module_desc}")
        lines.append(f"    DEFINE_ERROR_CODE(")
        lines.append(f"        {err['name']},")
        lines.append(f"        error_system::core::error_level_t::{err['level']},")
        lines.append(f"        error_system::domain::system_domain_t::{domain},")
        lines.append(f"        {subsystem_id},  // subsystem: {service_name}")
        lines.append(f"        {module_id},    // module: {err['module']}")
        lines.append(f"        {err['number']},")
        lines.append(f"        \"{err['desc']}\",")
        lines.append(f"        \"{service_name}\",  // 子系统名称")
        lines.append(f"        \"{module_desc}\"    // 模块名称")
        lines.append(f"    );")
        lines.append("")
        
    lines.append(f"}} // namespace {namespace}")
    
    os.makedirs(out_dir, exist_ok=True)
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"✅ 成功生成 C++ 头文件: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_codes.py <input_json_file> <output_directory>")
        sys.exit(1)
    generate_header(sys.argv[1], sys.argv[2])
#!/usr/bin/env python3
import json
import os
import sys
import tempfile

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
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"[错误] JSON 解析失败: {json_file}, 错误: {e}", file=sys.stderr)
        sys.exit(1)
        
    service_name = data.get('service_name', 'unknown')
    domain = data.get('domain', 'application')
    subsystem_id = data.get('subsystem_id', 0)
    namespace = data.get('namespace', 'errors')
    
    base_name = os.path.splitext(os.path.basename(json_file))[0]
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
        err_module = err.get('module')
        err_name = err.get('name')
        err_level = err.get('level')
        err_number = err.get('number')
        err_desc = err.get('desc', '')

        mod_info = data.get('modules', {}).get(err_module)
        if not mod_info:
            print(f"⚠️ Warning: Module '{err_module}' not found for error {err_name}")
            continue

        module_id = mod_info.get('id')
        module_desc = mod_info.get('desc', '')

        escaped_desc = err_desc.replace('\\', '\\\\').replace('"', '\\"')
        escaped_service_name = service_name.replace('\\', '\\\\').replace('"', '\\"')
        escaped_module_desc = module_desc.replace('\\', '\\\\').replace('"', '\\"')

        lines.append(f"    // 模块: {module_desc}")
        lines.append(f"    DEFINE_ERROR_CODE(")
        lines.append(f"        {err_name},")
        lines.append(f"        error_system::core::error_level_t::{err_level},")
        lines.append(f"        error_system::domain::system_domain_t::{domain},")
        lines.append(f"        {subsystem_id},  // subsystem: {service_name}")
        lines.append(f"        {module_id},    // module: {err_module}")
        lines.append(f"        {err_number},")
        lines.append(f"        \"{escaped_desc}\",")
        lines.append(f"        \"{escaped_service_name}\",  // 子系统名称")
        lines.append(f"        \"{escaped_module_desc}\"    // 模块名称")
        lines.append(f"    );")
        lines.append("")
        
    lines.append(f"}} // namespace {namespace}")
    
    os.makedirs(out_dir, exist_ok=True)
    fd, tmp_path = tempfile.mkstemp(dir=out_dir, suffix='.h')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
    except:
        os.unlink(tmp_path)
        raise
    os.replace(tmp_path, out_file)
    print(f"✅ 成功生成 C++ 头文件: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_codes.py <input_json_file> <output_directory>")
        sys.exit(1)
    generate_header(sys.argv[1], sys.argv[2])
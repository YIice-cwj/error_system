#!/usr/bin/env python3
"""错误码 JSON → C++ 头文件生成器

输入 JSON 格式（i18n 字段可选，缺失时默认用 desc 注册为 zh_CN）：

    {
        "namespace": "biz::trade_errors",
        "service_name": "交易服务",
        "domain": "application",
        "subsystem_id": 101,
        "modules": {
            "order": { "id": 1, "desc": "订单模块" }
        },
        "errors": [
            {
                "name": "ERR_ORDER_NOT_FOUND",
                "module": "order",
                "level": "error",
                "number": 1,
                "desc": "订单不存在或已删除",
                "i18n": {
                    "en_US": "Order not found or has been deleted",
                    "zh_CN": "订单不存在或已删除"
                }
            }
        ]
    }

生成内容：
  1. constexpr error_code_t 常量
  2. error_registrar_t 静态注册（注册错误码元数据 + 子系统/模块名，向后兼容）
  3. i18n 静态注册（注册错误码消息到 i18n_t，按 locale 分组）
"""
import json
import os
import sys
import tempfile


def _validate_safe_path(path, base_dir):
    """校验路径在 base_dir 内，防止路径穿越"""
    resolved = os.path.realpath(path)
    base_real = os.path.realpath(base_dir)
    if os.path.commonpath([resolved, base_real]) != base_real:
        print(f"[错误] 路径越界: {resolved} 不在 {base_real} 内", file=sys.stderr)
        sys.exit(1)
    return resolved


def _escape_cpp_string(text):
    """转义 C++ 字符串字面量"""
    return text.replace('\\', '\\\\').replace('"', '\\"')


def _normalize_i18n_entries(err):
    """从 error 条目解析 i18n 翻译表，返回 [(locale_str, message), ...]

    规则：
      - 若存在 i18n 字段，使用其中的 locale → message 映射
      - 否则用 desc 注册为 zh_CN（默认中文）
      - zh_CN 缺失时也用 desc 补齐，保证回退 locale 始终有值
    """
    desc = err.get('desc', '')
    raw_i18n = err.get('i18n')

    entries = []
    if isinstance(raw_i18n, dict) and raw_i18n:
        for locale, message in raw_i18n.items():
            entries.append((locale, message))
        # 保证 zh_CN 存在（回退目标）
        if 'zh_CN' not in raw_i18n:
            entries.append(('zh_CN', desc))
    else:
        entries.append(('zh_CN', desc))

    return entries


def generate_header(json_file, out_dir):
    json_file = _validate_safe_path(json_file, '/')
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
    out_file = _validate_safe_path(out_file, out_dir)

    lines = [
        "#pragma once",
        "/**",
        f" * @file {base_name}.h",
        " * @brief 自动生成的错误码定义与 i18n 注册，请勿手动修改！",
        f" * @note Service: {service_name}, Subsystem ID: {subsystem_id}",
        " * @details 包含三部分：",
        " *          1. constexpr error_code_t 常量定义",
        " *          2. error_registrar_t 静态注册（错误码元数据 + 子系统/模块名称，注册为 zh_CN）",
        " *          3. i18n 静态注册（错误码消息按 locale 注册到 i18n_t）",
        " */",
        "",
        '#include "error_system/core/error_registry.h"',
        '#include "error_system/domain/system_domain.h"',
        '#include "error_system/i18n/i18n.h"',
        "",
        f"namespace {namespace} {{",
        ""
    ]

    # 收集所有 i18n 注册（每个错误码一组）
    i18n_registrations = []

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

        escaped_desc = _escape_cpp_string(err_desc)
        escaped_service_name = _escape_cpp_string(service_name)
        escaped_module_desc = _escape_cpp_string(module_desc)

        lines.append(f"    // 模块: {module_desc}")
        lines.append(f"    DEFINE_ERROR_CODE(")
        lines.append(f"        {err_name},")
        lines.append(f"        error_system::core::error_level_t::{err_level},")
        lines.append(f"        error_system::domain::system_domain_t::{domain},")
        lines.append(f"        error_system::core::subsystem_id_t{{{subsystem_id}}},  // subsystem: {service_name}")
        lines.append(f"        error_system::core::module_id_t{{{module_id}}},    // module: {err_module}")
        lines.append(f"        error_system::core::error_number_t{{{err_number}}},")
        lines.append(f"        \"{escaped_desc}\",")
        lines.append(f"        \"{escaped_service_name}\",  // 子系统名称")
        lines.append(f"        \"{escaped_module_desc}\"    // 模块名称")
        lines.append(f"    );")
        lines.append("")

        # 收集 i18n 翻译条目
        i18n_entries = _normalize_i18n_entries(err)
        i18n_registrations.append((err_name, i18n_entries))

    # 生成 i18n 静态注册块（每个错误码一个 registrar，避免单个巨型结构体）
    if i18n_registrations:
        lines.append("    // ===== i18n 静态注册（错误码消息按 locale 注册到 i18n_t） =====")
        lines.append("    namespace i18n_registration_detail_ {")
        lines.append("")
        for err_name, entries in i18n_registrations:
            struct_name = f"{err_name}_i18n_registrar_"
            instance_name = f"{err_name}_i18n_instance_"
            lines.append(f"        /// @brief 注册 {err_name} 的多语言消息到 i18n_t")
            lines.append(f"        struct {struct_name} {{")
            lines.append(f"            {struct_name}() noexcept {{")
            lines.append(f"                auto& catalog = ::error_system::i18n::i18n_t::instance();")
            for locale, message in entries:
                escaped_message = _escape_cpp_string(message)
                lines.append(f"                catalog.register_message(::error_system::i18n::locale_t::{locale},")
                lines.append(f"                                      {err_name},")
                lines.append(f"                                      \"{escaped_message}\");")
            lines.append(f"            }}")
            lines.append(f"        }};")
            lines.append(f"        inline const {struct_name} {instance_name}{{}};")
            lines.append("")
        lines.append("    }  // namespace i18n_registration_detail_")
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
    print(f"✅ 成功生成 C++ 头文件（含 i18n 注册）: {out_file}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_codes.py <input_json_file> <output_directory>")
        sys.exit(1)
    generate_header(sys.argv[1], sys.argv[2])

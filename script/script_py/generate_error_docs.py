#!/usr/bin/env python3
import json
import os
import sys
import glob
from datetime import datetime


def __validate_safe_path(path, base_dir):
    """校验路径在 base_dir 内，防止路径穿越"""
    resolved = os.path.realpath(path)
    base_real = os.path.realpath(base_dir)
    if not resolved.startswith(base_real + os.sep) and resolved != base_real:
        print(f"[错误] 路径越界: {resolved} 不在 {base_real} 内", file=sys.stderr)
        sys.exit(1)
    return resolved


def generate_markdown(json_dir, out_file):
    script_dir = os.path.dirname(os.path.realpath(__file__))
    project_root = os.path.realpath(os.path.dirname(os.path.dirname(script_dir)))
    json_dir = __validate_safe_path(json_dir, project_root)
    out_file = __validate_safe_path(out_file, project_root)

    lines = []
    lines.append("# 📖 全局业务错误码字典")
    lines.append(f"> 🕒 **最后更新时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("> ⚠️ **注意**: 本文档由脚本自动生成，请勿手动修改！如需新增错误码，请在 `config/errors/` 目录下的 JSON 文件中维护。\n")
    lines.append("---")

    # 1. 读取所有 JSON 数据
    services_data = []
    for filepath in glob.glob(os.path.join(json_dir, '*.json')):
        with open(filepath, 'r', encoding='utf-8') as f:
            services_data.append(json.load(f))
    
    # 按 subsystem_id 排序，保证每次生成的文档顺序稳定
    services_data.sort(key=lambda x: x.get('subsystem_id', 9999))

    # 2. 生成目录 (TOC)
    lines.append("## 📑 目录")
    for data in services_data:
        service_name = data.get('service_name', '未知服务')
        subsys_id = data.get('subsystem_id', 0)
        # Markdown 锚点链接规范：转小写，空格换横杠
        anchor = f"{service_name}-subsys-{subsys_id}".replace(" ", "-").lower()
        lines.append(f"- [{service_name} (ID: {subsys_id})](#{anchor})")
    lines.append("\n---\n")

    # 3. 生成每个服务的具体表格
    for data in services_data:
        service_name = data.get('service_name', '未知服务')
        subsys_id = data.get('subsystem_id', 0)
        domain = data.get('domain', 'application')
        
        lines.append(f"<h2 id=\"{service_name}-subsys-{subsys_id}\">🚀 {service_name} <span>(Subsys: {subsys_id} | Domain: {domain})</span></h2>\n")
        
        # 构建模块映射字典方便查询
        modules_map = {
            k: v['desc'] for k, v in data.get('modules', {}).items()
        }

        # 绘制 Markdown 表头
        lines.append("| 错误宏名称 (Code) | 级别 (Level) | 所属模块 | 业务描述 (Description) |")
        lines.append("|---|:---:|---|---|")

        # 填充错误码表格内容
        for err in data.get('errors', []):
            name = f"`{err['name']}`"
            
            # 用 emoji 让错误级别更直观
            level = err.get('level', 'info')
            level_str = level
            if level == 'error': level_str = "🔴 ERROR"
            elif level == 'warn': level_str = "🟠 WARN"
            elif level == 'fatal': level_str = "💀 FATAL"
            elif level == 'info': level_str = "🔵 INFO"

            module_desc = modules_map.get(err.get('module'), err.get('module'))
            desc = err.get('desc', '')

            lines.append(f"| {name} | {level_str} | {module_desc} | {desc} |")
        
        lines.append("\n[⬆️ 回到顶部](#-目录)\n")
        lines.append("---\n")

    # 4. 写入文件
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f"✅ 成功生成 Markdown 文档: {out_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python generate_error_docs.py <json_dir> <out_markdown_file>")
        sys.exit(1)
    generate_markdown(sys.argv[1], sys.argv[2])
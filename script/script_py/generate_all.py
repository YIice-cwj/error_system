#!/usr/bin/env python3
"""
统一错误码生成入口脚本。
一次调用完成：业务错误码头文件 + 全局字典 + Markdown 文档 三项生成。

用法:
    python generate_all.py                    # 默认输出到 build/generated_errors
    python generate_all.py build-release      # 指定构建目录
    python generate_all.py /path/to/output    # 指定绝对输出路径
"""

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="统一错误码生成入口脚本")
    parser.add_argument(
        "build_dir",
        nargs="?",
        default="build",
        help="构建目录（相对于项目根目录），默认为 build")
    args = parser.parse_args()

    # 动态定位项目根目录（脚本位于 script/script_py/ 下）
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))

    json_dir = os.path.join(project_root, "config", "errors")

    # 支持绝对路径和相对路径
    if os.path.isabs(args.build_dir):
        build_dir = args.build_dir
    else:
        build_dir = os.path.join(project_root, args.build_dir)

    output_dir = os.path.join(build_dir, "generated_errors", "include")
    generated_dir = os.path.join(build_dir, "generated_errors")

    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(generated_dir, exist_ok=True)

    py_dir = os.path.dirname(os.path.abspath(__file__))

    code_script = os.path.join(py_dir, "generate_error_codes.py")
    dict_script = os.path.join(py_dir, "generate_error_dict.py")
    docs_script = os.path.join(py_dir, "generate_error_docs.py")

    dict_output = os.path.join(output_dir, "error_dict.h")
    docs_output = os.path.join(generated_dir, "error_dictionary.md")

    print("=" * 60)
    print("  统一错误码生成")
    print("=" * 60)

    # 阶段一：业务头文件
    print("\n[1/3] 生成业务模块 C++ 头文件...")
    count = 0
    for filename in sorted(os.listdir(json_dir)):
        if not filename.endswith(".json"):
            continue
        json_file = os.path.join(json_dir, filename)
        name = filename.replace(".json", "")
        print(f"  → {name}.h")
        result = subprocess.run(
            [sys.executable, code_script, json_file, output_dir],
            capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  失败: {result.stderr.strip()}")
            sys.exit(1)
        count += 1
    print(f"  成功生成 {count} 个头文件")

    # 阶段二：全局字典（含 ID 冲突检测）
    print("\n[2/3] 生成全局错误字典...")
    result = subprocess.run(
        [sys.executable, dict_script, json_dir, dict_output],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  失败: {result.stderr.strip()}")
        sys.exit(1)
    print(result.stdout.strip())

    # 阶段三：Markdown 文档
    print("\n[3/3] 生成 Markdown 文档...")
    result = subprocess.run(
        [sys.executable, docs_script, json_dir, docs_output],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  失败: {result.stderr.strip()}")
        sys.exit(1)
    print(result.stdout.strip())

    print("\n" + "=" * 60)
    print("  全部完成!")
    print("=" * 60)


if __name__ == "__main__":
    main()
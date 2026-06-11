#!/bin/bash

# 1. 动态获取项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 2. 定义目录和脚本路径
JSON_DIR="$PROJECT_ROOT/config/errors"
OUTPUT_DIR="$PROJECT_ROOT/build/generated_errors/include"
GENERATED_DIR="$PROJECT_ROOT/build/generated_errors"

CODE_SCRIPT="$SCRIPT_DIR/script_py/generate_error_codes.py"
DICT_SCRIPT="$SCRIPT_DIR/script_py/generate_error_dict.py"
DOCS_SCRIPT="$SCRIPT_DIR/script_py/generate_error_docs.py"

DICT_OUTPUT="$OUTPUT_DIR/error_dict.h"
DOCS_OUTPUT="$GENERATED_DIR/error_dictionary.md"

echo "================================================="
echo "   🚀 开始一键生成 Error System 错误码与文档..."
echo "================================================="

if [ ! -d "$JSON_DIR" ]; then
    echo "❌ 错误: 找不到配置目录 $JSON_DIR"
    exit 1
fi

# 确保输出目录存在
mkdir -p "$OUTPUT_DIR"
mkdir -p "$GENERATED_DIR"

# ---------------------------------------------------------
# 阶段一：业务头文件生成
# ---------------------------------------------------------
echo "-------------------------------------------------"
echo " ⚙️ [1/3] 正在生成业务模块 C++ 头文件 (.h)"
echo "-------------------------------------------------"
count=0
for json_file in "$JSON_DIR"/*.json; do
    if [ -f "$json_file" ]; then
        # 提取文件名 (例如 trade_service_errors.json)
        filename=$(basename -- "$json_file")
        # 去除后缀名 (得到 trade_service_errors)
        filename_no_ext="${filename%.*}"
        
        echo "  ⏳ 读取 $filename ➡️ 生成 ${filename_no_ext}.h"
        python3 "$CODE_SCRIPT" "$json_file" "$OUTPUT_DIR"
        
        if [ $? -eq 0 ]; then
            ((count++))
        else
            echo "  ❌ 失败: $filename 生成 ${filename_no_ext}.h 报错！"
            exit 1
        fi
    fi
done

# ---------------------------------------------------------
# 阶段二：全局字典生成
# ---------------------------------------------------------
echo "-------------------------------------------------"
echo " ⚙️ [2/3] 正在生成 C++ 全局极速字典"
echo "-------------------------------------------------"
echo "  ⏳ 汇总全部 JSON 数据 ➡️ 生成 error_dict.h"
python3 "$DICT_SCRIPT" "$JSON_DIR" "$DICT_OUTPUT"

if [ $? -ne 0 ]; then
    echo "  ❌ 全局字典 error_dict.h 生成失败！"
    exit 1
fi

# ---------------------------------------------------------
# 阶段三：Markdown 文档生成
# ---------------------------------------------------------
echo "-------------------------------------------------"
echo " ⚙️ [3/3] 正在生成 Markdown 错误码字典文档"
echo "-------------------------------------------------"
echo "  ⏳ 汇总全部 JSON 数据 ➡️ 生成 error_dictionary.md"
python3 "$DOCS_SCRIPT" "$JSON_DIR" "$DOCS_OUTPUT"

if [ $? -ne 0 ]; then
    echo "  ❌ Markdown 文档 error_dictionary.md 生成失败！"
    exit 1
fi

# ---------------------------------------------------------
# 结果汇总
# ---------------------------------------------------------
echo "================================================="
echo "✅ 大功告成！构建摘要如下："
echo "📦 业务头文件: 成功生成了 $count 个 (.h)"
echo "📕 全局字典  : error_dict.h"
echo "📝 说明文档  : error_dictionary.md"
echo "================================================="
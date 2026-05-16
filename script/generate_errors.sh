#!/bin/bash
# 1. 动态获取项目根目录 (无论你在哪个目录执行这个脚本，都能正确找到路径)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 2. 定义输入和输出路径
JSON_DIR="$PROJECT_ROOT/config/errors"
OUTPUT_DIR="$PROJECT_ROOT/include/generated" # 手动生成时，可以直接放到源码目录的 generated 下
PYTHON_SCRIPT="$SCRIPT_DIR/script_py/generate_error_codes.py"

echo "========================================"
echo "   🚀 开始手动生成 C++ 错误码头文件..."
echo "========================================"

# 3. 检查输入目录是否存在
if [ ! -d "$JSON_DIR" ]; then
    echo "❌ 错误: 找不到配置目录 $JSON_DIR"
    exit 1
fi

# 4. 确保输出目录存在
mkdir -p "$OUTPUT_DIR"

# 5. 遍历并处理所有 JSON 文件
count=0
for json_file in "$JSON_DIR"/*.json; do
    # 检查是否存在 .json 文件（防止目录为空时通配符不展开）
    if [ -f "$json_file" ]; then
        filename=$(basename -- "$json_file")
        echo "⏳ 正在处理: $filename"
        
        # 调用 Python 脚本
        python3 "$PYTHON_SCRIPT" "$json_file" "$OUTPUT_DIR"
        
        # 检查 Python 脚本执行是否成功
        if [ $? -eq 0 ]; then
            ((count++))
        else
            echo "❌ 失败: $filename 处理报错！"
            exit 1
        fi
    fi
done

echo "========================================"
echo "✅ 成功！共生成了 $count 个错误码头文件。"
echo "📁 输出目录: $OUTPUT_DIR"
echo "========================================"
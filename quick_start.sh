#!/bin/bash
# USB速度检测工具 - 快速开始脚本

set -e

echo "╔═══════════════════════════════════════════════════════╗"
echo "║      USB速度检测工具 - 快速开始                      ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# 检查是否在正确的目录
if [ ! -f "usb_speed_detector.c" ]; then
    echo "❌ 错误: 请在包含源代码的目录中运行此脚本"
    exit 1
fi

# 1. 检查依赖
echo "📦 步骤 1/3: 检查编译依赖..."
if ! pkg-config --exists glib-2.0 libudev; then
    echo "❌ 缺少依赖包，正在安装..."
    echo "   需要安装: libglib2.0-dev libudev-dev"
    
    if [ -f /etc/debian_version ]; then
        echo "   检测到Debian/Ubuntu系统"
        sudo apt-get update
        sudo apt-get install -y build-essential libglib2.0-dev libudev-dev
    elif [ -f /etc/redhat-release ]; then
        echo "   检测到RHEL/Fedora系统"
        sudo dnf install -y gcc glib2-devel systemd-devel
    else
        echo "⚠️  未知的Linux发行版，请手动安装依赖"
        exit 1
    fi
else
    echo "✅ 依赖已安装"
fi

# 2. 编译工具
echo ""
echo "🔨 步骤 2/3: 编译工具..."
make -f Makefile.usb_tools

if [ $? -eq 0 ]; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    exit 1
fi

# 3. 显示使用说明
echo ""
echo "🎉 步骤 3/3: 安装完成！"
echo ""
echo "═══════════════════════════════════════════════════════"
echo "📖 使用说明"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "1️⃣  查看当前连接的USB设备:"
echo "   lsblk"
echo ""
echo "2️⃣  检测指定U盘的速度:"
echo "   sudo ./usb_speed_detector /dev/sdX"
echo "   （将 sdX 替换为实际设备名，如 sdb）"
echo ""
echo "3️⃣  实时监听U盘插入:"
echo "   sudo ./usb_monitor"
echo "   （后台运行，插入U盘时自动显示速度信息）"
echo ""
echo "═══════════════════════════════════════════════════════"
echo "💡 现在做什么？"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "选项 A: 立即测试已连接的U盘"
echo "选项 B: 启动监听器，等待插入新U盘"
echo "选项 Q: 退出"
echo ""
read -p "请选择 [A/B/Q]: " choice

case $choice in
    [Aa])
        echo ""
        echo "📋 当前连接的存储设备:"
        lsblk -d -o NAME,SIZE,TYPE,VENDOR,MODEL | grep -E "NAME|disk"
        echo ""
        read -p "请输入设备名（如 sdb）: " device
        
        if [ -z "$device" ]; then
            echo "❌ 未输入设备名"
            exit 1
        fi
        
        # 添加 /dev/ 前缀（如果用户没有输入）
        if [[ ! $device == /dev/* ]]; then
            device="/dev/$device"
        fi
        
        if [ ! -b "$device" ]; then
            echo "❌ 设备不存在: $device"
            exit 1
        fi
        
        echo ""
        echo "🔍 正在检测 $device ..."
        echo ""
        sudo ./usb_speed_detector "$device"
        ;;
    [Bb])
        echo ""
        echo "🔍 启动USB监听器..."
        echo "   请插入U盘，程序将自动显示速度信息"
        echo "   按 Ctrl+C 退出"
        echo ""
        sudo ./usb_monitor
        ;;
    [Qq])
        echo ""
        echo "👋 再见！"
        echo ""
        echo "💡 提示: 使用 'lsblk' 查看设备，然后运行:"
        echo "   sudo ./usb_speed_detector /dev/sdX"
        ;;
    *)
        echo ""
        echo "❌ 无效的选择"
        exit 1
        ;;
esac

echo ""
echo "✅ 完成！"

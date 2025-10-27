/* USB设备插入监听工具 - 自动检测U盘速度
 * 编译: gcc -o usb_monitor usb_monitor.c `pkg-config --cflags --libs glib-2.0 libudev`
 * 用法: sudo ./usb_monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libudev.h>

typedef struct {
    int speed_mbps;
    const char *version;
    double theoretical_mbs;
} SpeedInfo;

SpeedInfo get_speed_info(int speed) {
    SpeedInfo info = {speed, "未知", 0};
    
    switch (speed) {
        case 1:
            info.version = "USB 1.0";
            info.theoretical_mbs = 0.1875;
            break;
        case 12:
            info.version = "USB 1.1";
            info.theoretical_mbs = 1.5;
            break;
        case 480:
            info.version = "USB 2.0";
            info.theoretical_mbs = 60;
            break;
        case 5000:
            info.version = "USB 3.0/3.1 Gen1";
            info.theoretical_mbs = 625;
            break;
        case 10000:
            info.version = "USB 3.1 Gen2";
            info.theoretical_mbs = 1250;
            break;
        case 20000:
            info.version = "USB 3.2";
            info.theoretical_mbs = 2500;
            break;
        default:
            info.theoretical_mbs = speed / 8.0;
    }
    
    return info;
}

void print_usb_device(struct udev_device *dev, const char *devnode) {
    struct udev_device *usb_dev = udev_device_get_parent_with_subsystem_devtype(
        dev, "usb", "usb_device");
    
    if (!usb_dev) return;
    
    const char *speed_str = udev_device_get_sysattr_value(usb_dev, "speed");
    const char *product = udev_device_get_sysattr_value(usb_dev, "product");
    const char *manufacturer = udev_device_get_sysattr_value(usb_dev, "manufacturer");
    
    if (!speed_str) return;
    
    int speed = atoi(speed_str);
    SpeedInfo info = get_speed_info(speed);
    
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  🔌 检测到USB存储设备插入                             ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    g_print("⏰ 时间:       %s", g_date_time_format(g_date_time_new_now_local(), "%Y-%m-%d %H:%M:%S\n"));
    g_print("📁 设备:       %s\n", devnode ? devnode : "未知");
    g_print("🏭 制造商:     %s\n", manufacturer ? manufacturer : "未知");
    g_print("📦 产品:       %s\n", product ? product : "未知");
    g_print("───────────────────────────────────────────────────────\n");
    g_print("⚡ USB版本:    %s\n", info.version);
    g_print("🚀 协商速度:   %d Mbps\n", info.speed_mbps);
    g_print("📊 理论速度:   %.2f MB/s\n", info.theoretical_mbs);
    g_print("═══════════════════════════════════════════════════════\n");
    
    // 给出性能评估建议
    if (info.speed_mbps <= 12) {
        g_print("⚠️  这是一个USB 1.x设备，速度非常慢，建议更换！\n");
    } else if (info.speed_mbps == 480) {
        g_print("ℹ️  这是USB 2.0设备，实际速度通常在20-40 MB/s之间。\n");
        g_print("   如果您的电脑有USB 3.0口（蓝色），建议使用USB 3.0 U盘。\n");
    } else if (info.speed_mbps >= 5000) {
        g_print("✅ 这是USB 3.0+设备，具备高速传输能力。\n");
        g_print("   实际速度取决于U盘质量，优质U盘可达100-400 MB/s。\n");
    }
    
    g_print("\n💡 提示: 运行以下命令测试实际速度:\n");
    g_print("   sudo ./usb_speed_detector %s\n\n", devnode ? devnode : "/dev/sdX");
}

static gboolean monitor_callback(GIOChannel *channel, GIOCondition condition, gpointer data) {
    struct udev_monitor *mon = (struct udev_monitor *)data;
    struct udev_device *dev;
    
    dev = udev_monitor_receive_device(mon);
    if (dev) {
        const char *action = udev_device_get_action(dev);
        const char *devnode = udev_device_get_devnode(dev);
        const char *devtype = udev_device_get_devtype(dev);
        
        // 只处理磁盘设备的添加事件（不包括分区）
        if (g_strcmp0(action, "add") == 0 && 
            devnode && 
            g_strcmp0(devtype, "disk") == 0) {
            print_usb_device(dev, devnode);
        }
        
        udev_device_unref(dev);
    }
    
    return TRUE;
}

int main() {
    struct udev *udev;
    struct udev_monitor *mon;
    GMainLoop *loop;
    GIOChannel *channel;
    int fd;
    
    g_print("╔═══════════════════════════════════════════════════════╗\n");
    g_print("║      USB设备速度监听器 v1.0                          ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    g_print("\n🎯 功能说明:\n");
    g_print("   • 自动检测USB存储设备插入\n");
    g_print("   • 显示USB协议版本和理论速度\n");
    g_print("   • 评估设备性能等级\n");
    g_print("\n💡 重要提示:\n");
    g_print("   即使显示相同的USB版本（如USB 3.0），不同U盘的\n");
    g_print("   实际读写速度也可能相差3-10倍！这是因为:\n");
    g_print("   • 闪存芯片质量差异巨大\n");
    g_print("   • 控制器性能有高有低\n");
    g_print("   • 固件优化程度不同\n");
    g_print("\n🔍 正在监听USB设备插入事件...\n");
    g_print("   (按 Ctrl+C 退出)\n");
    g_print("═══════════════════════════════════════════════════════\n");
    
    // 初始化udev
    udev = udev_new();
    if (!udev) {
        g_printerr("❌ 无法初始化udev\n");
        return 1;
    }
    
    // 创建监听器
    mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        g_printerr("❌ 无法创建udev监听器\n");
        udev_unref(udev);
        return 1;
    }
    
    // 只监听块设备
    udev_monitor_filter_add_match_subsystem_devtype(mon, "block", NULL);
    udev_monitor_enable_receiving(mon);
    
    // 设置GLib事件循环
    fd = udev_monitor_get_fd(mon);
    channel = g_io_channel_unix_new(fd);
    g_io_add_watch(channel, G_IO_IN, monitor_callback, mon);
    
    // 运行主循环
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    
    // 清理
    g_io_channel_unref(channel);
    udev_monitor_unref(mon);
    udev_unref(udev);
    g_main_loop_unref(loop);
    
    return 0;
}

/* 简化版USB信息查看器 - 无需root权限
 * 只显示USB协议速度，不进行实际速度测试
 * 编译: gcc -o usb_info_simple usb_info_simple.c `pkg-config --cflags --libs glib-2.0 libudev`
 * 用法: ./usb_info_simple [设备路径]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libudev.h>

typedef struct {
    const char *device;
    const char *manufacturer;
    const char *product;
    const char *serial;
    const char *vid;
    const char *pid;
    int speed_mbps;
    const char *usb_version;
    double theoretical_mbs;
} UsbDeviceInfo;

void get_speed_description(int speed_mbps, const char **version, double *theoretical_mbs) {
    switch (speed_mbps) {
        case 1:
            *version = "USB 1.0 (Low Speed)";
            *theoretical_mbs = 0.1875;
            break;
        case 12:
            *version = "USB 1.1 (Full Speed)";
            *theoretical_mbs = 1.5;
            break;
        case 480:
            *version = "USB 2.0 (High Speed)";
            *theoretical_mbs = 60.0;
            break;
        case 5000:
            *version = "USB 3.0/3.1 Gen1 (SuperSpeed)";
            *theoretical_mbs = 625.0;
            break;
        case 10000:
            *version = "USB 3.1 Gen2 (SuperSpeed+)";
            *theoretical_mbs = 1250.0;
            break;
        case 20000:
            *version = "USB 3.2 Gen2x2";
            *theoretical_mbs = 2500.0;
            break;
        default:
            *version = "未知USB版本";
            *theoretical_mbs = speed_mbps / 8.0;
    }
}

gboolean get_device_info(const char *device_path, UsbDeviceInfo *info) {
    struct udev *udev;
    struct udev_device *dev = NULL;
    struct udev_device *usb_dev = NULL;
    gboolean result = FALSE;
    
    memset(info, 0, sizeof(UsbDeviceInfo));
    info->device = device_path;
    
    udev = udev_new();
    if (!udev) {
        g_printerr("❌ 无法初始化udev\n");
        return FALSE;
    }
    
    // 获取块设备
    const char *dev_name = g_path_get_basename(device_path);
    dev = udev_device_new_from_subsystem_sysname(udev, "block", dev_name);
    
    if (!dev) {
        g_printerr("❌ 无法找到设备: %s\n", device_path);
        goto cleanup;
    }
    
    // 查找USB父设备
    usb_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    
    if (!usb_dev) {
        g_printerr("❌ 这不是一个USB设备\n");
        goto cleanup;
    }
    
    // 获取设备信息
    const char *speed_str = udev_device_get_sysattr_value(usb_dev, "speed");
    info->manufacturer = udev_device_get_sysattr_value(usb_dev, "manufacturer");
    info->product = udev_device_get_sysattr_value(usb_dev, "product");
    info->serial = udev_device_get_sysattr_value(usb_dev, "serial");
    info->vid = udev_device_get_sysattr_value(usb_dev, "idVendor");
    info->pid = udev_device_get_sysattr_value(usb_dev, "idProduct");
    
    if (speed_str) {
        info->speed_mbps = atoi(speed_str);
        get_speed_description(info->speed_mbps, &info->usb_version, &info->theoretical_mbs);
        result = TRUE;
    }
    
cleanup:
    if (dev) udev_device_unref(dev);
    udev_unref(udev);
    return result;
}

void print_device_info(const UsbDeviceInfo *info) {
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  📱 USB设备信息                                       ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    g_print("设备路径:     %s\n", info->device);
    g_print("制造商:       %s\n", info->manufacturer ? info->manufacturer : "未知");
    g_print("产品名称:     %s\n", info->product ? info->product : "未知");
    g_print("序列号:       %s\n", info->serial ? info->serial : "未知");
    g_print("VID:PID:      %s:%s\n", 
            info->vid ? info->vid : "????", 
            info->pid ? info->pid : "????");
    
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  ⚡ USB速度信息                                        ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    g_print("USB版本:      %s\n", info->usb_version);
    g_print("协商速度:     %d Mbps\n", info->speed_mbps);
    g_print("理论最大值:   %.2f MB/s\n", info->theoretical_mbs);
    
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  📊 性能评估                                          ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    
    if (info->speed_mbps <= 12) {
        g_print("🔴 低速设备 (USB 1.x)\n");
        g_print("   • 这是非常老旧的USB标准\n");
        g_print("   • 实际速度: 通常不到 2 MB/s\n");
        g_print("   • 建议: 强烈建议更换为USB 3.0设备\n");
    } else if (info->speed_mbps == 480) {
        g_print("🟡 中速设备 (USB 2.0)\n");
        g_print("   • 这是较旧的USB标准\n");
        g_print("   • 实际速度: 通常在 20-40 MB/s 之间\n");
        g_print("   • 不同U盘的实际速度差异: 可达 2-3 倍\n");
        g_print("   • 建议: 如果经常传输大文件，考虑升级到USB 3.0\n");
    } else if (info->speed_mbps >= 5000) {
        g_print("🟢 高速设备 (USB 3.0+)\n");
        g_print("   • 这是现代USB标准，具备高速传输能力\n");
        g_print("   • 实际速度: 根据U盘质量差异巨大\n");
        g_print("     - 低端U盘: 50-80 MB/s\n");
        g_print("     - 中端U盘: 100-200 MB/s\n");
        g_print("     - 高端U盘: 200-400 MB/s\n");
        g_print("   • 不同U盘的实际速度差异: 可达 5-10 倍！\n");
        g_print("   • 关键因素: 闪存芯片质量、控制器性能、是否有缓存\n");
    }
    
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  💡 重要说明                                          ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n");
    g_print("协商速度 vs 实际速度:\n");
    g_print("• 协商速度: USB总线协议的理论带宽（上面显示的）\n");
    g_print("• 实际速度: U盘硬件的真实读写性能\n");
    g_print("\n");
    g_print("即使协商速度相同，不同U盘的实际速度也可能相差数倍！\n");
    g_print("这取决于:\n");
    g_print("  1. 闪存芯片质量 (SLC > MLC > TLC > QLC)\n");
    g_print("  2. 控制器性能 (多通道 > 单通道)\n");
    g_print("  3. 是否有DRAM缓存\n");
    g_print("  4. 固件优化程度\n");
    g_print("\n");
    g_print("要测试实际速度，请运行:\n");
    g_print("  sudo ./usb_speed_detector %s\n", info->device);
    g_print("═══════════════════════════════════════════════════════\n");
}

void list_all_usb_devices() {
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *entry;
    int device_count = 0;
    
    g_print("\n╔═══════════════════════════════════════════════════════╗\n");
    g_print("║  🔍 扫描USB存储设备                                   ║\n");
    g_print("╚═══════════════════════════════════════════════════════╝\n\n");
    
    udev = udev_new();
    if (!udev) {
        g_printerr("❌ 无法初始化udev\n");
        return;
    }
    
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    
    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);
        
        // 只显示磁盘设备，不显示分区
        const char *devtype = udev_device_get_devtype(dev);
        if (g_strcmp0(devtype, "disk") != 0) {
            udev_device_unref(dev);
            continue;
        }
        
        // 检查是否是USB设备
        struct udev_device *usb_dev = udev_device_get_parent_with_subsystem_devtype(
            dev, "usb", "usb_device");
        
        if (usb_dev) {
            const char *devnode = udev_device_get_devnode(dev);
            const char *product = udev_device_get_sysattr_value(usb_dev, "product");
            const char *speed_str = udev_device_get_sysattr_value(usb_dev, "speed");
            
            int speed = speed_str ? atoi(speed_str) : 0;
            const char *version;
            double theoretical;
            get_speed_description(speed, &version, &theoretical);
            
            device_count++;
            g_print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
            g_print("设备 %d: %s\n", device_count, devnode);
            g_print("名称:   %s\n", product ? product : "未知");
            g_print("速度:   %s (%d Mbps, 理论%.0f MB/s)\n", 
                    version, speed, theoretical);
            g_print("\n查看详情: ./usb_info_simple %s\n", devnode);
        }
        
        udev_device_unref(dev);
    }
    
    if (device_count == 0) {
        g_print("❌ 未找到USB存储设备\n");
        g_print("\n💡 提示: 请插入U盘后重试\n");
    } else {
        g_print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        g_print("\n✅ 共找到 %d 个USB存储设备\n", device_count);
    }
    
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        g_print("═══════════════════════════════════════════════════════\n");
        g_print("  USB设备信息查看器 v1.0 (简化版)\n");
        g_print("═══════════════════════════════════════════════════════\n");
        g_print("\n💡 此工具无需root权限，只显示USB协议速度\n");
        g_print("\n用法:\n");
        g_print("  %s <设备路径>    - 查看指定设备信息\n", argv[0]);
        g_print("  %s --list        - 列出所有USB设备\n", argv[0]);
        g_print("\n示例:\n");
        g_print("  %s /dev/sdb\n", argv[0]);
        g_print("  %s --list\n", argv[0]);
        g_print("\n");
        
        // 自动列出设备
        list_all_usb_devices();
        return 0;
    }
    
    if (g_strcmp0(argv[1], "--list") == 0 || g_strcmp0(argv[1], "-l") == 0) {
        list_all_usb_devices();
        return 0;
    }
    
    UsbDeviceInfo info;
    if (get_device_info(argv[1], &info)) {
        print_device_info(&info);
        return 0;
    } else {
        return 1;
    }
}

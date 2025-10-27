/* USB U盘速度检测工具
 * 编译: gcc -o usb_speed_detector usb_speed_detector.c `pkg-config --cflags --libs glib-2.0 libudev`
 * 用法: ./usb_speed_detector /dev/sdb
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <glib.h>
#include <libudev.h>

// USB速度类型
typedef struct {
    int protocol_speed_mbps;  // 协议速度 (Mbps)
    const char *usb_version;  // USB版本
    double theoretical_speed_mbs;  // 理论速度 (MB/s)
} UsbSpeedInfo;

// 解析USB速度
UsbSpeedInfo parse_usb_speed(const char *speed_str) {
    UsbSpeedInfo info = {0};
    
    if (!speed_str) {
        info.usb_version = "未知";
        return info;
    }
    
    int speed = atoi(speed_str);
    info.protocol_speed_mbps = speed;
    
    switch (speed) {
        case 1:     // 1.5 Mbps
            info.usb_version = "USB 1.0 (Low Speed)";
            info.theoretical_speed_mbs = 0.1875;  // 1.5/8
            break;
        case 12:    // 12 Mbps
            info.usb_version = "USB 1.1 (Full Speed)";
            info.theoretical_speed_mbs = 1.5;  // 12/8
            break;
        case 480:   // 480 Mbps
            info.usb_version = "USB 2.0 (High Speed)";
            info.theoretical_speed_mbs = 60;  // 480/8
            break;
        case 5000:  // 5 Gbps
            info.usb_version = "USB 3.0/3.1 Gen1 (SuperSpeed)";
            info.theoretical_speed_mbs = 625;  // 5000/8
            break;
        case 10000: // 10 Gbps
            info.usb_version = "USB 3.1 Gen2 (SuperSpeed+)";
            info.theoretical_speed_mbs = 1250;  // 10000/8
            break;
        case 20000: // 20 Gbps
            info.usb_version = "USB 3.2 Gen2x2";
            info.theoretical_speed_mbs = 2500;  // 20000/8
            break;
        default:
            info.usb_version = "未知";
            info.theoretical_speed_mbs = speed / 8.0;
    }
    
    return info;
}

// 获取USB设备的协议速度
gboolean get_usb_protocol_speed(const char *device_path, UsbSpeedInfo *info) {
    struct udev *udev;
    struct udev_device *dev = NULL;
    struct udev_device *usb_dev = NULL;
    gboolean result = FALSE;
    
    udev = udev_new();
    if (!udev) {
        g_printerr("❌ 无法初始化udev\n");
        return FALSE;
    }
    
    // 获取块设备
    dev = udev_device_new_from_subsystem_sysname(udev, "block", 
                                                  g_path_get_basename(device_path));
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
    
    // 获取速度信息
    const char *speed = udev_device_get_sysattr_value(usb_dev, "speed");
    const char *product = udev_device_get_sysattr_value(usb_dev, "product");
    const char *manufacturer = udev_device_get_sysattr_value(usb_dev, "manufacturer");
    const char *serial = udev_device_get_sysattr_value(usb_dev, "serial");
    const char *idVendor = udev_device_get_sysattr_value(usb_dev, "idVendor");
    const char *idProduct = udev_device_get_sysattr_value(usb_dev, "idProduct");
    
    *info = parse_usb_speed(speed);
    
    g_print("\n═══════════════════════════════════════════════════════\n");
    g_print("📱 USB设备信息\n");
    g_print("═══════════════════════════════════════════════════════\n");
    g_print("设备路径:     %s\n", device_path);
    g_print("制造商:       %s\n", manufacturer ? manufacturer : "未知");
    g_print("产品名称:     %s\n", product ? product : "未知");
    g_print("序列号:       %s\n", serial ? serial : "未知");
    g_print("VID:PID:      %s:%s\n", idVendor ? idVendor : "????", 
                                      idProduct ? idProduct : "????");
    g_print("───────────────────────────────────────────────────────\n");
    
    result = TRUE;
    
cleanup:
    if (dev) udev_device_unref(dev);
    udev_unref(udev);
    return result;
}

// 测试实际读取速度
double measure_read_speed(const char *device_path) {
    int fd;
    char *buffer;
    size_t buffer_size = 64 * 1024 * 1024;  // 64MB
    ssize_t bytes_read;
    struct timeval start, end;
    double elapsed, speed;
    
    g_print("\n⏳ 正在测试实际读取速度（读取64MB数据）...\n");
    
    fd = open(device_path, O_RDONLY | O_DIRECT);
    if (fd < 0) {
        // 如果O_DIRECT失败，尝试不带标志打开
        fd = open(device_path, O_RDONLY);
        if (fd < 0) {
            g_printerr("❌ 无法打开设备进行读取测试\n");
            return -1;
        }
    }
    
    // 分配对齐的缓冲区
    if (posix_memalign((void **)&buffer, 4096, buffer_size) != 0) {
        g_printerr("❌ 内存分配失败\n");
        close(fd);
        return -1;
    }
    
    // 测量读取速度
    gettimeofday(&start, NULL);
    bytes_read = read(fd, buffer, buffer_size);
    gettimeofday(&end, NULL);
    
    close(fd);
    free(buffer);
    
    if (bytes_read <= 0) {
        g_printerr("❌ 读取失败\n");
        return -1;
    }
    
    elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    speed = (bytes_read / (1024.0 * 1024.0)) / elapsed;  // MB/s
    
    return speed;
}

// 从/sys/block获取设备容量
guint64 get_device_capacity(const char *device_path) {
    const char *dev_name = g_path_get_basename(device_path);
    gchar *size_file = g_strdup_printf("/sys/block/%s/size", dev_name);
    gchar *contents = NULL;
    guint64 sectors = 0;
    
    if (g_file_get_contents(size_file, &contents, NULL, NULL)) {
        sectors = g_ascii_strtoull(contents, NULL, 10);
        g_free(contents);
    }
    
    g_free(size_file);
    return sectors * 512;  // 每个扇区512字节
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        g_print("用法: %s <设备路径>\n", argv[0]);
        g_print("例如: %s /dev/sdb\n", argv[0]);
        g_print("\n提示: 使用 'lsblk' 命令查看可用设备\n");
        return 1;
    }
    
    const char *device_path = argv[1];
    UsbSpeedInfo info;
    
    // 检查是否有root权限（读取速度测试需要）
    if (geteuid() != 0) {
        g_print("⚠️  警告: 需要root权限才能进行实际速度测试\n");
        g_print("   建议使用: sudo %s %s\n\n", argv[0], device_path);
    }
    
    // 获取USB协议速度
    if (!get_usb_protocol_speed(device_path, &info)) {
        return 1;
    }
    
    // 显示协议速度信息
    g_print("⚡ USB协议速度（协商速度）\n");
    g_print("═══════════════════════════════════════════════════════\n");
    g_print("USB版本:      %s\n", info.usb_version);
    g_print("协商速度:     %d Mbps\n", info.protocol_speed_mbps);
    g_print("理论最大值:   %.2f MB/s\n", info.theoretical_speed_mbs);
    g_print("───────────────────────────────────────────────────────\n");
    g_print("💡 说明: 这是USB总线协商的速度，由USB口和U盘中较慢\n");
    g_print("        的一方决定。实际速度取决于U盘的硬件性能。\n");
    
    // 获取设备容量
    guint64 capacity = get_device_capacity(device_path);
    if (capacity > 0) {
        g_print("\n💾 设备容量: %.2f GB\n", capacity / (1024.0 * 1024.0 * 1024.0));
    }
    
    // 测试实际读取速度
    if (geteuid() == 0) {
        double actual_speed = measure_read_speed(device_path);
        
        if (actual_speed > 0) {
            g_print("\n🎯 实际测试速度\n");
            g_print("═══════════════════════════════════════════════════════\n");
            g_print("实际读取速度: %.2f MB/s\n", actual_speed);
            g_print("───────────────────────────────────────────────────────\n");
            
            // 分析性能
            double efficiency = (actual_speed / info.theoretical_speed_mbs) * 100;
            g_print("\n📊 性能分析\n");
            g_print("═══════════════════════════════════════════════════════\n");
            g_print("协议理论速度: %.2f MB/s\n", info.theoretical_speed_mbs);
            g_print("实际测试速度: %.2f MB/s\n", actual_speed);
            g_print("协议利用率:   %.1f%%\n", efficiency);
            g_print("───────────────────────────────────────────────────────\n");
            
            if (efficiency < 30) {
                g_print("💭 评价: 这是一个低性能U盘，建议更换\n");
            } else if (efficiency < 60) {
                g_print("💭 评价: 中等性能U盘，适合一般使用\n");
            } else {
                g_print("💭 评价: 高性能U盘，速度表现优秀\n");
            }
            
            // 显示不同U盘的差异说明
            g_print("\n📌 重要说明\n");
            g_print("═══════════════════════════════════════════════════════\n");
            g_print("即使多个U盘都显示相同的协议速度（如USB 3.0），\n");
            g_print("实际读写速度也可能相差3-10倍！\n\n");
            g_print("影响因素包括：\n");
            g_print("  • 闪存芯片质量（SLC > MLC > TLC > QLC）\n");
            g_print("  • 控制器性能（便宜的vs高端的）\n");
            g_print("  • 固件优化程度\n");
            g_print("  • 是否有DRAM缓存\n");
            g_print("═══════════════════════════════════════════════════════\n");
        }
    }
    
    return 0;
}

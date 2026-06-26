#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/console/console.h>
#else
#include <iostream>
#include <thread>
#include <chrono>
#include <stdlib.h>

struct device {
    const char *name;
};

struct sensor_value {
    int val1;
    int val2;
};

#define SENSOR_CHAN_AMBIENT_TEMP 0

inline void console_getline_init() {}
inline char* console_getline() {
    static std::string input_buf;
    if (std::getline(std::cin, input_buf)) {
        return &input_buf[0];
    }
    // Exit simulator gracefully on stdin EOF (e.g. piped input completes)
    exit(0);
}

#define printk printf
#define k_msleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#define sys_reboot(type) do { printf("\n[MOCK] System Rebooting...\n"); exit(0); } while(0)
#define SYS_REBOOT_COLD 0
#define device_is_ready(dev) (false)

static struct device mock_devices[] = {
    {"CPU_0"},
    {"UART_CONSOLE"},
    {"VIRTUAL_DISPLAY"},
    {"KEYBOARD"},
    {"TEMPERATURE_SENSOR"}
};

inline size_t z_device_get_all_static(const struct device **devices) {
    *devices = mock_devices;
    return sizeof(mock_devices) / sizeof(struct device);
}

inline int sensor_sample_fetch(const struct device *dev) { return 0; }
inline int sensor_channel_get(const struct device *dev, int chan, struct sensor_value *val) {
    static int mock_temp = 20;
    mock_temp += 2;
    if (mock_temp > 50) mock_temp = 20;
    val->val1 = mock_temp;
    val->val2 = 0;
    return 0;
}
#endif

// ANSI Escape Codes for CLI Colors
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"

extern const struct device *sensor_dev;

#endif // PLATFORM_H

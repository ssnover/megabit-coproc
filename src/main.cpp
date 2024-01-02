#include "usb.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(megabit, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
constexpr uint32_t SLEEP_TIME_MS(1000);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main() {
    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    if (int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE); ret < 0) {
        return 0;
    }

    if (int ret = init_usb_stack(); ret != 0) {
        LOG_ERR("Failed to initialize USB stack: %d", ret);
        return 0;
    }

    bool led_state = true;
    while (true) {
        gpio_pin_toggle_dt(&led);
        led_state = !led_state;
        LOG_INF("LED state: %s", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }

	return 0;
}

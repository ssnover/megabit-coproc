#include "led.hpp"
#include "usb.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(megabit, LOG_LEVEL_INF);

int main() {
    if (int ret = led::init_leds(); ret != 0) {
        LOG_ERR("Failed to init LEDs: %d", ret);
        return 0;
    }
    if (int ret = init_usb_stack(); ret != 0) {
        LOG_ERR("Failed to init USB stack: %d", ret);
        led::execute_panic_loop();
    }

    bool led_state = true;
    while (true) {
        led_state = !led_state;
        led::set_state(led_state);
        LOG_INF("LED state: %s", led_state ? "ON" : "OFF");
        k_msleep(1000);
    }

	return 0;
}

#include "led.hpp"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

namespace led {
namespace {

const struct gpio_dt_spec SIMPLE_LED = GPIO_DT_SPEC_GET(DT_ALIAS(led4), gpios);

} // namespace

int init_leds() {
    if (!gpio_is_ready_dt(&SIMPLE_LED)) {
        return -ENODEV;
    }
    if (int ret = gpio_pin_configure_dt(&SIMPLE_LED, GPIO_OUTPUT_INACTIVE); ret < 0) {
        return ret;
    }

    return 0;
}

int toggle() {
    return gpio_pin_toggle_dt(&SIMPLE_LED);
}

int set_state(bool new_state) {
    return gpio_pin_configure_dt(&SIMPLE_LED, new_state ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
}

int set_color(Color color) {
    return 0;
}

void execute_panic_loop() {
    constexpr uint32_t PANIC_SHORT_HALF_PERIOD_MS(100);
    constexpr uint32_t PANIC_LONG_HALF_PERIOD_MS(700);
    while (true) {
        for (auto i = 0u; i < 3; ++i) {
            set_state(true);
            k_msleep(PANIC_LONG_HALF_PERIOD_MS);
            set_state(false);
            k_msleep(PANIC_LONG_HALF_PERIOD_MS);
        }
        for (auto i = 0u; i < 3; ++i) {
            set_state(true);
            k_msleep(PANIC_SHORT_HALF_PERIOD_MS);
            set_state(false);
            k_msleep(PANIC_SHORT_HALF_PERIOD_MS);
        }
    }
}


} // namespace led
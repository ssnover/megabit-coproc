#include <cstdio>
#include <cstdint>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

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

    bool led_state = true;
    while (true) {
        auto toggle_result = gpio_pin_toggle_dt(&led);
        if (toggle_result < 0) {
            return 0;
        }

        led_state = !led_state;
        printf("LED state: %s\n", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
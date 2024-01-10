#include "led.hpp"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

namespace led {
namespace {

// DL2 green LED
const struct gpio_dt_spec SIMPLE_LED = GPIO_DT_SPEC_GET(DT_ALIAS(led4), gpios);
// DL3 RGB LED
const struct pwm_dt_spec RED_PWM_LED = PWM_DT_SPEC_GET(DT_ALIAS(red_pwm_led));
const struct pwm_dt_spec GREEN_PWM_LED = PWM_DT_SPEC_GET(DT_ALIAS(green_pwm_led));
const struct pwm_dt_spec BLUE_PWM_LED = PWM_DT_SPEC_GET(DT_ALIAS(blue_pwm_led));
constexpr u32 PWM_STEP_SIZE_US = PWM_USEC(2000);
constexpr float MAX_BRIGHTNESS_DIM_RATIO(0.75);

} // namespace

int init_leds() {
    if (!gpio_is_ready_dt(&SIMPLE_LED) || !pwm_is_ready_dt(&RED_PWM_LED) || !pwm_is_ready_dt(&GREEN_PWM_LED) || !pwm_is_ready_dt(&BLUE_PWM_LED)) {
        return -ENODEV;
    }
    if (int ret = gpio_pin_configure_dt(&SIMPLE_LED, GPIO_OUTPUT_INACTIVE); ret < 0) {
        return ret;
    }
    if (int ret = pwm_set_pulse_dt(&RED_PWM_LED, 0); ret != 0) {
        return ret;
    }
    if (int ret = pwm_set_pulse_dt(&GREEN_PWM_LED, 0); ret != 0) {
        return ret;
    }
    if (int ret = pwm_set_pulse_dt(&BLUE_PWM_LED, 0); ret != 0) {
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
    int retVal = 0;
    switch (color) {
        case Color::Red:
            retVal |= pwm_set_pulse_dt(&RED_PWM_LED, RED_PWM_LED.period * MAX_BRIGHTNESS_DIM_RATIO);
            retVal |= pwm_set_pulse_dt(&GREEN_PWM_LED, 0);
            retVal |= pwm_set_pulse_dt(&BLUE_PWM_LED, 0);
            break;
        case Color::Green:
            retVal |= pwm_set_pulse_dt(&GREEN_PWM_LED, GREEN_PWM_LED.period * MAX_BRIGHTNESS_DIM_RATIO);
            retVal |= pwm_set_pulse_dt(&RED_PWM_LED, 0);
            retVal |= pwm_set_pulse_dt(&BLUE_PWM_LED, 0);
            break;
        case Color::Blue:
            retVal |= pwm_set_pulse_dt(&BLUE_PWM_LED, BLUE_PWM_LED.period * MAX_BRIGHTNESS_DIM_RATIO);
            retVal |= pwm_set_pulse_dt(&RED_PWM_LED, 0);
            retVal |= pwm_set_pulse_dt(&GREEN_PWM_LED, 0);
            break;
        case Color::None:
        default:
            retVal |= pwm_set_pulse_dt(&RED_PWM_LED, 0);
            retVal |= pwm_set_pulse_dt(&GREEN_PWM_LED, 0);
            retVal |= pwm_set_pulse_dt(&BLUE_PWM_LED, 0);
            break;
    }
    return retVal;
}

void execute_panic_loop() {
    constexpr u32 PANIC_SHORT_HALF_PERIOD_MS(100);
    constexpr u32 PANIC_LONG_HALF_PERIOD_MS(700);
    while (true) {
        for (auto i = 0u; i < 3; ++i) {
            set_state(true);
            set_color(Color::Red);
            k_msleep(PANIC_LONG_HALF_PERIOD_MS);
            set_state(false);
            set_color(Color::None);
            k_msleep(PANIC_LONG_HALF_PERIOD_MS);
        }
        for (auto i = 0u; i < 3; ++i) {
            set_state(true);
            set_color(Color::Red);
            k_msleep(PANIC_SHORT_HALF_PERIOD_MS);
            set_state(false);
            set_color(Color::None);
            k_msleep(PANIC_SHORT_HALF_PERIOD_MS);
        }
    }
}


} // namespace led
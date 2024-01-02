#include "usb.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(megabit, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
constexpr uint32_t SLEEP_TIME_MS(1000);

constexpr uint32_t RING_BUFFER_SIZE(1024);
std::array<uint8_t, RING_BUFFER_SIZE> ring_buffer_backend;
struct ring_buf ring_buffer;

static bool data_received;
static bool rx_throttled;

UsbContext * usb_serial;

int enable_usb_device_next() {
    usb_serial = init_usb_device();
    if (usb_serial == nullptr) {
        LOG_ERR("Failed to initialize USB device");
        return -ENODEV;
    }

    if (int err = usbd_enable(usb_serial); err) {
        LOG_ERR("Failed to enable device support: %d", err);
        return err;
    }

    LOG_DBG("USB device support enabled");

    return 0;
}

void usb_interrupt(const struct device * dev, void * /* user_data */) {
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (!rx_throttled && uart_irq_rx_ready(dev)) {
            std::array<uint8_t, 64> buffer;
            size_t len = std::min(ring_buf_space_get(&ring_buffer), buffer.size());
            if (len == 0) {
                // TODO: Write my own ring buffer which allows overwriting
                LOG_ERR("Ring buffer is full, dropping bytes");
                uart_irq_rx_disable(dev);
                rx_throttled = true;
                continue;
            }

            auto rx_bytes = uart_fifo_read(dev, buffer.data(), len);
            if (rx_bytes < 0) {
                LOG_ERR("Failed to read UART FIFO: %d", rx_bytes);
                rx_bytes = 0;
            }

            auto ring_buf_tx_bytes = ring_buf_put(&ring_buffer, buffer.data(), rx_bytes);
            if ((int64_t)ring_buf_tx_bytes < rx_bytes) {
                LOG_ERR("Drop %u bytes", ring_buf_tx_bytes - rx_bytes);
            }

            LOG_DBG("tty fifo -> ringbuf %d bytes", ring_buf_tx_bytes);
            if (ring_buf_tx_bytes) {
                data_received = true;
                uart_irq_tx_enable(dev);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            std::array<uint8_t, 64> buffer;
            uint32_t ring_buf_rx_bytes = ring_buf_get(&ring_buffer, buffer.data(), buffer.size());
            if (ring_buf_rx_bytes == 0) {
                LOG_DBG("Ring buffer empty, disable TX IRQ");
                uart_irq_tx_disable(dev);
                continue;
            }

            if (rx_throttled) {
                uart_irq_rx_enable(dev);
                rx_throttled = false;
            }

            int32_t tx_bytes = uart_fifo_fill(dev, buffer.data(), ring_buf_rx_bytes);
            if (tx_bytes < (int64_t)ring_buf_rx_bytes) {
                LOG_ERR("Drop %d bytes", ring_buf_rx_bytes - tx_bytes);
            }

            LOG_DBG("ringbuf -> tty fifo %d bytes", tx_bytes);
        }
    }
}

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void flash_led_forever() {
    bool led_state = true;
    while (true) {
        gpio_pin_toggle_dt(&led);
        led_state = !led_state;
        LOG_INF("LED state: %s", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }
}

uint32_t dtr = 0;

int main() {
    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    if (int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE); ret < 0) {
        return 0;
    }

    const struct device * dev = DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0));
    if (!device_is_ready(dev)) {
        LOG_ERR("CDC ACM device not ready");
        return 0;
    }

    if (int ret = enable_usb_device_next(); ret != 0) {
        LOG_ERR("Failed to enable USB");
        return 0;
    }

    ring_buf_init(&ring_buffer, ring_buffer_backend.size(), ring_buffer_backend.data());

    while (true) {
        gpio_pin_toggle_dt(&led);
        uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
        if (dtr) {
            break;
        } else {
            k_msleep(100);
        }
    }

    flash_led_forever();

    uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
    uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);

    LOG_INF("DTR set");
    // Wait 100ms for the host to do all settings
    k_msleep(100);

    // uint32_t baudrate = 0;
    // if (int err = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate); err) {
    //     LOG_WRN("Failed to get baudrate: %d", err);
    // } else {
    //     LOG_INF("Baudrate detected: %d", baudrate);
    // }

    uart_irq_callback_set(dev, usb_interrupt);
    uart_irq_rx_enable(dev);

    flash_led_forever();

    return 0;
}
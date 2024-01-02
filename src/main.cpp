/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample echo app for CDC ACM class
 *
 * Sample app for USB CDC ACM class driver. The received data is echoed back
 * to the serial port.
 */

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

constexpr uint32_t RING_BUFFER_SIZE(1024);
std::array<uint8_t, RING_BUFFER_SIZE> ring_buffer;
struct ring_buf ringbuf;

static bool rx_throttled;

static UsbContext *usb_serial;

static int enable_usb_device_next()
{
	usb_serial = init_usb_device();
	if (usb_serial == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (int err = usbd_enable(usb_serial); err) {
		LOG_ERR("Failed to enable device support");
		return err;
	}

	LOG_DBG("USB device support enabled");

	return 0;
}

static void interrupt_handler(const struct device *dev, void * /* user_data */)
{
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!rx_throttled && uart_irq_rx_ready(dev)) {
			uint8_t buffer[64];
			size_t len = std::min(ring_buf_space_get(&ringbuf),
					 sizeof(buffer));

			if (len == 0) {
				/* Throttle because ring buffer is full */
				uart_irq_rx_disable(dev);
				rx_throttled = true;
				continue;
			}

			int bytes_received = uart_fifo_read(dev, buffer, len);
			if (bytes_received < 0) {
				LOG_ERR("Failed to read UART FIFO");
				bytes_received = 0;
			};

			int bytes_written = ring_buf_put(&ringbuf, buffer, bytes_received);
			if (bytes_written < bytes_received) {
				LOG_ERR("Drop %u bytes", bytes_received - bytes_written);
			}

			LOG_DBG("tty fifo -> ringbuf %d bytes", bytes_written);
			if (bytes_written) {
				uart_irq_tx_enable(dev);
			}
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];

			int bytes_read = ring_buf_get(&ringbuf, buffer, sizeof(buffer));
			if (!bytes_read) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			if (rx_throttled) {
				uart_irq_rx_enable(dev);
				rx_throttled = false;
			}

			int bytes_tx = uart_fifo_fill(dev, buffer, bytes_read);
			if (bytes_tx < bytes_read) {
				LOG_ERR("Drop %d bytes", bytes_read - bytes_tx);
			}

			LOG_DBG("ringbuf -> tty fifo %d bytes", bytes_tx);
		}
	}
}

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    if (int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE); ret < 0) {
        return 0;
    }

	uint32_t baudrate, dtr = 0U;

	const struct device *dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	if (!device_is_ready(dev)) {
		LOG_ERR("CDC ACM device not ready");
		return 0;
	}

	if (int ret = enable_usb_device_next(); ret != 0) {
		LOG_ERR("Failed to enable USB");
		return 0;
	}

	ring_buf_init(&ringbuf, ring_buffer.size(), ring_buffer.data());

	LOG_INF("Wait for DTR");

	while (true) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		} else {
			/* Give CPU resources to low priority threads. */
			k_sleep(K_MSEC(100));
		}
	}

	LOG_INF("DTR set");

	if (int ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1); ret) {
		LOG_WRN("Failed to set DCD, ret code %d", ret);
	}
	if (int ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1); ret) {
		LOG_WRN("Failed to set DSR, ret code %d", ret);
	}

	/* Wait 100ms for the host to do all settings */
	k_msleep(100);

	if (int ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate); ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, interrupt_handler);
	uart_irq_rx_enable(dev);

    bool led_state = true;
    while (true) {
        gpio_pin_toggle_dt(&led);
        led_state = !led_state;
        LOG_INF("LED state: %s", led_state ? "ON" : "OFF");
        k_msleep(SLEEP_TIME_MS);
    }

	return 0;
}

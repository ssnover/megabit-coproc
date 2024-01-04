#include "usb.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usb);
namespace {
constexpr uint16_t COPROC_USB_VID(0x16c0);
constexpr uint16_t COPROC_USB_PID(0x27de);
} // anonymous

USBD_DEVICE_DEFINE(usb_serial_dev,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   COPROC_USB_VID, COPROC_USB_PID);
USBD_DESC_LANG_DEFINE(lang_desc);
USBD_DESC_MANUFACTURER_DEFINE(mnfr_desc, "Snostorm Labs");
USBD_DESC_PRODUCT_DEFINE(product_desc, "Megabit coproc");
USBD_DESC_SERIAL_NUMBER_DEFINE(sn_desc, "0123456789ABCDEF");

using UsbContext = struct usbd_contex;

namespace {
constexpr uint8_t attributes = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);
} // anonymous
USBD_CONFIGURATION_DEFINE(usb_cfg, attributes, 125);

namespace {

constexpr uint32_t RING_BUFFER_SIZE(1024);
std::array<uint8_t, RING_BUFFER_SIZE> ring_buffer;
struct ring_buf ringbuf;
bool rx_throttled;

} // anonymous

static UsbContext * init_usb_device() {
	if (int err = usbd_add_descriptor(&usb_serial_dev, &lang_desc); err) {
		LOG_ERR("Failed to init_leds language descriptor (%d)", err);
		return nullptr;
	}

	if (int err = usbd_add_descriptor(&usb_serial_dev, &mnfr_desc); err) {
		LOG_ERR("Failed to init_leds manufacturer descriptor (%d)", err);
		return nullptr;
	}

	if (int err = usbd_add_descriptor(&usb_serial_dev, &product_desc); err) {
		LOG_ERR("Failed to init_leds product descriptor (%d)", err);
		return nullptr;
	}

	if (int err = usbd_add_descriptor(&usb_serial_dev, &sn_desc); err) {
		LOG_ERR("Failed to init_leds SN descriptor (%d)", err);
		return nullptr;
	}
	
	if (int err = usbd_add_configuration(&usb_serial_dev, &usb_cfg); err) {
		LOG_ERR("Failed to add configuration (%d)", err);
		return nullptr;
	}

    // This macro causes link-time failures for undefined references it generates to
    // _usbd_class_node_list_start and _usbd_class_node_list_end
	STRUCT_SECTION_FOREACH(usbd_class_node, node) {
		// Pull everything that is enabled in our configuration.
		if (int err = usbd_register_class(&usb_serial_dev, node->name, 1); err) {
			LOG_ERR("Failed to register %s (%d)", node->name, err);
			return nullptr;
		}

		LOG_DBG("Register %s", node->name);
	}

    /*
     * Class with multiple interfaces have an Interface
     * Association Descriptor available, use an appropriate triple
     * to indicate it.
     */
    usbd_device_set_code_triple(&usb_serial_dev, USB_BCC_MISCELLANEOUS, 0x02, 0x01);

	if (int err = usbd_init(&usb_serial_dev); err) {
		LOG_ERR("Failed to init_leds device support");
		return nullptr;
	}

	return &usb_serial_dev;
}

namespace {

UsbContext *usb_serial;

int enable_usb_device_next() {
    usb_serial = init_usb_device();
    if (usb_serial == nullptr) {
	    LOG_ERR("Failed to init_leds USB device");
	    return -ENODEV;
    }

    if (int err = usbd_enable(usb_serial); err) {
	    LOG_ERR("Failed to enable device support");
	    return err;
    }

    LOG_DBG("USB device support enabled");
    return 0;
}

void usb_interrupt(const struct device * dev, void * /* user_data */) {
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (!rx_throttled && uart_irq_rx_ready(dev)) {
			uint8_t buffer[64];
			size_t len = std::min(ring_buf_space_get(&ringbuf), sizeof(buffer));

			if (len == 0) {
				/* Throttle because ring buffer is full */
                // TODO Send message when this occurs
				uart_irq_rx_disable(dev);
				rx_throttled = true;
				continue;
			}

			int bytes_rx = uart_fifo_read(dev, buffer, len);
			if (bytes_rx < 0) {
				LOG_ERR("Failed to read UART FIFO");
				bytes_rx = 0;
			};

			int bytes_written = ring_buf_put(&ringbuf, buffer, bytes_rx);
			if (bytes_written < bytes_rx) {
				LOG_ERR("Drop %u bytes", bytes_rx - bytes_written);
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

} // anonymous

int init_usb_stack() {
    const struct device *dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	if (!device_is_ready(dev)) {
		LOG_ERR("CDC ACM device not ready");
		return -ENODEV;
	}

	if (int ret = enable_usb_device_next(); ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	ring_buf_init(&ringbuf, ring_buffer.size(), ring_buffer.data());

	LOG_INF("Wait for DTR");

	while (true) {
        uint32_t dtr = 0;
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

    uint32_t baudrate = 0;
	if (int ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate); ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, usb_interrupt);
	uart_irq_rx_enable(dev);

    return 0;
}
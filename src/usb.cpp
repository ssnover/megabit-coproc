#include "ring-buffer.hpp"
#include "usb.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usb);

using UsbContext = struct usbd_contex;

namespace {

constexpr uint16_t COPROC_USB_VID(0x16c0);
constexpr uint16_t COPROC_USB_PID(0x27de);
constexpr uint8_t ATTRIBUTES = (IS_ENABLED(CONFIG_SAMPLE_USBD_SELF_POWERED) ?
				   USB_SCD_SELF_POWERED : 0) |
				  (IS_ENABLED(CONFIG_SAMPLE_USBD_REMOTE_WAKEUP) ?
				   USB_SCD_REMOTE_WAKEUP : 0);

RingBuffer<1024> usb_rx_buffer;
UsbContext *usb_serial;
struct k_spinlock usb_event_lock;
K_EVENT_DEFINE(usb_events);

} // anonymous

USBD_DEVICE_DEFINE(usb_serial_dev,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   COPROC_USB_VID, COPROC_USB_PID);
USBD_DESC_LANG_DEFINE(lang_desc);
USBD_DESC_MANUFACTURER_DEFINE(mnfr_desc, "Snostorm Labs");
USBD_DESC_PRODUCT_DEFINE(product_desc, "Megabit coproc");
USBD_DESC_SERIAL_NUMBER_DEFINE(sn_desc, "0123456789ABCDEF");
USBD_CONFIGURATION_DEFINE(usb_cfg, ATTRIBUTES, 125);

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
		if (uart_irq_rx_ready(dev)) {
			// Post a receive event
			k_event_post(&usb_events, 1u << 0);
		}

		if (uart_irq_tx_ready(dev)) {
			// Post a different event
			k_event_post(&usb_events, 1u << 1);
		}
	}
}

} // anonymous

int init_usb_stack(const struct device * dev) {
	if (!device_is_ready(dev)) {
		LOG_ERR("CDC ACM device not ready");
		return -ENODEV;
	}

	if (int ret = enable_usb_device_next(); ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	LOG_INF("Wait for DTR");

	while (true) {
        u32 dtr = 0;
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

    u32 baudrate = 0;
	if (int ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate); ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, usb_interrupt);
	uart_irq_rx_enable(dev);
	uart_irq_tx_enable(dev);

    return 0;
}

int usb_task_main() {
	const struct device *dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	if (init_usb_stack(dev) != 0) {
		LOG_ERR("Failed to init USB stack");
		while (true) {
			k_msleep(100);
		}
	}

	while (true) {
		auto events = k_event_wait(&usb_events, 0xFFFF, false, K_MSEC(50));
		if (events & (1u << 0)) {
			// Received bytes event
			K_SPINLOCK(&usb_event_lock) {
				std::array<u8, 64> buffer;
				usize len = std::min(usb_rx_buffer.space_in_buffer(), buffer.size());

				if (len == 0) {
					/* Throttle because ring buffer is full */
					// TODO Send message when this occurs
					continue;
				}

				int bytes_rx = uart_fifo_read(dev, buffer.data(), len);
				if (bytes_rx < 0) {
					LOG_ERR("Failed to read UART FIFO");
					bytes_rx = 0;
				};

				int bytes_written = usb_rx_buffer.write(buffer.data(), bytes_rx);
				if (bytes_written < bytes_rx) {
					LOG_ERR("Drop %u bytes", bytes_rx - bytes_written);
				}
			}
			// Now read data from the buffer to decode
		}
		if (events & (1u << 1)) {
			// Transmitted bytes event
		}
	}
}

K_THREAD_DEFINE(usb_thread_id, 1024, usb_task_main, nullptr, nullptr, nullptr, 5, 0, 0);
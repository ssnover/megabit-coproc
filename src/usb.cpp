#include "usb.hpp"
#include "zephyr-usbd-wrapper.h"
#include <cstdint>
#include <zephyr/device.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usb);

constexpr uint16_t COPROC_USB_PID(0x27de);
constexpr uint16_t COPROC_USB_VID(0x16c0);

USBD_DEVICE_DEFINE(
    usb_serial,
    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
    COPROC_USB_VID, 
    COPROC_USB_PID
);
USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "Snostorm Labs");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Megabit Coproc");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "1234567890");
USBD_CONFIGURATION_DEFINE(sample_config, 0, 125);

UsbContext * init_usb_device() {
    if (int err = usbd_add_descriptor(&usb_serial, &sample_lang); err) {
        LOG_ERR("Failed to initialize lang descriptor: %d", err);
        return nullptr;
    }

    if (int err = usbd_add_descriptor(&usb_serial, &sample_mfr); err) {
        LOG_ERR("Failed to initalize manufacturer descriptor: %d", err);
        return nullptr;
    }

    if (int err = usbd_add_descriptor(&usb_serial, &sample_product); err) {
        LOG_ERR("Failed to initialize product descriptor: %d", err);
        return nullptr;
    }

    if (int err = usbd_add_descriptor(&usb_serial, &sample_sn); err) {
        LOG_ERR("Failed to initialize SN descriptor: %d", err);
        return nullptr;
    }

    if (int err = usbd_add_configuration(&usb_serial, &sample_config); err) {
        LOG_ERR("Failed to add configuration: %d", err);
        return nullptr;
    }

    STRUCT_SECTION_FOREACH(usbd_class_node, node) {
        if (int err = usbd_register_class(&usb_serial, node->name, 1); err) {
            LOG_ERR("Failed to register %s: %d", node->name, err);
            return nullptr;
        }

        LOG_DBG("Register %s", node->name);
    }

    usbd_device_set_code_triple(&usb_serial, USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    if (int err = usbd_init(&usb_serial); err) {
        LOG_ERR("Failed to initialize device support");
        return nullptr;
    }

    return &usb_serial;
}

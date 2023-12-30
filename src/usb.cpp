#include "usb.hpp"
#include <cstdint>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usb);

namespace {

constexpr uint16_t ZEPHYR_PROJECT_USB_VID(0x2fe3);

USBD_DEVICE_DEFINE(usb_serial,
    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
    ZEPHYR_PROJECT_USB_VID, CONFIG_USB_SERIAL_PID);
USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, CONFIG_USB_SERIAL_MANUFACTURER);
USBD_PRODUCT_DEFINE(sample_product, CONFIG_USB_SERIAL_PRODUCT);
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");

} // anonymous namespace

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

    STRUCT_SECTION_FOREACH(usbd_class_node, node) {
        if (int err = usbd_register_class(&usb_serial, node->name, 1); err) {
            LOG_ERR("Failed to register %s: %d", node->name, err);
            return nullptr;
        }

        LOG_DBG("Register %s", node->name);
    }

    if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) || IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS)) {
        usbd_device_set_code_triple(&usb_serial, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
    } else {
        usbd_device_set_code_triple(&usb_serial, 0, 0, 0);
    }

    if (int err = usbd_init(&usb_serial); err) {
        LOG_ERR("Failed to initialize device support");
        return nullptr;
    }

    return &usb_serial;
}
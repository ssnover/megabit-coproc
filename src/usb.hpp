#pragma once

#include <cstdint>
#include <zephyr/usb/usbd.h>

using UsbContext = struct usbd_contex;

UsbContext * init_usb_device();
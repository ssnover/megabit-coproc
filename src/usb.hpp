#pragma once

#include <cstdint>
#include "zephyr-usbd-wrapper.h"

using UsbContext = struct usbd_contex;

UsbContext * init_usb_device();
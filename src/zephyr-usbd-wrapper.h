#pragma once

// This header uses "class" as an identifier in a function signature...

#define class class_
#include <zephyr/usb/usbd.h>
#undef class
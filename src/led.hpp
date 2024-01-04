#pragma once

#include <cstdint>

namespace led {

enum Color : uint8_t {
    None = 0u,
    Red = 1u,
    Green = 2u,
    Blue = 3u,
};

int init_leds();

int toggle();

int set_state(bool new_state);

int set_color(Color color);

void execute_panic_loop();

} // namespace led
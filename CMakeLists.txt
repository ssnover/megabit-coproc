cmake_minimum_required(VERSION 3.20.0)

set(BOARD arduino_nano_33_ble)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(megabit-coproc)

target_sources(app
  PRIVATE
    src/led.cpp
    src/main.cpp
    src/usb.cpp
)
target_include_directories(app
  PRIVATE
    src
)
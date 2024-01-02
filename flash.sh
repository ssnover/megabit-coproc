#!/bin/bash

BOSSAC_PATH="$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2"

$BOSSAC_PATH/bossac -d -p /dev/ttyACM0 -U -i -e -w -R build/zephyr/zephyr.bin

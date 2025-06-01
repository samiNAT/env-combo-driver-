#!/bin/sh

# Constants
I2C_ADAPTER=0
I2C_ADDR=0x39
TEMP_RAW_FILE="/sys/bus/iio/devices/iio:device0/in_temp0_raw"
HUM_RAW_FILE="/sys/bus/iio/devices/iio:device0/in_humidityrelative1_raw"

echo "-------------------------------------------------------"
echo "[INFO] Removing any previously loaded modules..."
rmmod env_combo 2>/dev/null
rmmod i2c-envcombo-sim 2>/dev/null

echo "-------------------------------------------------------"
echo "[INFO] Loading I2C simulation module..."
insmod /tmp/i2c-envcombo-sim.ko

echo "-------------------------------------------------------"
echo "[INFO] Creating new I2C device..."
echo "env-combo $I2C_ADDR" > /sys/bus/i2c/devices/i2c-$I2C_ADAPTER/new_device

echo "-------------------------------------------------------"
echo "[INFO] Loading environment combo driver..."
insmod /tmp/env-combo.ko

echo "-------------------------------------------------------"
echo "[INFO] Listing IIO devices..."
ls /sys/bus/iio/devices/

echo "-------------------------------------------------------"
echo "[INFO] Reading one sample of temperature and humidity..."

RAW_TEMP=$(cat "$TEMP_RAW_FILE" 2>/dev/null)
RAW_HUM=$(cat "$HUM_RAW_FILE" 2>/dev/null)

TEMP_C=$(awk -v raw="$RAW_TEMP" 'BEGIN { printf "%.2f", raw / 100 }')
HUM_PCT=$(awk -v raw="$RAW_HUM" 'BEGIN { printf "%.1f", raw / 2 }')

echo "Temperature: raw=$RAW_TEMP, converted=$TEMP_C °C"
echo "Humidity:    raw=$RAW_HUM, converted=$HUM_PCT %RH"

echo "-------------------------------------------------------"
echo "[INFO] Checking dmesg for logs..."
dmesg | grep env-combo | tail -10

echo "-------------------------------------------------------"
echo "[INFO] Cleaning up..."
rmmod env_combo
rmmod i2c-envcombo-sim
echo "-------------------------------------------------------"


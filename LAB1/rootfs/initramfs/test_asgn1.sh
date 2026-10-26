#!/bin/sh

set -e

DEVICE="/dev/asgn1"
MODULE="asgn1_driver"

echo "===== ASGN1 TEST START ====="

if ! grep -q "^${MODULE} " /proc/modules; then
    insmod /lib/modules/5.15.0/asgn1_driver.ko
fi

MAJOR=$(awk '$2 == "asgn1" {print $1}' /proc/devices)
if [ -z "$MAJOR" ]; then
    echo "FAIL: Cannot find major number"
    exit 1
fi

if [ ! -e "$DEVICE" ]; then
    mknod "$DEVICE" c "$MAJOR" 0
fi
chmod 666 "$DEVICE"

echo "[1] Test write/read"
printf 'Hello ASGN1' > "$DEVICE"
RESULT=$(cat "$DEVICE")
echo "Read result: $RESULT"

echo "[2] Test ioctl commands"
if [ -x /test_asgn1 ]; then
    /test_asgn1
else
    echo "FAIL: /test_asgn1 not found"
    exit 1
fi

echo "[3] Kernel log"
dmesg | grep 'asgn1:' | tail -n 20 || true

echo "[4] Unload driver"
rmmod "$MODULE"
echo "Driver unloaded successfully"

echo "===== ASGN1 TEST FINISHED ====="

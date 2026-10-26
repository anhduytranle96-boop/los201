BACKUP LAB

TASK 1 - ASGN1
- Character driver: asgn1_driver
- Device: /dev/asgn1
- Ioctl: RESET_BUFFER, GET_STATS, SET_MODE, GET_VERSION
- Tested on QEMU ARM Linux 5.15.0
- Read/write, all four ioctl commands, and module unload tested successfully.

TASK 2 - COUNTER DRIVER
- Driver: counter_driver
- Device: /dev/counter
- Uses miscdevice, major 10, dynamic minor, and atomic counter.
- Tested on QEMU ARM Linux 5.15.0
- Read output: 0, 1, 2, 3
- Module unload tested successfully.

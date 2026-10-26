CC ?= gcc
ARM_CC ?= arm-linux-gnueabihf-gcc
CFLAGS = -Wall -Wextra -O2 -g
LDLIBS = -pthread -lrt

all: sms_app

sms_app: sms_app.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

arm: sms_app.c
	$(ARM_CC) $(CFLAGS) -o sms_app_arm $< $(LDLIBS)

clean:
	rm -f sms_app sms_app_arm

.PHONY: all arm clean

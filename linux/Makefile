all: libwooting-rgb-control.so

CC ?= gcc
CFLAGS ?= -Wall -g -fPIC

LDFLAGS ?= -Wall -g

OBJS = ../hidapi/linux/hid.o ../src/wooting-rgb-control.o ../src/wooting-usb.o
LIBS = `pkg-config libusb-1.0 --libs`
INCLUDES ?= -I../hidapi/hidapi -I../src `pkg-config libusb-1.0 --cflags`

libwooting-rgb-control.so: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-soname,$0.0 $^ -o $@

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)

.PHONY: clean libs

all: libwooting-rgb-sdk.so

CC ?= gcc
CFLAGS ?= -Wall -g -fPIC

LDFLAGS ?= -Wall -g

OBJS = ../src/wooting-rgb-sdk.o ../src/wooting-usb.o
LIBS = `pkg-config libusb-1.0 --libs` `pkg-config hidapi-libusb --libs`
INCLUDES ?= `pkg-config hidapi-libusb --cflags` -I../src `pkg-config libusb-1.0 --cflags`

libwooting-rgb-sdk.so: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-soname,$0.0 $^ -o $@

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)

install: libwooting-rgb-sdk.so
	cp libwooting-rgb-sdk.so /usr/lib/libwooting-rgb-sdk.so
	cp libwooting-rgb-sdk.pc /usr/lib/pkgconfig/libwooting-rgb-sdk.pc

.PHONY: clean libs

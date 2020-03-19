all: libwooting-rgb-sdk.dylib

CC ?= gcc
CFLAGS ?= -Wall -g -fPIC

LDFLAGS ?= -Wall -g

OBJS = ../src/wooting-rgb-sdk.o ../src/wooting-usb.o
LIBS = `pkg-config libusb-1.0 --libs` `pkg-config hidapi --libs`
INCLUDES ?= `pkg-config hidapi --cflags` -I../src `pkg-config libusb-1.0 --cflags`

libwooting-rgb-sdk.dylib: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-install_name,$0.0 $^ -o $@

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)

install: libwooting-rgb-sdk.dylib
	cp libwooting-rgb-sdk.dylib /usr/local/libwooting-rgb-sdk.dylib
	cp libwooting-rgb-sdk.pc /usr/local/pkgconfig/libwooting-rgb-sdk.pc

.PHONY: clean libs

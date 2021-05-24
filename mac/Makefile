all: libwooting-rgb-sdk.dylib

prefix ?= /usr/local

CC ?= gcc
CFLAGS ?= -Wall -g -fPIC
CPPFLAGS ?= #-DDEBUG_LOG
LDFLAGS ?= -Wall -g

OBJS = ../src/wooting-rgb-sdk.o ../src/wooting-usb.o
LIBS = `pkg-config libusb-1.0 --libs` `pkg-config hidapi --libs`
INCLUDES ?= `pkg-config hidapi --cflags` -I../src `pkg-config libusb-1.0 --cflags`

libwooting-rgb-sdk.dylib: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-install_name,$0.0 $^ -o $@

$(OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)

install: libwooting-rgb-sdk.dylib
	mkdir -p $(prefix)/lib
	cp libwooting-rgb-sdk.dylib $(prefix)/lib
	chmod 755 $(prefix)/lib/libwooting-rgb-sdk.dylib
	cp libwooting-rgb-sdk.pc $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	chmod 644 $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	cp ../src/wooting-rgb-sdk.h $(prefix)/include/wooting-rgb-sdk.h
	chmod 644 $(prefix)/include/wooting-rgb-sdk.h
	cp ../src/wooting-usb.h $(prefix)/include/wooting-usb.h
	chmod 644 $(prefix)/include/wooting-usb.h

uninstall:
	rm -f $(prefix)/lib/libwooting-rgb-sdk.dylib
	rm -f $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	rm -f $(prefix)/include/wooting-rgb-sdk.h
	rm -f $(prefix)/include/wooting-usb.h

.PHONY: clean libs uninstall

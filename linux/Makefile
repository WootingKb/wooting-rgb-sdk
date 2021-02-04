all: libwooting-rgb-sdk.so

prefix ?= /usr/local

CC ?= gcc
CFLAGS ?= -Wall -g -fPIC
CPPFLAGS ?= #-DDEBUG_LOG
LDFLAGS ?= -Wall -g -Wl,--no-as-needed

OBJS = ../src/wooting-rgb-sdk.o ../src/wooting-usb.o
LIBS = `pkg-config libusb-1.0 --libs` `pkg-config hidapi-libusb --libs`
INCLUDES ?= `pkg-config hidapi-libusb --cflags` -I../src `pkg-config libusb-1.0 --cflags`

libwooting-rgb-sdk.so: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-soname,$0.0 $^ -o $@

$(OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)

install: libwooting-rgb-sdk.so
	install -Dm755 libwooting-rgb-sdk.so $(prefix)/lib/libwooting-rgb-sdk.so
	install -Dm644 libwooting-rgb-sdk.pc $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	install -Dm644 ../src/wooting-rgb-sdk.h $(prefix)/include/wooting-rgb-sdk.h
	install -Dm644 ../src/wooting-usb.h $(prefix)/include/wooting-usb.h
	

uninstall:
	rm -f $(prefix)/lib/libwooting-rgb-sdk.so
	rm -f $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	rm -f $(prefix)/include/wooting-rgb-sdk.h
	rm -f $(prefix)/include/wooting-usb.h

.PHONY: clean libs uninstall

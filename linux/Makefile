all: libwooting-rgb-sdk.so libwooting-rgb-sdk.pc

prefix ?= /usr/local

CC ?= gcc
CDEFS=
CFLAGS ?= -Wall -g -fPIC ${CDEFS}
CPPFLAGS ?= #-DDEBUG_LOG
LDFLAGS ?= -Wall -g -Wl,--no-as-needed

OBJS = ../src/wooting-rgb-sdk.o ../src/wooting-usb.o
LIBS =  `pkg-config hidapi-hidraw --libs`
INCLUDES ?= `pkg-config hidapi-hidraw --cflags` -I../src 

libwooting-rgb-sdk.so: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -shared -fPIC -Wl,-soname,$@.0 $^ -o $@

libwooting-rgb-sdk.pc: libwooting-rgb-sdk.pc.in
	sed -e "s%^prefix=.*%prefix=$(prefix)%" libwooting-rgb-sdk.pc.in > libwooting-rgb-sdk.pc

pc: libwooting-rgb-sdk.pc

$(OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) libwooting-rgb-sdk.pc libwooting-rgb-sdk.so

install: libwooting-rgb-sdk.so libwooting-rgb-sdk.pc
	install -Dm755 libwooting-rgb-sdk.so $(prefix)/lib/libwooting-rgb-sdk.so
	ln -srf $(prefix)/lib/libwooting-rgb-sdk.so $(prefix)/lib/libwooting-rgb-sdk.so.0
	install -Dm644 libwooting-rgb-sdk.pc $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	install -Dm644 ../src/wooting-rgb-sdk.h $(prefix)/include/wooting-rgb-sdk.h
	install -Dm644 ../src/wooting-usb.h $(prefix)/include/wooting-usb.h
	

uninstall:
	rm -f $(prefix)/lib/libwooting-rgb-sdk.so
	rm -f $(prefix)/lib/pkgconfig/libwooting-rgb-sdk.pc
	rm -f $(prefix)/include/wooting-rgb-sdk.h
	rm -f $(prefix)/include/wooting-usb.h

.PHONY: clean libs uninstall

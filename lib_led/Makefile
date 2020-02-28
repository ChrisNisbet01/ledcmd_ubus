DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755
INSTALL_DATA?= install -m 666

CFLAGS +=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS +=-D_GNU_SOURCE
CFLAGS +=-I../..

TARGETS=libled.so

LIB_OBJS=lib_led.o lib_led_control.o lib_led_pattern.o string_constants.o

all: $(TARGETS)

libled.so: $(LIB_OBJS)
	$(CC) $(LDFLAGS) -shared $(^) -lubus -lubox -Wl,-soname,$(@) -o $(@)

.PHONY: install
install: $(TARGETS)
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_BIN) $< $(DESTDIR)$(PREFIX)/lib/$<
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/include/ledcmd_ubus/lib_led
	$(INSTALL_DATA) lib_led.h $(DESTDIR)$(PREFIX)/include/ledcmd_ubus/lib_led/lib_led.h
	$(INSTALL_DATA) lib_led_control.h $(DESTDIR)$(PREFIX)/include/ledcmd_ubus/lib_led/lib_led_control.h
	$(INSTALL_DATA) lib_led_pattern.h $(DESTDIR)$(PREFIX)/include/ledcmd_ubus/lib_led/lib_led_pattern.h
	$(INSTALL_DATA) string_constants.h $(DESTDIR)$(PREFIX)/include/ledcmd_ubus/lib_led/string_constants.h


.PHONY: clean
clean:
	@-rm -f $(TARGETS)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(LIB_OBJS:%.o=%.d)



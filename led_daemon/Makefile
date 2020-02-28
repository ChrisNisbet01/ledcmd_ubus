DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755
INSTALL_DATA?= install -m 666

CFLAGS +=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS +=-D_GNU_SOURCE
CFLAGS +=-I.. -I../.. -I.

LDFLAGS+=-L../lib_led -L../lib_log -L../../libubus_utils

TARGETS=ledcmd_daemon

LED_DAEMON_OBJS=\
	platform_leds_plugin.o \
	led_aliases.o \
	iterate_files.o \
	led_priority_context.o \
	priorities.o \
	led_pattern_control.o \
	led_patterns.o \
	ledcmd_daemon.o \
	led_daemon_ubus.o \
	led_states.o \
	led_priorities.o \
	led_colours.o \
	led_control.o \
	led_lock.o \
	flash_types.o

LIBS=-llog -lled -lubus_utils -lubus -lubox -ljson-c -lblobmsg_json -ldl

.PHONY: all
all: $(TARGETS)

ledcmd_daemon: $(LED_DAEMON_OBJS)
	$(CC) $(LDFLAGS) $(LED_DAEMON_OBJS) $(LIBS) $(LDLIBS) -o $(@)

.PHONY: install
install: ledcmd_daemon
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/etc/config/led_patterns
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/etc/config/led_aliases
	$(INSTALL_BIN) $< $(DESTDIR)$(PREFIX)/bin/$<
	$(INSTALL_DATA) led_patterns/*.json $(DESTDIR)$(PREFIX)/etc/config/led_patterns/
	$(INSTALL_DATA) led_aliases/*.json $(DESTDIR)$(PREFIX)/etc/config/led_aliases/
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/include/led_daemon
	$(INSTALL_DATA) platform_specific.h $(DESTDIR)$(PREFIX)/include/led_daemon/platform_specific.h
	$(INSTALL_DATA) led_states.h $(DESTDIR)$(PREFIX)/include/led_daemon/led_states.h
	$(INSTALL_DATA) led_colours.h $(DESTDIR)$(PREFIX)/include/led_daemon/led_colours.h

.PHONY: clean
clean:
	@-rm -f $(TARGETS)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(LED_DAEMON_OBJS:%.o=%.d)



DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755

CFLAGS +=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS +=-D_GNU_SOURCE
CFLAGS +=-I../..

LDFLAGS +=-L../lib -L../../libubus_utils

TARGETS=led_pattern

LIBS=-lled -lubus_utils

CLI_OBJS=led_pattern.o

.PHONY: all
all: $(TARGETS)

led_pattern: $(CLI_OBJS)
	$(CC) $(LDFLAGS) $(CLI_OBJS) $(LIBS) $(LDLIBS) -o $(@)

.PHONY: install
install:

.PHONY: install
install: led_pattern
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_BIN) $< $(DESTDIR)$(PREFIX)/bin/$<

.PHONY: clean
clean:
	@-rm -f $(TARGETS)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(CLI_OBJS:%.o=%.d)



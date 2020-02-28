DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755

CFLAGS +=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS +=-D_GNU_SOURCE
CFLAGS +=-I.. -I../..

LDFLAGS +=-L../lib -L../../libubus_utils

TARGETS=ledcmd_cli

LIBS=-lled -lubus_utils

CLI_OBJS=ledcmd_cli.o

.PHONY: all
all: $(TARGETS)

ledcmd_cli: $(CLI_OBJS)
	$(CC) $(LDFLAGS) $(CLI_OBJS) $(LIBS) $(LDLIBS) -o $(@)

.PHONY: install
install: ledcmd_cli
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_BIN) $< $(DESTDIR)$(PREFIX)/bin/$<

.PHONY: clean
clean:
	@-rm -f $(TARGETS)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(CLI_OBJS:%.o=%.d)



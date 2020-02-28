DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755
INSTALL_DATA?= install -m 666

CFLAGS+=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS+=-D_GNU_SOURCE
CFLAGS+=-I../..

TARGET=liblog.so

LIB_OBJS=lib_log.o

all: $(TARGET)

$(TARGET): $(LIB_OBJS)
	$(CC) $(LDFLAGS) -shared $(^) -Wl,-soname,$(@) -o $(@)

.PHONY: install
install: $(TARGET)
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/include/lib_log
	$(INSTALL_BIN) $< $(DESTDIR)$(PREFIX)/lib/$<
	$(INSTALL_DATA) logging_plugin.h $(DESTDIR)$(PREFIX)/include/lib_log/logging_plugin.h
	$(INSTALL_DATA) log.h $(DESTDIR)$(PREFIX)/include/lib_log/log.h

.PHONY: clean
clean:
	@-rm -f $(TARGET)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(LIB_OBJS:%.o=%.d)



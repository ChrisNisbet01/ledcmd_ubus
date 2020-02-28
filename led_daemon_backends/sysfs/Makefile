DESTDIR?=
PREFIX?=/usr
INSTALL_DIR?= install -d
INSTALL_BIN?= install -m 755

LIBUBUS_UTILS_INCLUDE_DIR?=../../..
CFLAGS +=-g -Wall -Werror -Wmissing-prototypes -std=gnu11 -fPIC
CFLAGS +=-D_GNU_SOURCE
CFLAGS +=-I../../led_daemon -I$(LIBUBUS_UTILS_INCLUDE_DIR)

TARGET=led_daemon_backend_plugin.so

LIB_OBJS=led_sysfs.o

all: $(TARGET)

$(TARGET): $(LIB_OBJS)
	$(CC) $(LDFLAGS) -shared $(^) -Wl,-soname,$(@) -o $(@)

.PHONY: install
install: all
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_BIN) $(TARGET) $(DESTDIR)$(PREFIX)/lib/$(TARGET)

.PHONY: clean
clean:
	@-rm -f $(TARGET)
	@-find . -type f -name '*.[od]' -delete

CFLAGS += -MMD -MP
-include $(LIB_OBJS:%.o=%.d)



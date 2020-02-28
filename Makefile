DIRS=lib_log lib_led led_daemon led_cli led_pattern

DIRS_y=

DIRS_$(LED_DAEMON_SYSFS) +=\
	led_daemon_backends/sysfs

DIRS_$(LED_DAEMON_TEST) +=\
	led_daemon_backends/test

DIRS_$(LOGGING_STDERR) +=\
	logging_stderr

DIRS+=$(DIRS_y)

export CFLAGS+=-I$(CURDIR) -I$(CURDIR)/..
export LDFLAGS+=-L$(CURDIR)/lib_led -L$(CURDIR)/lib_log -L$(CURDIR)/../libubus_utils

.PHONY: all clean install
all clean install:
	@for d in $(DIRS); do \
		$(MAKE) -C $$d $@ || exit 1 ; \
	done


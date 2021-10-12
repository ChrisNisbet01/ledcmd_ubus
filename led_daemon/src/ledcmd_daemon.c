#include "led_control.h"

#include <lib_log/log.h>
#include <lib_led/string_constants.h>

#include <libubox/uloop.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

static char const default_patterns_directory[] = "/etc/config/led_patterns";
static char const default_aliases_directory[] = "/etc/config/led_aliases";

static bool
is_only_instance(void)
{
	bool only_instance;
	int const fd = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		log_error("Unable to open lock: %m");
		only_instance = false;
		goto done;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		log_error("Unable to obtain lock: %m");
		only_instance = false;
		goto done;
	}

	only_instance = true;

done:
	return only_instance;
}

static void
ignore_sigpipe(void)
{
	struct sigaction sa;

	if (sigaction(SIGPIPE, NULL, &sa) == 0) {
		sa.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &sa, NULL);
	}
}

static bool
run(
	char const * const ubus_path,
	char const * const patterns_directory,
	char const * const aliases_directory,
	char const * const backend_directory)
{
	bool success;

	ignore_sigpipe();

	uloop_init();

	ledcmd_ctx_st * const context =
		ledcmd_init(
			ubus_path,
			patterns_directory,
			aliases_directory,
			backend_directory);

	if (context != NULL) {
		uloop_run();
		success = true;
		ledcmd_deinit(context);
	} else {
		success = false;
	}

	uloop_done();

	return success;
}

static void
usage(FILE * const fp, char const * const program_name)
{
	fprintf(fp,
			"usage: %s [-u ubus_path] [-p pattern_path] [-a LED aliases path] "
			"[-l logging plugin path] [-b LED backend plugin path]\n"
			"LED control daemon\n\n"
			"\t-h\thelp      - this help\n"
			"\t-u\tubus path - UBUS socket path\n"
			"\t-p\tpatterns  - LED patterns directory (default: %s)\n"
			"\t-a\taliases   - LED aliases directory (default: %s)\n"
			"\t-l\tlogging   - Path to logging plugin (default: None)\n"
			"\t-b\tbackend   - Path to backend LED plugin\n",
			program_name,
			default_patterns_directory,
			default_aliases_directory);
}

int
main(int argc, char **argv)
{
	int exit_code;
	char const *ubus_path = NULL;
	char const *patterns_directory = default_patterns_directory;
	char const *aliases_directory = default_aliases_directory;
	char const *backend_directory = NULL;
	char const *logging_plugin_path = NULL;

	int opt;

	while ((opt = getopt(argc, argv, "hm:p:u:b:l:")) != -1) {
		switch (opt) {
			case 'u':
				ubus_path = optarg;
				break;
			case 'p':
				patterns_directory = optarg;
				break;
			case 'm':
				aliases_directory = optarg;
				break;
			case 'b':
				backend_directory = optarg;
				break;
			case 'h':
				usage(stdout, argv[0]);
				exit_code = EXIT_SUCCESS;
				goto done;
			case 'l':
				logging_plugin_path = optarg;
				break;
			default:
				usage(stderr, argv[0]);
				exit_code = EXIT_FAILURE;
				goto done;
		}
	}

	logging_plugin_load(logging_plugin_path, _led_ledcmd, false, false);

	if (!is_only_instance()) {
		log_error("only one instance at a time allowed to run\n");
		exit_code = EXIT_FAILURE;
		goto done;
	}

	log_info("Daemon starting");

	exit_code =
		run(ubus_path, patterns_directory, aliases_directory, backend_directory)
		? EXIT_SUCCESS
		: EXIT_FAILURE;

	log_info("Daemon stopping");

	logging_plugin_unload();

done:
	return exit_code;
}

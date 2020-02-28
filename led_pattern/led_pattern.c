#include <lib_led/lib_led.h>
#include <lib_led/lib_led_pattern.h>
#include <lib_led/string_constants.h>
#include <libubus_utils/ubus_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>

static void
print_pattern(char const * const pattern_name, void * const user_context)
{
	UNUSED_ARG(user_context);

	fprintf(stdout, "%s\n", pattern_name);
}

static void
print_patterns(struct ledcmd_ctx_st const * const ctx)
{
	led_list_patterns(ctx, print_pattern, NULL);
}

static void
print_playing_patterns(struct ledcmd_ctx_st const * const ctx)
{
	led_list_playing_patterns(ctx, print_pattern, NULL);
}

struct play_pattern_context_st {
	bool success;
};

static void
play_pattern_result(
	bool const success, char const * const error_msg, void * const user_ctx)
{
	struct play_pattern_context_st * const context = user_ctx;

	if (error_msg != NULL) {
		fprintf(stdout, "Error: %s\n", error_msg);
	}
	context->success = success;
}

static bool
play_pattern(
	struct ledcmd_ctx_st const * const ctx,
	char const * const pattern_name,
	bool const retrigger)
{
	struct play_pattern_context_st context = {
		.success = false
	};

	led_play_pattern(ctx, pattern_name, retrigger, play_pattern_result, &context);

	return context.success;
}

static int
stop_pattern(
	struct ledcmd_ctx_st const * const ctx, char const * const pattern_name)
{
	struct play_pattern_context_st context = {
		.success = false
	};

	led_stop_pattern(ctx, pattern_name, play_pattern_result, &context);

	return context.success;
}

static void
usage(FILE * const fp)
{
	fprintf(fp,
		"usage:  led_pattern [-h?] -r\n"
		"        -h?    - help    - what you see below\n"
		"        -r     - replay pattern if already playing\n"
		"        led_pattern list               - List patterns\n"
		"        led_pattern list_playing       - List playing patterns\n"
		"        led_pattern play -r <pattern>  - Play pattern <pattern>\n"
		"        led_pattern stop <pattern>     - Stop pattern <pattern>\n"
		"\n");
}

int
main(int argc, char *argv[])
{
	int c;
	int result;
	struct ledcmd_ctx_st const * const ctx = led_init();
	bool retrigger = false;

	if (ctx == NULL) {
		fprintf(stderr, "Unable to connect to ubus\n");
		result = EXIT_FAILURE;
		goto done;
	}

	while ((c = getopt(argc, argv, "?hr")) != -1) {
		switch (c) {
		case '?':
		case 'h':
			usage(stdout);
			result = EXIT_SUCCESS;
			goto done;

		case 'r':
			retrigger = true;
			break;

		default:
			usage(stderr);
			result = EXIT_FAILURE;
			goto done;

		}
	}

	int const args_left = argc - optind;

	if (args_left <= 0) {
		usage(stderr);
		result = EXIT_FAILURE;
		goto done;
	}

	char const * const command = argv[optind];

	if (strcmp(command, "list") == 0) {
		print_patterns(ctx);
		result = EXIT_SUCCESS;
		goto done;
	}
	if (strcmp(command, "list_playing") == 0) {
		print_playing_patterns(ctx);
		result = EXIT_SUCCESS;
		goto done;
	}
	if (args_left < 2) {
		usage(stdout);
		result = EXIT_FAILURE;
		goto done;
	}
	if (strcmp(argv[optind], "play") == 0) {
		result =
			play_pattern(
				ctx,
				argv[optind + 1], retrigger)
				? EXIT_SUCCESS
				: EXIT_FAILURE;
		goto done;
	}
	if (strcmp(argv[optind], "stop") == 0) {
		result =
			stop_pattern(
				ctx,
				argv[optind + 1])
			? EXIT_SUCCESS
			: EXIT_FAILURE;
		goto done;
	}

	fprintf(stdout, "Unknown command\n");
	usage(stdout);
	result = EXIT_FAILURE;

done:
	led_deinit(ctx);

	return result;
}


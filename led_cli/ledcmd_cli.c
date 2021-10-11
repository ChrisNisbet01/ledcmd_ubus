#include <lib_led/lib_led.h>
#include <lib_led/lib_led_control.h>
#include <lib_led/string_constants.h>
#include <ubus_utils/ubus_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>

struct led_state_context_st {
	bool first;
};

static void
print_led_state(char const * const led_state, void * const user_context)
{
	struct led_state_context_st * const context = user_context;

	if (led_state == NULL) {
		goto done;
	}

	fprintf(stdout, "%s%s", context->first ? "" : " ", led_state);
	context->first = false;

done:
	return;
}

static void
print_led_states(struct ledcmd_ctx_st const * const ledcmd_ctx)
{
	struct led_state_context_st context = {
		.first = true
	};

	led_get_states(ledcmd_ctx, print_led_state, &context);
	if (!context.first) {
		fputs("\n", stdout);
	}
}

static void
handle_lock_result(
	struct led_lock_result_st const * const lock_result,
	void * const user_context)
{
	bool * const lock_succeeded = user_context;

	if (lock_result->success) {
		*lock_succeeded = true;
		goto done;
	}

	if (lock_result->error_msg == NULL) {
		goto done;
	}

	fprintf(stdout, "Error: %s", lock_result->error_msg);

	if (lock_result->lock_id != NULL) {
		fprintf(stdout, ". LED is locked with ID: %s", lock_result->lock_id);
	}
	fputs("\n", stdout);

done:
	return;
}

static bool
activate_or_deactivate_led(
	struct ledcmd_ctx_st const * const ctx,
	bool const lock_led,
	char const * const led_priority,
	char const * const lock_id,
	char const * const led_name)
{
	bool lock_succeeded = false;
	bool const success =
		led_activate_or_deactivate(
			ctx,
			lock_led,
			led_name,
			led_priority,
			lock_id,
			handle_lock_result,
			&lock_succeeded)
		&& lock_succeeded;

	if (!success){
		char const * const lock_action =
			lock_led ? _led_activate : _led_deactivate;

		fprintf(stderr, "failed to %s led: %s\n", lock_action, led_name);
	}

	return success;
}

static bool
activate_or_deactivate_leds(
	struct ledcmd_ctx_st const * const ctx,
	bool activate_leds,
	char const * const led_priority,
	char const * const lock_id,
	size_t const argc,
	char * const argv[])
{
	bool success;

	if (argc == 0) {
		char const * const lock_action =
			activate_leds ? _led_activate : _led_deactivate;

		fprintf(stderr, "specify a LED to %s\n", lock_action);
		success = false;
		goto done;
	}

	/* Assume success unless any request command failed. */
	success = true;

	for (size_t i = 0; i < argc; i++) {
		if (!activate_or_deactivate_led(
				ctx, activate_leds, led_priority, lock_id, argv[i])) {
			success = false;
			/* Continue even if a lock/unlock attempt fails. */
		}
	}

done:
	return success;
}

struct led_name_cb_context {
	bool first;
	FILE * const fp;
};

static void
print_led_name(char const * const led_name, void * const user_context)
{
	struct led_name_cb_context * const context = user_context;

	if (led_name != NULL) {
		fprintf(stdout, "%s%s", context->first ? "" : " ", led_name);
		context->first = false;
	}
}

static void
print_led_names(struct ledcmd_ctx_st const * const ctx)
{
	struct led_name_cb_context context = {
		.first = true
	};

	led_get_names(ctx, print_led_name, &context);
	if (!context.first) {
		fputs("\n", stdout);
	}
}

struct led_get_set_context_st {
	bool success;
	bool const is_get_request;
};

static void
led_get_set_handler(
	struct led_get_set_result_st const * const result,
	void * const user_context)
{
	struct led_get_set_context_st * const context = user_context;

	if (!result->success) {
		if (result->error_msg != NULL) {
			fprintf(stdout, "Error: %s", result->error_msg);
			if (result->lock_id != NULL) {
				fprintf(stdout, ". Locked with ID: %s", result->lock_id);
			}
			fputs("\n", stdout);
		}
		context->success = false;
	} else if (context->is_get_request) {
		if (result->led_name != NULL && result->led_state != NULL) {
			fprintf(stdout, "%s %s\n", result->led_name, result->led_state);
		}
	}
}

static bool
get_set_request(
	struct ledcmd_ctx_st const * const ledcmd_ctx,
	char const * const cmd,
	char const * const state,
	char const * const led_name,
	char const * const lock_id,
	char const * const led_priority,
	bool const brief)
{
	struct led_get_set_context_st context = {
		.success = true,
		.is_get_request = strcmp(cmd, _led_get) == 0
	};
	uint32_t const flash_time_ms = brief ? 50 : 0;
	char const * const flash_type = brief ? _led_flash_type_one_shot : NULL;

	bool const success = led_get_set_request(
		ledcmd_ctx,
		cmd,
		state,
		led_name,
		lock_id,
		led_priority,
		flash_type,
		flash_time_ms,
		led_get_set_handler,
		&context);

	return success && context.success;
}

#define led_query(ledcmd_ctx, led_name) \
	({ \
		bool _res; \
		do { \
			_res = get_set_request( \
				ledcmd_ctx, _led_get, NULL, led_name, NULL, NULL, false); \
		} while (0); \
		_res; \
	})

#define led_set_common(ledcmd_ctx, state, led_name, lock_id, led_priority, brief) \
	get_set_request( \
		ledcmd_ctx, _led_set, state, led_name, lock_id, led_priority, brief)

#define led_activate_alternate_priority(ledcmd_ctx, led_name) \
	activate_or_deactivate_led(ledcmd_ctx, true, _led_priority_alternate, NULL, led_name)

#define led_deactivate_alternate_priority(ledcmd_ctx, led_name) \
	activate_or_deactivate_led(ledcmd_ctx, false, _led_priority_alternate, NULL, led_name)

#define led_on_brief(ledcmd_ctx, led_name, lock_id, led_priority) \
	led_set_common(ledcmd_ctx, _led_off, led_name, lock_id, led_priority, true)

#define led_on(ledcmd_ctx, led_name, lock_id, led_priority) \
	led_set_common(ledcmd_ctx, _led_on, led_name, lock_id, led_priority, false)

#define led_off(ledcmd_ctx, led_name, lock_id, led_priority) \
	led_set_common(ledcmd_ctx, _led_off, led_name, lock_id, led_priority, false)

#define led_flash(ledcmd_ctx, led_name, lock_id, led_priority) \
	led_set_common(ledcmd_ctx, _led_flash, led_name, lock_id, led_priority, false)

#define led_fast_flash(ledcmd_ctx, led_name, lock_id, led_priority) \
	led_set_common(ledcmd_ctx, _led_fast_flash, led_name, lock_id, led_priority, false)

/*
 * TODO: Could possibly get this app to read supported features from the
 * daemon.
 * e.g. Some platforms don't actually support fast flashing in the LED driver.
 */
static void
usage(FILE * const fp)
{
	fprintf(fp,
		"usage:  ledcmd [-h?]\n"
		"        ledcmd [-lL]\n"
		"        ledcmd [-k|-K] <lock ID> <LED name> ...\n"
		"        ledcmd [-i <lock ID>] ((-s|-o|-O|-f|-F|-q) <LED name>) ...\n\n"
		"\t-h?   help    - what you see below\n"
		"\t-k    acquire - acquire exclusive access to this LED, locked by <lock ID>\n"
		"\t-K    release - release access to this LED, held by <lock ID>\n"
		"\t-i    lock ID - execute commands with <lock ID> access\n"
		"\t-s    set     - turn LED on briefly\n"
		"\t-o    on      - turn LED on\n"
		"\t-O    off     - turn LED off\n"
		"\t-f    flash   - make LED flash\n"
		"\t-F    flash   - make LED flash fast\n"
		"\t-q    query   - print LED state\n"
		"\t-l    list    - list LEDs\n"
		"\t-L    states  - list LED states\n"
		"\t-n    alton   - set LED to alternate mode\n"
		"\t-N    altoff  - set LED to normal mode\n"
		"\t-a    altbit  - alt LED mode applies to following commands\n"
		"\t-A    ~altbit - alt LED mode does not apply to following commands\n"
		"\n");
}

int
main(int argc, char *argv[])
{
	int c;
	int result;
	char const * lock_id = NULL;
	char const * led_priority = _led_priority_normal;
	struct ubus_context * ubus_ctx = NULL;
	struct ledcmd_ctx_st * ctx = NULL;

	ubus_ctx = ubus_connect(NULL);
	if (ubus_ctx == NULL)
	{
		fprintf(stderr, "Unable to connect to UBUS\n");
		result = EXIT_FAILURE;
		goto done;
	}

	ctx = led_init(ubus_ctx);
	if (ctx == NULL) {
		fprintf(stderr, "Unable to connect to LED daemon\n");
		result = EXIT_FAILURE;
		goto done;
	}

	while ((c = getopt(argc, argv, "aAn:N:lLs:o:O:f:F:q:k:K:i:")) != -1) {
		switch (c) {
		case 'n':
			led_activate_alternate_priority(ctx, optarg);
			break;

		case 'N':
			led_deactivate_alternate_priority(ctx, optarg);
			break;

		case 'a':
			led_priority = _led_priority_alternate;
			break;

		case 'A':
			led_priority = _led_priority_normal;
			break;

		case 'k':
		{
			int const num_args = argc - optind;

			result =
				activate_or_deactivate_leds(
					ctx, true, _led_priority_locked, optarg, num_args, &argv[optind])
				? EXIT_SUCCESS
				: EXIT_FAILURE;
			goto done;
		}

		case 'K':
		{
			int const num_args = argc - optind;

			result =
				activate_or_deactivate_leds(ctx, false, _led_priority_locked, optarg, num_args, &argv[optind])
				? EXIT_SUCCESS
				: EXIT_FAILURE;
			goto done;
		}

		case 'i':
			if (lock_id != NULL) {
				fprintf(stderr, "lock ID has already been specified (%s)", lock_id);
				result = EXIT_FAILURE;
				goto done;
			}
			lock_id = optarg;
			led_priority = _led_priority_locked;
			break;

		case 's':
			if (!led_on_brief(ctx, optarg, lock_id, led_priority)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'o':
			if (!led_on(ctx, optarg, lock_id, led_priority)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'O':
			if (!led_off(ctx, optarg, lock_id, led_priority)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'f':
			if (!led_flash(ctx, optarg, lock_id, led_priority)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'F':
			if (!led_fast_flash(ctx, optarg, lock_id, led_priority)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'q':
			if (!led_query(ctx, optarg)) {
				result = EXIT_FAILURE;
				goto done;
			}
			break;

		case 'l':
			print_led_names(ctx);
			result = EXIT_SUCCESS;
			goto done;

		case 'L':
			print_led_states(ctx);
			result = EXIT_SUCCESS;
			goto done;

		case '?':
		case 'h':
			usage(stdout);
			result = EXIT_SUCCESS;
			goto done;

		default:
			usage(stderr);
			result = EXIT_FAILURE;
			goto done;

		}
	}

	if (argc < 3 || optind != argc) {
		usage(stderr);
		result = EXIT_FAILURE;
		goto done;
	}

	result = EXIT_SUCCESS;

done:
	led_deinit(ctx);
	ubus_free(ubus_ctx);

	return result;
}


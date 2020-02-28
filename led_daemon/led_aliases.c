#include "led_aliases.h"
#include "iterate_files.h"

#include <lib_led/string_constants.h>
#include <libubus_utils/ubus_utils.h>

#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <linux/limits.h>

struct led_alias_st {
	/* This logical LED name may alias to multiple 'real' LED names. */
	char const *name;
	struct avl_node node;

	size_t num_aliased_leds;
	char const * * aliased_leds;
};

struct led_aliases_st {
	struct avl_tree all_aliases;
};

typedef void (*new_alias_fn)
	(struct led_alias_st * const led_alias, void * const user_ctx);

static void
free_const(void const * const mem)
{
	free(UNCONST(mem));
}

static void
free_led_alias(struct led_alias_st const * const alias)
{
	if (alias == NULL) {
		goto done;
	}

	if (alias->aliased_leds != NULL) {
		for (size_t i = 0; i < alias->num_aliased_leds; i++) {
			char const * aliased_name = alias->aliased_leds[i];

			free_const(aliased_name);
		}

		free(alias->aliased_leds);
	}

	free_const(alias);

done:
	return;
}

static void
free_led_aliases(struct led_aliases_st * const led_aliases)
{
	if (led_aliases == NULL) {
		goto done;
	}

	struct avl_tree * const tree = &led_aliases->all_aliases;
	struct led_alias_st *led_alias;
	struct led_alias_st *tmp;

	avl_remove_all_elements(tree, led_alias, node, tmp) {
		free_led_alias(led_alias);
	}

	free_const(led_aliases);

done:
	return;
}

static char const * *
append_alias_name(struct led_alias_st * const alias)
{
	char const * * new_name;
	size_t const new_count = alias->num_aliased_leds + 1;
	char const * * const new_aliases =
		realloc(alias->aliased_leds, new_count * sizeof *new_name);

	if (new_aliases == NULL) {
		new_name = NULL;
		goto done;
	}

	alias->num_aliased_leds = new_count;
	alias->aliased_leds = new_aliases;

	new_name = &alias->aliased_leds[alias->num_aliased_leds - 1];

	memset(new_name, 0, sizeof *new_name);

done:
	return new_name;
}

static bool
parse_alias_names(
	struct led_alias_st * const alias, struct blob_attr const * const attr)
{
	bool success;

	if (attr == NULL) {
		success = false;
		goto done;
	}

	if (!blobmsg_array_is_type(attr, BLOBMSG_TYPE_STRING)) {
		success = false;
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, attr, rem) {
		char const * * new_name = append_alias_name(alias);

		if (new_name == NULL) {
			success = false;
			goto done;
		}

		*new_name = strdup(blobmsg_get_string(cur));
		if (*new_name == NULL) {
			success = false;
			goto done;
		}
	}

	success = true;

done:
	return success;
}

static struct led_alias_st *
parse_led_alias(struct blob_attr const * const attr)
{
	bool success;
	struct led_alias_st *alias = NULL;
	enum {
		ALIAS_NAME,
		ALIAS_ALIASES,
		__ALIAS_MAX
	};
	struct blobmsg_policy const alias_policy[__ALIAS_MAX] = {
		[ALIAS_NAME] =
		{.name = _led_alias_name, .type = BLOBMSG_TYPE_STRING },
		[ALIAS_ALIASES] =
		{.name = _led_alias_aliases, .type = BLOBMSG_TYPE_ARRAY }
	};
	struct blob_attr *fields[__ALIAS_MAX];

	blobmsg_parse(alias_policy, ARRAY_SIZE(alias_policy), fields,
				  blobmsg_data(attr), blobmsg_data_len(attr));

	if (fields[ALIAS_NAME] == NULL) {
		success = false;
		goto done;
	}

	alias = calloc(1, sizeof *alias);
	if (alias == NULL) {
		success = false;
		goto done;
	}

	alias->name = strdup(blobmsg_get_string(fields[ALIAS_NAME]));
	if (alias->name == NULL) {
		success = false;
		goto done;
	}

	if (!parse_alias_names(alias, fields[ALIAS_ALIASES])) {
		success = false;
		goto done;
	}

	success = true;

done:
	if (!success) {
		free_led_alias(alias);
		alias = NULL;
	}

	return alias;
}

static int
alias_name_cmp(void const * const k1, void const * const k2, void * const ptr)
{
	UNUSED_ARG(ptr);

	return strcasecmp(k1, k2);
}

static void
parse_aliases_array(
	struct blob_attr const * const attr,
	new_alias_fn const new_alias_cb,
	void * const user_ctx)
{
	if (!blobmsg_array_is_type(attr, BLOBMSG_TYPE_TABLE)) {
		goto done;
	}

	struct blob_attr *cur;
	int rem;

	blobmsg_for_each_attr(cur, attr, rem) {
		struct led_alias_st * const alias = parse_led_alias(cur);

		if (alias != NULL) {
			new_alias_cb(alias, user_ctx);
		}
	}

done:
	return;
}

static void
parse_led_aliases(
	struct blob_attr const * const attr,
	new_alias_fn const new_alias_cb,
	void * const user_ctx)
{
	enum {
		ALIASES,
		__ALIASES_MAX
	};
	struct blobmsg_policy const aliases_policy[__ALIASES_MAX] = {
		[ALIASES] =
			{ .name = _led_alias_aliases, .type = BLOBMSG_TYPE_ARRAY }
	};
	struct blob_attr *fields[__ALIASES_MAX];

	blobmsg_parse(aliases_policy, ARRAY_SIZE(aliases_policy), fields,
				  blobmsg_data(attr), blobmsg_data_len(attr));

	parse_aliases_array(fields[ALIASES], new_alias_cb, user_ctx);
}

static void
parse_led_aliases_json(
	json_object * const json_obj,
	new_alias_fn const new_alias_cb,
	void * const user_ctx)
{
	struct blob_buf blob;

	blob_buf_full_init(&blob, 0);

	if (!blobmsg_add_json_element(&blob, "", json_obj)) {
		goto done;
	}

	parse_led_aliases(blob_data(blob.head), new_alias_cb, user_ctx);

done:
	blob_buf_free(&blob);

	return;
}

static void
load_aliases_from_file(
	char const * const filename,
	new_alias_fn const new_alias_cb,
	void * const user_ctx)
{
	json_object * const json_obj = json_object_from_file(filename);

	if (json_obj == NULL) {
		goto done;
	}

	parse_led_aliases_json(json_obj, new_alias_cb, user_ctx);

done:
	json_object_put(json_obj);

	return;
}

static void
new_alias_cb(struct led_alias_st * const led_alias, void * const user_ctx)
{
	struct avl_tree * const tree = user_ctx;

	led_alias->node.key = led_alias->name;
	if (avl_insert(tree, &led_alias->node) != 0) {
		/*
		 * Failed to insert the alias.
		 * Free it up.
		 * Log a message?
		 */
		free_led_alias(led_alias);
	}
}

static void
load_alias_cb(char const * const filename, void * const user_ctx)
{
	struct avl_tree * const tree = user_ctx;

	load_aliases_from_file(filename, new_alias_cb, tree);
}

static void
load_aliases_from_directory(
	char const * const aliases_directory, struct avl_tree * const tree)
{
	char path[PATH_MAX];

	snprintf(path, sizeof path, "%s/*.json", aliases_directory);
	iterate_files(path, load_alias_cb, tree);
}

void
led_alias_iterate(
	led_alias_st const *led_alias,
	bool (* const cb)(char const *led_name, void *user_ctx),
	void * const user_ctx)
{
	if (led_alias == NULL) {
		goto done;
	}

	for (size_t i = 0; i < led_alias->num_aliased_leds; i++) {
		char const * const aliased_name = led_alias->aliased_leds[i];
		bool const should_continue = cb(aliased_name, user_ctx);

		if (!should_continue) {
			break;
		}
	}

done:
	return;
}

led_alias_st const *
led_alias_lookup(
	led_aliases_st const * const led_aliases,
	char const * const logical_led_name)
{
	struct led_alias_st *led_alias;

	if (led_aliases == NULL) {
		led_alias = NULL;
		goto done;
	}

	led_alias = avl_find_element(
		&led_aliases->all_aliases, logical_led_name, led_alias, node);

done:
	return led_alias;
}

char const *
led_alias_name(led_alias_st const * const led_alias)
{
	char const * const alias_name =
		(led_alias != NULL) ? led_alias->name : NULL;

	return alias_name;
}

void
led_aliases_iterate(
	led_aliases_st const * const led_aliases,
	bool (* const cb)(led_alias_st const *led_alias, void *user_ctx),
	void * const user_ctx)
{
	if (led_aliases == NULL) {
		goto done;
	}

	struct avl_tree const * const tree = &led_aliases->all_aliases;
	struct led_alias_st *led_alias;

	avl_for_each_element(tree, led_alias, node) {
		cb(led_alias, user_ctx);
	}

done:
	return;
}

void
led_aliases_free(struct led_aliases_st const * const led_aliases)
{
	free_led_aliases(UNCONST(led_aliases));
}

struct led_aliases_st const *
led_aliases_load(char const * const aliases_directory)
{
	struct led_aliases_st *led_aliases;

	if (aliases_directory == NULL) {
		led_aliases = NULL;
		goto done;
	}

	led_aliases = calloc(1, sizeof *led_aliases);

	if (led_aliases == NULL) {
		goto done;
	}

	struct avl_tree * const tree = &led_aliases->all_aliases;
	bool const duplicates_allowed = false;

	avl_init(tree, alias_name_cmp, duplicates_allowed, NULL);

	load_aliases_from_directory(aliases_directory, tree);

done:
	return led_aliases;
}


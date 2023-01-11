/*
 * config.h - Configuration variables and deltas
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef CONFIG_H
#define	CONFIG_H

#include <stdbool.h>

#include <json-c/json.h>

#include "var.h"


struct cfgvar {
	char *name;
	char *value;
	bool keys;		/* variable contains keys of associative
				   array */
	struct cfgvar *next;
};

struct config {
	struct cfgvar *vars;
};

struct delta {
	char *name;
	char *old;		/* NULL if unset */
	char *new;		/* NULL if unset or deleted */
	struct delta *next;
};


bool config_set(struct config *c, const char *name, const char *value);

/*
 * change_to_json has only the set commands. delta_to_json has old and new
 * settings.
 */
struct json_object *change_to_json(const struct delta *d);
struct json_object *delta_to_json(const struct delta *d);

bool delta_same(struct delta *d);

struct delta *config_delta(struct config *c, const struct var *v);
void config_free_delta(struct delta *d);

char *config_hash(struct config *c);
char *config_hash_delta(struct delta *d);

void dump_delta(const struct delta *d);

void config_reset(struct config *c);
struct config *config_new(void);
void config_free(struct config *c);

#endif /* !CONFIG_H */

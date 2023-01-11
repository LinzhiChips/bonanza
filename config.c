/*
 * config.c - Configuration variables and deltas
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

#include "alloc.h"
#include "expr.h"
#include "hash.h"
#include "config.h"


/* ----- Variable setting (from MQTT) -------------------------------------- */


bool config_set(struct config *c, const char *name, const char *value)
{
	struct cfgvar **anchor;
	struct cfgvar *cv;

	for (anchor = &c->vars; *anchor; anchor = &(*anchor)->next) {
		int cmp;

		cv = *anchor;
		cmp = strcmp(name, cv->name);
		if (!cmp) {
			if (!strcmp(cv->value, value))
				return 0;
			free(cv->value);
			if (*value) {
				cv->value = stralloc(value);
			} else {
				*anchor = cv->next;
				free(cv->name);
				free(cv);
			}
			return 1;
		}
		if (cmp < 0)
			break;
	}
	if (!*value)
		return 0;
	cv = alloc_type(struct cfgvar);
	cv->name = stralloc(name);
	cv->value = stralloc(value);
	cv->keys = !strcmp(name, "DEST");
	cv->next = *anchor;
	*anchor = cv;
	return 1;
}


/* ----- Differences ------------------------------------------------------- */


struct json_object *change_to_json(const struct delta *d)
{
	struct json_object *obj = json_object_new_object();

	if (!obj) {
		perror("json_object_new_object");
		return NULL;
	}
	for (; d; d = d->next) {
		struct json_object *value = NULL;

		if (!d->old && !d->new)
			continue;
		if (d->old && d->new && !strcmp(d->old, d->new))
			continue;
		if (d->new) {
			value = json_object_new_string(d->new);
			if (!value) {
				perror("json_object_new_string");
				json_object_put(obj);
				return NULL;
			}
		}
		if (json_object_object_add(obj, d->name, value) < 0) {
			perror("json_object_object_add");
			json_object_put(obj);
			return NULL;
		}
	}
	return obj;
}


static json_object *json_object_string_or_null(const char *s)
{
	json_object *obj;

	if (!s || !*s)
		return NULL;
	obj = json_object_new_string(s);
	if (!obj) {
		perror("json_object_new_string");
		exit(1);
	}
	return obj;
}


struct json_object *delta_to_json(const struct delta *d)
{
	json_object *obj = json_object_new_array();

	if (!obj) {
		perror("json_object_new_object");
		exit(1);
	}
	while (d) {
		json_object *old = json_object_string_or_null(d->old);
		json_object *new = json_object_string_or_null(d->new);
		json_object *entry = json_object_new_object();
		json_object *name;

		if (!entry) {
			perror("json_object_new_object");
			exit(1);
		}

		name = json_object_new_string(d->name);
		if (!name) {
			perror("json_object_new_string");
			exit(1);
		}
		if (json_object_object_add(entry, "name", name) < 0 ||
		    json_object_object_add(entry, "old", old) < 0 ||
		    json_object_object_add(entry, "new", new) < 0) {
			perror("json_object_object_add");
			exit(1);
		}
		if (json_object_array_add(obj, entry) < 0) {
			perror("json_object_array_add");
			exit(1);
		}
		d = d->next;
	}
	return obj;
}


bool delta_same(struct delta *d)
{
	while (d) {
		if (strcmp(d->old ? d->old : "", d->new ? d->new : ""))
			return 0;
		d = d->next;
	}
	return 1;
}


static void delta_add(struct delta ***anchor, const char *name,
    const char *old, const char *new)
{
	struct delta *d;

	if ((!old || !*old) && (!new || !*new))
		return;
	d = alloc_type(struct delta);
	d->name = stralloc(name);
	d->old = old && *old ? stralloc(old) : NULL;
	d->new = new && *new ? stralloc(new) : NULL;
	d->next = NULL;
	**anchor = d;
	*anchor = &d->next;
}


struct delta *config_delta(struct config *c, const struct var *v)
{
	struct cfgvar *cv = c->vars;
	struct delta *d = NULL;
	struct delta **anchor = &d;

	while (cv || v) {
		int cmp;

		while (cv && cv->keys && strcmp(cv->name, "DEST"))
			cv = cv->next;
		if (cv && v)
			cmp = strcmp(cv->name, v->name);
		if (!cv || cmp > 0) {
			delta_add(&anchor, v->name, NULL, v->value->s);
			v = v->next;
		} else if (!v || cmp < 0) {
			delta_add(&anchor, cv->name, cv->value, NULL);
			cv = cv->next;
		} else {
			delta_add(&anchor, cv->name, cv->value, v->value->s);
			v = v->next;
			cv = cv->next;
		}
	}
	return d;
}


void config_free_delta(struct delta *d)
{
	struct delta *next;

	while (d) {
		next = d->next;
		free(d->name);
		free(d->old);
		free(d->new);
		free(d);
		d = next;
	}
}


/* ----- Hashes ------------------------------------------------------------ */


char *config_hash(struct config *c)
{
	const struct cfgvar *cv;

	hash_begin();
	for (cv = c->vars; cv; cv = cv->next) {
		hash_add(cv->name, strlen(cv->name));
		hash_add("=", 1);
		hash_add(cv->value, strlen(cv->value));
		hash_add("\n", 1);
	}
	return hash_end();
}


char *config_hash_delta(struct delta *d)
{
	hash_begin();
	while (d) {
		if (strcmp(d->old ? d->old : "", d->new ? d->new : "")) {
			hash_add(d->name, strlen(d->name));
			hash_add("=", 1);
			if (d->old)
				hash_add(d->old, strlen(d->old));
			hash_add("\n", 1);
			if (d->new)
				hash_add(d->new, strlen(d->new));
			hash_add("\n", 1);
		}
		d = d->next;
	}
	return hash_end();
}


/* ----- Dumping ----------------------------------------------------------- */


void dump_delta(const struct delta *d)
{
	printf("----- Delta -----\n");
	while (d) {
		const char *old = d->old ? d->old : "";
		const char *new = d->new ? d->new : "";

		if (!strcmp(old, new))
			printf(" %s=%s\n", d->name, old);
		else if (*old)
			printf("-%s=%s\n", d->name, old);
		else if (*new)
			printf("+%s=%s\n", d->name, new);
		d = d->next;
	}
	printf("-----\n");
}


/* ----- Construction and restart ------------------------------------------ */


void config_reset(struct config *c)
{
	if (!c)
		return;
	while (c->vars) {
		struct cfgvar *cv = c->vars;

		c->vars = cv->next;
		free(cv->name);
		free(cv->value);
		free(cv);
	}
}


struct config *config_new(void)
{
	struct config *c;

	c = alloc_type(struct config);
	c->vars = NULL;
	return c;
}


void config_free(struct config *c)
{
	config_reset(c);
	free(c);
}

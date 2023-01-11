/*
 * var.c - Variables
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define	_GNU_SOURCE	/* for asprintf */
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "alloc.h"
#include "error.h"
#include "expr.h"
#include "validate.h"
#include "exec.h"
#include "var.h"


static unsigned sequence = 0;


/* ----- Ordering of associative arrays ------------------------------------ */


static struct var **assoc_list(struct var *vars, const char *base, unsigned *n)
{
	struct var **list = NULL;
	size_t base_len = strlen(base);
	struct var *v;

	*n = 0;
	for (v = vars; v; v = v->next) {
		if (strncmp(v->name, base, base_len))
			continue;
		if (v->name[base_len] != '_')
			continue;
		if (!v->assoc)
			continue;
		list = realloc_type_n(list, *n + 1);
		list[*n] = v;
		(*n)++;
	}
	return list;
}


void var_set_keys(struct var *vars, const char *base, const char *keys)
{
	unsigned n;
	struct var **list = assoc_list(vars, base, &n);
	size_t base_len = strlen(base);
	const char *p;

	p = keys;
	while (1) {
		struct var **v;
		const char *end;
		unsigned key_len;

		end = strchr(p, ' ');
		if (!end)
			end = strchr(p, 0);
		key_len = end - p;
		for (v = list; v != list + n; v++)
			if (!strncmp((*v)->name + base_len + 1, p, key_len) &&
			   !(*v)->name[base_len + 1 + key_len] &&
			   (*v)->assoc)
				break;
		if (v)
			(*v)->seq = sequence++;
		else
			fprintf(stderr, "warning: key \"%.*s\" not found\n",
			    (int) (end - p), p);
		if (!*end)
			break;
		p = end + 1;
	}
	free(list);
}


static int cmp_seq(const void *a, const void *b)
{
	const struct var *const *va = a;
	const struct var *const *vb = b;

	return (int) (*va)->seq - (int) (*vb)->seq;
}


char *var_get_keys(struct var *vars, const char *base)
{
	unsigned n;
	struct var **list = assoc_list(vars, base, &n);
	size_t base_len = strlen(base);
	struct var *const *v;
	char *s = NULL;

	qsort(list, n, sizeof(*list), cmp_seq);
	for (v = list; v != list + n; v++) {
		if (s)
			s = stralloc_append(s, " ");
		s = stralloc_append(s, (*v)->name + base_len + 1);
	}
	free(list);
	return s;
}


void var_unset_assoc(struct var **vars, const char *base)
{
	struct var **anchor = vars;
	size_t base_len = strlen(base);

	while (*anchor) {
		struct var *v = *anchor;

		if (!strncmp(v->name, base, base_len) &&
		    v->name[base_len] == '_' && v->assoc) {
			*anchor = v->next;
			free(v->name);
			free_value(v->value);
			free(v);
		} else {
			anchor = &(*anchor)->next;
		}
	}
}


/* ----- Get and  dump variables ------------------------------------------- */


const struct var *var_get_var(const struct var *vars, const char *name,
    const char *key)
{
	const struct var *v;
	char *n = NULL;

	if (key) {
		asprintf_req(&n, "%s_%s", name, key);
		name = n;
	}
	for (v = vars; v; v = v->next) {
		if (key && !v->assoc)
			continue;
		if (!key && v->assoc)
			continue;
		if (!strcmp(v->name, name))
			break;
	}
	free(n);
	return v;
}


const struct value *var_get(const struct var *vars, const char *name,
    const char *key)
{
	const struct var *v = var_get_var(vars, name, key);

	return v ? v->value : NULL;
}


void dump_vars(const struct var *vars)
{
	const struct var *v;

	for (v = vars; v; v = v->next) {
		printf("%s = ", v->name);
		dump_value(v->value);
		printf(" (%u)%s\n", v->seq, v->assoc ? " assoc" : "");
	}
}


/* ----- Set variables ----------------------------------------------------- */


void var_set(struct var **vars, const char *name, const char *key,
    struct value *value, const struct validate *val)
{
	struct var **anchor = vars;
	char *n = NULL;
	struct var *v;

	if (key) {
		asprintf_req(&n, "%s_%s", name, key);
		name = n;
	}

	if (val) {
		switch (validate(val, name, value->s)) {
		case 0:
			errorf("unrecognized variable '%s'", name);
			free(n);
			free_value(value);
			return;
		case 1:
			errorf("invalid value '%s' for variable %s",
			    value->s, name);
			free(n);
			free_value(value);
			return;
		case 2:
			break;
		default:
			abort();
		}
	}

	for (anchor = vars; *anchor; anchor = &(*anchor)->next) {
		int cmp;

		v = *anchor;
		cmp = strcmp(name, v->name);
		if (!cmp) {
			if ((v->assoc && !key) || (!v->assoc && key)) {
				errorf("'%s' is used with and without key",
				    name);
				return;
			}
			free(n);
			free_value(v->value);
			v->value = value;
			v->seq = sequence++;
			return;
		}
		if (cmp < 0)
			break;
	}
	v = alloc_type(struct var);
	v->name = stralloc(name);
	v->value = value;
	v->seq = sequence++;
	v->assoc = key;
	v->next = *anchor;
	*anchor = v;
	free(n);
}


/* ----- Freeing ----------------------------------------------------------- */


void free_vars(struct var *v)
{
	struct var *next;

	while (v) {
		next = v->next;
		free(v->name);
		free_value(v->value);
		free(v);
		v = next;
	}
}


void var_reset_sequence(void)
{
	sequence = 0;
}

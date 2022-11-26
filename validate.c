/*
 * validate.c - Validation of variable names and values
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdbool.h>
#include <string.h>
#include <regex.h>

#include "alloc.h"
#include "validate.h"


#define	ERR_BUF_LEN	200


struct validate_var {
	regex_t name;
	regex_t value;
	struct validate_var *next;
};

struct validate {
	struct validate_var *vars;
};


/* ----- File name validity check ------------------------------------------ */


bool valid_file_name(const char *s)
{
	return !strchr(s, '/');
}


/* ----- Matching ---------------------------------------------------------- */


static bool match(const regex_t *preg, const char *s)
{
	return !regexec(preg, s, 0, NULL, 0);
}


unsigned validate(const struct validate *val, const char *name,
    const char *value)
{
	const struct validate_var *vv;

	for (vv = val->vars; vv; vv = vv->next) {
		if (!*value)
			return 2;
		if (match(&vv->name, name))
			return match(&vv->value, value) ? 2 : 1;
	}
	return 0;
}


/* ----- Construction ------------------------------------------------------ */


static void re(regex_t *preg, const char *s)
{
	char *tmp = stralloc("^(");
	int err;

	while (*s) {
		if (strncmp(s, "\\d", 2)) {
			char c[2] = "?";

			c[0] = *s++;
			tmp = stralloc_append(tmp, c);
		} else {
			tmp = stralloc_append(tmp, "[0-9]");
			s += 2;
		}
	}
	tmp = stralloc_append(tmp, ")$");
	err = regcomp(preg, tmp, REG_EXTENDED | REG_NOSUB | REG_NEWLINE);
	if (err) {
		char buf[ERR_BUF_LEN];

		regerror(err, preg, buf, sizeof(buf));
		fprintf(stderr, "%s: %s\n", tmp, buf);
		exit(1);
	}
	free(tmp);
}


void validate_add(struct validate *val, const char *name, const char *value)
{
	struct validate_var *vv;

	vv = alloc_type(struct validate_var);
	re(&vv->name, name);
	re(&vv->value, value);
	vv->next = val->vars;
	val->vars = vv;
}


struct validate *validate_new(void)
{
	struct validate *val;

	val = alloc_type(struct validate);
	val->vars = NULL;
	return val;
}


void validate_free(struct validate *val)
{
	while (val->vars) {
		struct validate_var *vv = val->vars;

		regfree(&vv->name);
		regfree(&vv->value);
		val->vars = vv->next;
		free(vv);
	}
	free(val);
}

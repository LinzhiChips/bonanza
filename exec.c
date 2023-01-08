/*
 * exec.c - Rules file execution
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "bonanza.h"
#include "alloc.h"
#include "error.h"
#include "expr.h"
#include "var.h"
#include "validate.h"
#include "exec.h"

#include "y.tab.h"


static struct rule **rule_anchor;


/* ----- Execution --------------------------------------------------------- */


void set_clear(const struct setting *self, struct exec_env *exec)
{
	if (verbose)
		printf("%s = {}\n", self->name);
	var_unset_assoc(&exec->cfg_vars, self->name);
}


void set_cfg(const struct setting *self, struct exec_env *exec)
{
	struct value *v = evaluate(self->expr, exec);
	struct value *key = self->key ? evaluate(self->key, exec) : NULL;
	char *s = v->s;

	if (verbose) {
		if (key)
			printf("%s[%s] = \"%s\"\n", self->name, key->s, s);
		else
			printf("%s = \"%s\"\n", self->name, s);
	}
	var_set(&exec->cfg_vars, self->name, key ? key->s : NULL, v,
	    exec->validate);
	if (key)
		free_value(key);
}


void set_var(const struct setting *self, struct exec_env *exec)
{
	struct value *v = evaluate(self->expr, exec);
	struct value *key = self->key ? evaluate(self->expr, exec) : NULL;
	char *s = v->s;

	if (verbose) {
		if (key)
			printf("%s[%s] = \"%s\"\n", self->name, key->s, s);
		else
			printf("%s = \"%s\"\n", self->name, s);
	}
	var_set(&exec->script_vars, self->name, key ? key->s : NULL, v, NULL);
	if (key)
		free_value(key);
	if (magic && !strcmp(self->name, magic)) {
		if (!strcmp(v->s, "stop"))
			exec->flags |= mf_stop;
		if (!strcmp(v->s, "delta"))
			exec->flags |= mf_delta;
	}
}


enum magic_flags run(struct exec_env *exec, const struct rule *r)
{
	struct setting *s;

	while (r && !get_error() && !(exec->flags & mf_stop)) {
		if (!r->cond || bool_evaluate(r->cond, exec))
			for (s = r->settings; s; s = s->next)
				s->op(s, exec);
		r = r->next;
	}

	return exec->flags;
}


void exec_env_init(struct exec_env *exec, const char *dir,
    const struct validate *validate)
{
	exec->dir = dir ? stralloc(dir) : NULL;
	exec->validate = validate;
	exec->cfg_vars = NULL;
	exec->script_vars = NULL;
	exec->flags = 0;
}


void exec_env_free(struct exec_env *exec)
{
	free(exec->dir);
	free_vars(exec->cfg_vars);
	free_vars(exec->script_vars);
}


/* ----- Dumping ----------------------------------------------------------- */


void dump_setting(const struct setting *s)
{
	void (*op)(const struct setting *self, struct exec_env *exec) = s->op;

	printf("%s", s->name);
	if (s->key) {
		printf("[");
		dump_expr(s->key);
		printf("]");
	}
	if (op == set_clear) {
		printf(" = {}");
	} else if (op == set_cfg) {
		printf(" /* cfg */ = ");
		dump_expr(s->expr);
	} else if (op == set_var) {
		printf(" = ");
		dump_expr(s->expr);
	} else {
		abort();
	}
}


static void dump_rule(const struct rule *r)
{
	const char *indent = "";
	const struct setting *s;

	if (r->cond) {
		dump_bool_expr(r->cond);
		printf(":\n");
		indent = "\t";
	}
	for (s = r->settings; s; s = s->next) {
		printf("%s", indent);
		dump_setting(s);
		printf("\n");
	}
}


void dump_rules(const struct rule *r)
{
	while (r) {
		dump_rule(r);
		r = r->next;
	}
}


/* ----- Freeing ----------------------------------------------------------- */


static void free_setting(struct setting *s)
{
	free((void *) s->name);
	if (s->op == set_cfg || s->op == set_var) {
		free_expr(s->expr);
		if (s->key)
			free_expr(s->key);
	} else if (s->op == set_clear) {
		/* nothing to do */
	} else {
		abort();
	}
	free(s);
}


static void free_rule(struct rule *r)
{
	struct setting *s;

	if (r->cond)
		free_bool_expr(r->cond);
	while (r->settings) {
		s = r->settings;
		r->settings = s->next;
		free_setting(s);
	}
	free(r);
}


void free_rules(struct rule *r)
{
	struct rule *next;

	while (r) {
		next = r->next;

		free_rule(r);
		r = next;
	}
}


/* ----- Construction ------------------------------------------------------ */


struct setting *new_setting(
    void (*op)(const struct setting *self, struct exec_env *exec))
{
	struct setting *s;

	s = alloc_type(struct setting);
	s->op = op;
	s->next = NULL;
	return s;
}


void add_rule(struct bool_expr *cond, struct setting *s)
{
	struct rule *r;

	r = alloc_type(struct rule);
	r->cond = cond;
	r->settings = s;
	r->next = NULL;
	*rule_anchor = r;
	rule_anchor = &r->next;
}


struct rule *rules_file(const char *name)
{
	FILE *file = stdin;
	struct rule *rules = NULL;

	file = fopen(name, "r");
	if (!file) {
		errorf("%s: %s", name, strerror(errno));
		return NULL;
	}

	rule_anchor = &rules;
	scan_rules(file, name);
	if (yyparse()) {
		fclose(file);
		free_rules(rules);
		return NULL;
	}
	fclose(file);

	return rules;
}

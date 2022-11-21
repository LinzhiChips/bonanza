/*
 * expr.c - Expressions (evaluation and life-cycle)
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define _GNU_SOURCE	/* for asprintf */
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include "alloc.h"
#include "var.h"
#include "host.h"
#include "map.h"
#include "exec.h"
#include "expr.h"


/* ----- Evaluation -------------------------------------------------------- */


struct value *evaluate(const struct expr *self, const struct exec_env *exec)
{
	return self->op(self, exec);
}


bool bool_evaluate(const struct bool_expr *self, const struct exec_env *exec)
{
	return self->op(self, exec);
}


bool eval_boolean(struct value *v)
{
	bool res;

	/*
	 * Important difference here: 0 == "0", but "0" is "true", while 0 is
	 * "false".
	 */
	if (v->num)
		res = v->n;
	else
		res = *v->s;
	free(v->s);
	free(v);
	return res;
}


char *eval_string(struct value *v)
{
	char *s = v->s;

	free(v);
	return s;
}


/* ----- Boolean operations ------------------------------------------------ */


bool op_or(const struct bool_expr *self, const struct exec_env *exec)
{
	return bool_evaluate(self->a.bool_expr, exec) ||
	    bool_evaluate(self->b.bool_expr, exec);
}


bool op_and(const struct bool_expr *self, const struct exec_env *exec)
{
	return bool_evaluate(self->a.bool_expr, exec) &&
	    bool_evaluate(self->b.bool_expr, exec);
}


bool op_not(const struct bool_expr *self, const struct exec_env *exec)
{
	return !bool_evaluate(self->a.bool_expr, exec);
}


/* ----- Comparisons ------------------------------------------------------- */


static int compare(const struct bool_expr *self, const struct exec_env *exec)
{
	struct value *a = evaluate(self->a.expr, exec);
	struct value *b = evaluate(self->b.expr, exec);
	int res;

	if (a->num && b->num)
		res = a->n < b->n ? -1 : a->n == b->n ? 0 : 1;
	else
		res = strcmp(a->s, b->s);
	free_value(a);
	free_value(b);
	return res;
}


bool op_eq(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) == 0;
}


bool op_ne(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) != 0;
}


bool op_lt(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) < 0;
}


bool op_le(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) <= 0;
}


bool op_gt(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) > 0;
}


bool op_ge(const struct bool_expr *self, const struct exec_env *exec)
{
	return compare(self, exec) >= 0;
}


bool op_in_file(const struct bool_expr *self, const struct exec_env *exec)
{
	struct value *a = evaluate(self->a.expr, exec);
	bool res;
	char *s = NULL;

	if (exec->dir && asprintf(&s, "%s/%s", exec->dir, self->b.s) < 0) {
		perror("asprintf");
		exit(1);
	}
	if (a->num)
		res = file_contains_ipv4(s ? s : self->b.s, a->n);
	else
		res = file_contains_name(s ? s : self->b.s, a->s);
	free(s);
	free_value(a);
	return res;
}


bool op_in_list(const struct bool_expr *self, const struct exec_env *exec)
{
	char *a = eval_string(evaluate(self->a.expr, exec));
	const struct list *e;
	bool found = 0;

	for (e = self->b.list; !found && e; e = e->next) {
		char *b = eval_string(evaluate(e->expr, exec));

		found = !strcasecmp(a, b);
		free(b);
	}
	free(a);
	return found;
}


bool op_bool(const struct bool_expr *self, const struct exec_env *exec)
{
	return eval_boolean(evaluate(self->a.expr, exec));
}


/* ----- String operation -------------------------------------------------- */


struct value *op_concat(const struct expr *self, const struct exec_env *exec)
{
	struct value *a = evaluate(self->a.expr, exec);
	struct value *b = evaluate(self->b.expr, exec);
	size_t a_len = strlen(a->s);
	size_t b_len = strlen(b->s);

	a->s = realloc(a->s, a_len + b_len + 1);
	memcpy(a->s + a_len, b->s, b_len + 1);
	free(b->s);
	free(b);
	return a;
}


/* ----- Value creation ---------------------------------------------------- */


struct value *string_value(const char *s)
{
	struct value *v;

	v = alloc_type(struct value);
	v->num = 0;
	v->s = stralloc(s);
	return v;
}


struct value *numeric_value(const char *s, unsigned n)
{
	struct value *v;

	v = alloc_type(struct value);
	v->num = 1;
	v->n = n;
	if (s)
		v->s = stralloc(s);
	else
		asprintf_req(&v->s, "%u", n);
	return v;
}


static struct value *duplicate_value(const struct value *v)
{
	struct value *new;

	new = alloc_type(struct value);
	new->num = v->num;
	new->s = stralloc(v->s);
	new->n = v->n;
	return new;
}


struct value *op_string(const struct expr *self, const struct exec_env *exec)
{
	return string_value(self->a.s);
}


struct value *op_num(const struct expr *self, const struct exec_env *exec)
{
	return numeric_value(self->a.s, self->b.n);
}


struct value *op_cfg(const struct expr *self, const struct exec_env *exec)
{
	char *key = self->key ? eval_string(evaluate(self->key, exec)) : NULL;
	const struct value *v = var_get(exec->cfg_vars, self->a.s, key);

	free(key);
	return v ? duplicate_value(v) : string_value("");
}


struct value *op_var(const struct expr *self, const struct exec_env *exec)
{
	char *key = self->key ? eval_string(evaluate(self->key, exec)) : NULL;
	const struct value *v = var_get(exec->script_vars, self->a.s, key);

	free(key);
	return v ? duplicate_value(v) : string_value("");
}


struct value *op_map(const struct expr *self, const struct exec_env *exec)
{
	char *key = eval_string(evaluate(self->b.expr, exec));
	const char *value;
	char *s = NULL;

	if (exec->dir && asprintf(&s, "%s/%s", exec->dir, self->a.s) < 0) {
		perror("asprintf");
		exit(1);
	}
	value = file_map(s ? s : self->a.s, key);
	free(s);
	free(key);
	return string_value(value ? value : "");
}


/* ----- Constructors ----------------------------------------------------- */


struct bool_expr *new_bool_op(
    bool (*op)(const struct bool_expr *self, const struct exec_env *exec))
{
	struct bool_expr *e;

	e = alloc_type(struct bool_expr);
	e->op = op;
	return e;
}


struct expr *new_op(
    struct value *(*op)(const struct expr *self, const struct exec_env *exec))
{
	struct expr *e;

	e = alloc_type(struct expr);
	e->op = op;
	return e;
}


struct list *new_list_item(struct expr *expr)
{
	struct list *list;

	list = alloc_type(struct list);
	list->expr = expr;
	list->next = NULL;
	return list;
}


/* ----- Dumping ----------------------------------------------------------- */


void dump_value(const struct value *v)
{
	if (v->num)
		printf("\"%s\" /* 0x%x */", v->s, v->n);
	else
		printf("\"%s\"", v->s);
}


void dump_bool_expr(const struct bool_expr *e)
{
	bool (*op)(const struct bool_expr *self, const struct exec_env *exec) =
	    e->op;

	if (op == op_or) {
		printf("(");
		dump_bool_expr(e->a.bool_expr);
		printf(" || ");
		dump_bool_expr(e->b.bool_expr);
		printf(")");
	} else if (op == op_and) {
		printf("(");
		dump_bool_expr(e->a.bool_expr);
		printf(" && ");
		dump_bool_expr(e->b.bool_expr);
		printf(")");
	} else if (op == op_not) {
		printf("(");
		printf("!");
		dump_bool_expr(e->a.bool_expr);
		printf(")");
	} else if (op == op_bool) {
		dump_expr(e->a.expr);
	} else if (op == op_in_file) {
		dump_expr(e->a.expr);
		printf("in \"%s\"", e->b.s);
	} else if (op == op_in_list) {
		const struct list *l;

		dump_expr(e->a.expr);
		printf(" in (");
		for (l = e->b.list; l; l = l->next) {
			if (l != e->b.list)
				printf(", ");
			dump_expr(l->expr);
		}
		printf(")");
	} else {
		const char *relop;

		if (op == op_eq)
			relop = "==";
		else if (op == op_ne)
			relop = "!=";
		else if (op == op_lt)
			relop = "<";
		else if (op == op_le)
			relop = "<=";
		else if (op == op_gt)
			relop = ">";
		else if (op == op_ge)
			relop = ">=";
		else
			abort();
		dump_expr(e->a.expr);
		printf(" %s ", relop);
		dump_expr(e->b.expr);
	}
}


void dump_expr(const struct expr *e)
{
	struct value *(*op)(const struct expr *self,
	    const struct exec_env *exec) = e->op;

	if (op == op_string) {
		printf("\"%s\"", e->a.s);
	} else if (op == op_num) {
		printf("%s /* 0x%x */", e->a.s, e->b.n);
	} else if (op == op_cfg) {
		printf("%s /* cfg */", e->a.s);
	} else if (op == op_var) {
		printf("%s", e->a.s);
	} else if (op == op_concat) {
		dump_expr(e->a.expr);
		printf(" + ");
		dump_expr(e->b.expr);
	} else if (op == op_map) {
		printf("%s[", e->a.s);
		dump_expr(e->b.expr);
		printf("]");
	} else {
		abort();
	}
}


/* ----- Freeing allocations ----------------------------------------------- */


void free_value(struct value *v)
{
	free(v->s);
	free(v);
}


static void free_list(struct list *list)
{
	struct list *next;

	while (list) {
		free_expr(list->expr);
		next = list->next;
		free(list);
		list = next;
	}
}


void free_bool_expr(struct bool_expr *e)
{
	bool (*op)(const struct bool_expr *self, const struct exec_env *exec) =
	    e->op;

	if (op == op_or || op == op_and) {
		free_bool_expr(e->a.bool_expr);
		free_bool_expr(e->b.bool_expr);
	} else if (op == op_not) {
		free_bool_expr(e->a.bool_expr);
	} else if ( op == op_eq || op == op_ne || op == op_lt || op == op_le ||
	    op == op_gt || op == op_ge) {
		free_expr(e->a.expr);
		free_expr(e->b.expr);
	} else if (op == op_bool) {
		free_expr(e->a.expr);
	} else if (op == op_in_file) {
		free_expr(e->a.expr);
		free(e->b.s);
	} else if (op == op_in_list) {
		free_expr(e->a.expr);
		free_list(e->b.list);
	} else {
		abort();
	}
	free(e);
}


void free_expr(struct expr *e)
{
	struct value *(*op)(const struct expr *self,
	    const struct exec_env *exec) = e->op;

	if (op == op_string || op == op_num || op == op_cfg || op == op_var) {
		free(e->a.s);
	} else if (op == op_concat) {
		free_expr(e->a.expr);
		free_expr(e->b.expr);
	} else if (op == op_map) {
		free(e->a.s);
		free_expr(e->b.expr);
	} else {
		abort();
	}
	free(e);
}

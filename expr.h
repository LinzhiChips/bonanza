/*
 * expr.h - Expressions (evaluation and life-cycle)
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef EXPR_H
#define	EXPR_H

#include <stdbool.h>


struct exec_env;
struct expr;

struct list {
	struct expr *expr;
	struct list *next;
};

struct expr {
	struct value *(*op)(const struct expr *self,
	    const struct exec_env *exec);
	union {
		struct expr *expr;
		char *s;
	} a;
	union {
		struct expr *expr;
		unsigned n;
		char *s;
	} b;
	struct expr *key;
};

struct bool_expr {
	bool (*op)(const struct bool_expr *self, const struct exec_env *exec);
	union {
		struct expr *expr;
		struct bool_expr *bool_expr;
	} a;
	union {
		struct expr *expr;
		struct bool_expr *bool_expr;
		char *s;
		struct list *list;
	} b;
	struct expr *key;
};

struct value {
	bool num;
	char *s;
	unsigned n;
};


bool op_or(const struct bool_expr *self, const struct exec_env *exec);
bool op_and(const struct bool_expr *self, const struct exec_env *exec);
bool op_not(const struct bool_expr *self, const struct exec_env *exec);

bool op_eq(const struct bool_expr *self, const struct exec_env *exec);
bool op_ne(const struct bool_expr *self, const struct exec_env *exec);
bool op_lt(const struct bool_expr *self, const struct exec_env *exec);
bool op_le(const struct bool_expr *self, const struct exec_env *exec);
bool op_gt(const struct bool_expr *self, const struct exec_env *exec);
bool op_ge(const struct bool_expr *self, const struct exec_env *exec);

bool op_in_file(const struct bool_expr *self, const struct exec_env *exec);
bool op_in_list(const struct bool_expr *self, const struct exec_env *exec);

bool op_bool(const struct bool_expr *self, const struct exec_env *exec);

struct value *op_concat(const struct expr *self, const struct exec_env *exec);

struct value *string_value(const char *s);
struct value *numeric_value(const char *s, unsigned n);

struct value *op_string(const struct expr *self, const struct exec_env *exec);
struct value *op_num(const struct expr *self, const struct exec_env *exec);
struct value *op_cfg(const struct expr *self, const struct exec_env *exec);
struct value *op_var(const struct expr *self, const struct exec_env *exec);

struct value *op_map(const struct expr *self, const struct exec_env *exec);

struct bool_expr *new_bool_op(
    bool (*op)(const struct bool_expr *self, const struct exec_env *exec));
struct expr *new_op(
    struct value *(*op)(const struct expr *self, const struct exec_env *exec));
struct list *new_list_item(struct expr *expr);

struct value *evaluate(const struct expr *self, const struct exec_env *exec);
bool bool_evaluate(const struct bool_expr *self, const struct exec_env *exec);
bool eval_boolean(struct value *v);
char *eval_string(struct value *v);

void dump_value(const struct value *v);

void dump_bool_expr(const struct bool_expr *e);
void dump_expr(const struct expr *e);

void free_value(struct value *v);
void free_bool_expr(struct bool_expr *e);
void free_expr(struct expr *e);

#endif /* !EXPR_H */

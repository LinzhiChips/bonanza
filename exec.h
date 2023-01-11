/*
 * exec.h - Rules file execution
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef EXEC_H
#define	EXEC_H

#include <stdbool.h>

#include "expr.h"
#include "validate.h"


struct setting {
	void (*op)(const struct setting *self, struct exec_env *exec);
	const char *name;
	struct expr *expr;
	struct expr *key;
	struct setting *next;
};

struct rule {
	struct bool_expr *cond;
	struct setting *settings;
	struct rule *next;
};

enum magic_flags {
	mf_stop		= 1 << 0,
	mf_delta	= 1 << 1,
	mf_dump		= 1 << 2,
};

struct exec_env {
	/* setup */
	char *dir;
	const struct validate *validate;

	/* run-time */
	struct var *cfg_vars;
	struct var *script_vars;
	enum magic_flags flags;
};


extern bool stop;


void set_clear_cfg(const struct setting *self, struct exec_env *exec);
void set_clear_var(const struct setting *self, struct exec_env *exec);
void set_cfg(const struct setting *self, struct exec_env *exec);
void set_var(const struct setting *self, struct exec_env *exec);

void dump_setting(const struct setting *s);
void dump_rules(const struct rule *c);

struct setting *new_setting(
    void (*op)(const struct setting *self, struct exec_env *exec));
void add_rule(struct bool_expr *cond, struct setting *s);

enum magic_flags run(struct exec_env *exec, const struct rule *r);
void exec_env_init(struct exec_env *exec, const char *dir,
    const struct validate *validate);
void exec_env_free(struct exec_env *exec);

struct rule *rules_file(const char *name);
void free_rules(struct rule *r);

#endif /* !EXEC_H */

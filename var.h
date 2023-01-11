/*
 * var.h - Variables
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef VAR_H
#define	VAR_H

#include <stdbool.h>

#include "validate.h"


struct value;

struct var {
	char *name;
	struct value *value;
	unsigned seq;
	struct var *next;
};


const struct var *var_get_var(const struct var *vars, const char *name,
    const char *key);
const struct value *var_get(const struct var *vars, const char *name,
    const char *key);

void var_set_keys(struct var *vars, const char *base, const char *keys);
char *var_get_keys(struct var *vars, const char *base);
void var_unset_assoc(struct var **vars, const char *base);

void dump_vars(const struct var *vars);

void var_set(struct var **vars, const char *name, const char *key,
    struct value *value, const struct validate *val);
void free_vars(struct var *var);
void var_reset_sequence(void);

#endif /* !VAR_H */

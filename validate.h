/*
 * validate.h - Validation of variable names and values
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef VALIDATE_H
#define	VALIDATE_H

#include <stdbool.h>


struct validate;


bool valid_file_name(const char *s);

/*
 * 0: invalid name
 * 1: valid name, invalid value
 * 2: both name and value are valid
 */

unsigned validate(const struct validate *val, const char *name,
    const char *value);
void validate_add(struct validate *val, const char *name, const char *value);

struct validate *validate_new(void);
void validate_free(struct validate *val);

#endif /* !VALIDATE_H */

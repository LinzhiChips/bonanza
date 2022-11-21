/*
 * hash.h - Wrapper for MD5
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef HASH_H
#define	HASH_H

#include <sys/types.h>


void hash_begin(void);
void hash_add(const void *data, size_t len);
char *hash_end(void);

#endif /* !HASH_H */

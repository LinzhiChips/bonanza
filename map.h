/*
 * map.h - Map file parsing and lookup
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef MAP_H
#define	MAP_H

#include <stdbool.h>


const char *file_map(const char *name, const char *key);

void dump_map_files(void);

void add_mapping(const char *key, const char *value);

void free_map_files(void);

#endif /* !MAP_H */

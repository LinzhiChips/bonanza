/*
 * map.c - Map file parsing and lookup
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "alloc.h"
#include "error.h"
#include "lode.h"
#include "expr.h" /* for y.tab.h */
#include "map.h"

#include "y.tab.h"


struct map_entry {
	const char *key;
	const char *value;
	struct map_entry *next;
};

struct map_file {
	const char *name;
	struct map_entry *entries;
	struct map_file *next;
};


static struct map_file *map_files = NULL;
static struct map_file *current_map_file = NULL;



/* ----- Map file ---------------------------------------------------------- */


static struct map_file *map_file(const char *name)
{
	struct map_file *m;
	FILE *file;

	for (m = map_files; m; m = m->next)
		if (!strcmp(m->name, name))
			return m;

	file = fopen(name, "r");
	if (!file) {
		errorf("%s: %s", name, strerror(errno));
		return NULL;
	}

	m = alloc_type(struct map_file);
	m->name = stralloc(name);
	m->entries = NULL;
	m->next = map_files;
	map_files = m;

	current_map_file = m;
	scan_map(file, name);
	(void) yyparse();
	(void) fclose(file);

	return m;
}


void add_mapping(const char *key, const char *value)
{
	struct map_entry *e = alloc_type(struct map_entry);

	e->key = key;
	e->value = value;
	e->next = current_map_file->entries;
	current_map_file->entries = e;
}


/* ----- Lookup ------------------------------------------------------------ */


const char *file_map(const char *name, const char *key)
{
	const struct map_file *f;
	const struct map_entry *e;

	f = map_file(name);
	if (!f)
		return NULL;
	for (e = f->entries; e; e = e->next)
		if (!strcasecmp(e->key, key))
			return e->value;
	return NULL;
}


/* ----- Dumping ----------------------------------------------------------- */


static void dump_map_string(const char *s)
{
	if (strchr(s, '"')) {
		if (strchr(s, '\''))
			printf("%s", s);
		else
			printf("'%s'", s);
	} else {
		printf("\"%s\"", s);
	}
}


void dump_map_files(void)
{
	const struct map_file *f;
	const struct map_entry *e;

	for (f = map_files; f; f = f->next) {
		printf("### %s:\n", f->name);
		for (e = f->entries; e; e = e->next) {
			dump_map_string(e->key);
			printf("\t");
			dump_map_string(e->value);
			printf("\n");
		}
	}
}


/* ----- Cleanup ----------------------------------------------------------- */


static void free_map_file(struct map_file *f)
{
	free((void *) f->name);
	while (f->entries) {
		struct map_entry *e = f->entries;

		f->entries = e->next;
		free((void *) e->key);
		free((void *) e->value);
		free(e);
	}
	free(f);
}


void free_map_files(void)
{
	while (map_files) {
		struct map_file *f = map_files;

		map_files = f->next;
		free_map_file(f);
	}
}

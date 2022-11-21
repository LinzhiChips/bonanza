/*
 * host.c - Host file parsing and lookup
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
#include "lode.h"
#include "error.h"
#include "expr.h" /* for y.tab.h */
#include "host.h"

#include "y.tab.h"


struct host {
	unsigned ipv4;
	struct host_name *names;
	struct host *next;
};

struct host_file {
	const char *name;
	struct host *hosts;
	struct host_file *next;
};


static struct host_file *host_files = NULL;
static struct host_file *current_host_file = NULL;


/* ----- Host file --------------------------------------------------------- */


static struct host_file *host_file(const char *name)
{
	struct host_file *h;
	FILE *file;

	for (h = host_files; h; h = h->next)
		if (!strcmp(h->name, name))
			return h;

	file = fopen(name, "r");
	if (!file) {
		errorf("%s: %s", name, strerror(errno));
		return NULL;
	}

	h = alloc_type(struct host_file);
	h->name = stralloc(name);
	h->hosts = NULL;
	h->next = host_files;
	host_files = h;

	current_host_file = h;
	scan_hosts(file, name);
	(void) yyparse();
	(void) fclose(file);

	return h;
}


void add_host(unsigned ipv4, struct host_name *names)
{
	struct host *h = alloc_type(struct host);

	h->ipv4 = ipv4;
	h->names = names;
	h->next = current_host_file->hosts;
	current_host_file->hosts = h;
}


/* ----- Lookup ------------------------------------------------------------ */


bool file_contains_ipv4(const char *name, unsigned ipv4)
{
	const struct host_file *f;
	const struct host *h;

	f = host_file(name);
	if (!f)
		return 0;
	for (h = f->hosts; h; h = h->next)
		if (h->ipv4 == ipv4)
			return 1;
	return 0;
}


bool file_contains_name(const char *name, const char *host)
{
	const struct host_file *f;
	const struct host *h;
	const struct host_name *n;

	f = host_file(name);
	if (!f)
		return 0;
	for (h = f->hosts; h; h = h->next)
		for (n = h->names; n; n = n->next)
			if (!strcasecmp(n->name, host))
				return 1;
	return 0;
}


/* ----- Dumping ----------------------------------------------------------- */


void dump_host_files(void)
{
	const struct host_file *f;
	const struct host *h;
	const struct host_name *n;

	for (f = host_files; f; f = f->next) {
		printf("### %s:\n", f->name);
		for (h = f->hosts; h; h = h->next) {
			printf("%d.%d.%d.%d",
			    h->ipv4 >> 24, (h->ipv4 >> 16) & 255,
			    (h->ipv4 >> 8) & 255, h->ipv4 & 255);
			for (n = h->names; n; n = n->next)
				printf("%c%s",
				    n == h->names ? '\t' : ' ', n->name);
			printf("\n");
		}
	}
}


/* ----- Cleanup ----------------------------------------------------------- */


static void free_host(struct host *h)
{
	while (h->names) {
		struct host_name *n = h->names;

		h->names = n->next;
		free(n->name);
		free(n);
	}
	free(h);
}


static void free_host_file(struct host_file *f)
{
	free((void *) f->name);
	while (f->hosts) {
		struct host *h = f->hosts;

		f->hosts = h->next;
		free_host(h);
	}
	free(f);
}


void free_host_files(void)
{
	while (host_files) {
		struct host_file *f = host_files;

		host_files = f->next;
		free_host_file(f);
	}
}


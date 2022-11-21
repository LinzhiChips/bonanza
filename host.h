/*
 * host.h - Host file parsing and lookup
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef HOST_H
#define	HOST_H

#include <stdbool.h>


struct host_name {
	char *name;
	struct host_name *next;
};


bool file_contains_ipv4(const char *name, unsigned ipv4);
bool file_contains_name(const char *name, const char *host);

void dump_host_files(void);

void add_host(unsigned ipv4, struct host_name *names);

void free_host_files(void);

#endif /* !HOST_H */

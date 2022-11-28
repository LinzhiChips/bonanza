/*
 * bonanza.h - Common items for the Linzhi operations daemon
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef BONANZA_H
#define	BONANZA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


#define	IPv4_QUAD_FMT	"%u.%u.%u.%u"
#define	IPv4_QUAD(a)	(a) >> 24, ((a) >> 16) & 255, \
			((a) >> 8) & 255, (a) & 255

#define	COOLDOWN_UPDATE_S	60
#define	COOLDOWN_ERROR_S	120

#define	ACTIVE_DIR		"active"
#define	TEST_DIR		"test"
#define	SCRIPT_NAME		"rules.txt"


struct rule;


extern struct rule *active_rules;
extern const char *magic;
extern unsigned verbose;


void scan_rules(FILE *file, const char *name);
void scan_hosts(FILE *file, const char *name);
void scan_map(FILE *file, const char *name);

#endif /* !BONANZA_H */

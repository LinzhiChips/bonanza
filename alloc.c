/*
 * alloc.c - Common allocation functions
 *
 * Copyright (C) 2021, 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define _GNU_SOURCE	/* for vasprintf */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "alloc.h"


/*
 * Note: we can't just use a macro or inline in alloc.h, since that would cause
 * asprintf to be undefined if stdio.h has been included before, without
 * _GNU_SOURCE. And adding seemingly useless _GNU_SOURCE all over the place
 * would be ugly.
 */

int asprintf_req(char **s, const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = vasprintf(s, fmt, ap);
	va_end(ap);
	if (res < 0) {
		perror("asprintf");
		exit(1);
	}
	return res;
}


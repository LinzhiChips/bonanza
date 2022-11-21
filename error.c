/*
 * error.c - Error reporting
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define	_GNU_SOURCE	/* for vasprintf */
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "alloc.h"
#include "lode.h"
#include "error.h"


unsigned lineno = 1;
char *file_name = NULL;
void (*report)(char *s) = report_fatal;

static char *last_error = NULL;


/* ----- Reporting --------------------------------------------------------- */


void report_fatal(char *s)
{
	fprintf(stderr, "%s\n", s);
	exit(1);
}


void report_store(char *s)
{
	if (verbose)
		fprintf(stderr, "%s\n", s);
	free(last_error);
	last_error = stralloc(s);
}


const char *get_error(void)
{
	return last_error;
}


void clear_error(void)
{
	free(last_error);
	last_error = NULL;
}


/* ----- Error formatting -------------------------------------------------- */


void verrorf(const char *fmt, va_list ap)
{
	char *s;

	if (vasprintf(&s, fmt, ap) < 0) {
		perror("vasprintf");
		exit(1);
	}
	report(s);
	free(s);
}


void errorf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrorf(fmt, ap);
	va_end(ap);
}


void error(const char *s)
{
	errorf("%s", s);
}


/* ----- Parse errors ------------------------------------------------------ */


void yyerrorf(const char *fmt, ...)
{
	va_list ap;
	char *s;

	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	errorf("%s:%u: %s", file_name, lineno, s);
	free(s);
}


void yyerror(const char *s)
{
	yyerrorf("%s", s);
}


/*
 * error.h - Error reporting
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>


extern unsigned lineno;
extern char *file_name;
extern void (*report)(char *s);


void report_fatal(char *s);
void report_store(char *s);

const char *get_error(void);
void clear_error(void);

void verrorf(const char *fmt, va_list ap);
void errorf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
void error(const char *s);

void yyerrorf(const char *fmt, ...);
void yyerror(const char *s);

#endif /* !ERROR_H */

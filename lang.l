/*
 * lang.l - Lexer for rules, host, and map files
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

%{
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "error.h"
#include "expr.h"
#include "exec.h"
#include "bonanza.h"

#include "y.tab.h"


static int start_token = START_RULES;

%}


UCNAME		[A-Z]|[A-Z_][A-Z_0-9]+
LCNAME		[a-z_][A-Za-z_0-9]*|[A-Za-z_][A-Za-z_0-9]*[a-z][A-Za-z_0-9]*
BYTE		[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]

HOST_ELEM	[0-9a-zA-Z][0-9a-zA-Z-]{0,62}
HOST_NAME	{HOST_ELEM}(\.{HOST_ELEM})*

%x		HOSTS MAP

%%

%{
	/*
	 * Nice hack:
	 *
	 * http://www.gnu.org/software/bison/manual/bison.html#
	 *   Multiple-start_002dsymbols
	 */

	if (start_token) {
		int tmp = start_token;

		start_token = 0;
		return tmp;
	}
%}


not		return TOK_NOT;
and		return TOK_AND;
or		return TOK_OR;

"!"		return TOK_NOT;
"&&"		return TOK_AND;
"||"		return TOK_OR;

in		return TOK_IN;

"=="		return TOK_EQ;
"!="		return TOK_NE;
"<"		return TOK_LT;
"<="		return TOK_LE;
">"		return TOK_GT;
">="		return TOK_GE;

([01]"/")?{UCNAME} {
		  yylval.s = stralloc(yytext);
		  return CFGNAME; }

<INITIAL,HOSTS>{BYTE}("."{BYTE}){3} {
		  unsigned a, b, c, d;

		  yylval.n.s = stralloc(yytext);
		  sscanf(yytext, "%u.%u.%u.%u", &a, &b, &c, &d);
		  yylval.n.n = a << 24 | b << 16 | c << 8 | d;
		  return IPv4; }

[0-9]+		{ yylval.n.s = stralloc(yytext);
		  yylval.n.n = strtoul(yytext, NULL, 10);
		  return NUM; }
0x[0-9A-Fa-f]+	{ yylval.n.s = stralloc(yytext);
		  yylval.n.n = strtoul(yytext, NULL, 16);
		  return NUM; }

([01]"/")?{LCNAME} {
		  yylval.s = stralloc(yytext);
		  return NAME; }

<HOSTS>{HOST_NAME} {
		  yylval.s = stralloc(yytext);
		  return HOST;
		}

<INITIAL,MAP>\"[^\"]*\"	{
		  yylval.s = stralloc(yytext + 1);
		  yylval.s[strlen(yytext) - 2] = 0;
		  return STRING; }
<INITIAL,MAP>'[^']*' {
		  yylval.s = stralloc(yytext + 1);
		  yylval.s[strlen(yytext) - 2] = 0;
		  return STRING; }

<MAP>[^ \t\n#][^ \t\n]*	{
		  yylval.s = stralloc(yytext);
		  return STRING; }

<*>[ \t]	;

\n[ \t]+	lineno++;
\n#.*		lineno++;
\n$		lineno++;
\n+		{ lineno += strlen(yytext);
		  return NOINDENT; }
<HOSTS,MAP>\n+	{ /* for clarity, we use NL instead of NOINDENT */
		  lineno += strlen(yytext);
		  return NL; }

^#\ [0-9]+\ \"[^"]*\"(\ [0-9]+)*\n {
		  lineno = strtoul(yytext+2, NULL, 0);
		}

<*>#.*		;

<*>.		return *yytext;

%%

static void start_file(FILE *file, const char *name)
{
	/*
	 * See https://www.gnu.org/software/bison/manual/html_node/How-Can-I-Reset-the-Parser.html
	 * and https://stackoverflow.com/a/11050831/8179289
	 *
	 * Note that yyin must be changed _AFTER_ calling YY_FLUSH_BUFFER,
	 * since yy_flush_buffer (if invoked on YY_CURRENT_BUFFER) calls
	 * yy_load_buffer_state, which resets yyin.
	 */

	YY_FLUSH_BUFFER;
	yyin = file;
	lineno = 1;
	free(file_name);
	file_name = stralloc(name);
}


void scan_rules(FILE *file, const char *name)
{
	BEGIN INITIAL;
	start_token = START_RULES;
	start_file(file, name);
}


void scan_hosts(FILE *file, const char *name)
{
	BEGIN HOSTS;
	start_token = START_HOSTS;
	start_file(file, name);
}


void scan_map(FILE *file, const char *name)
{
	BEGIN MAP;
	start_token = START_MAP;
	start_file(file, name);
}

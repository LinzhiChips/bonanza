/*
 * lang.y - Parser for rules, host, and map files
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

%{
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "alloc.h"
#include "error.h"
#include "validate.h"
#include "expr.h"
#include "exec.h"
#include "host.h"
#include "map.h"
#include "lode.h"
%}

%union {
	char *s;
	struct {
		unsigned n;
		char *s;
	} n;
	bool (*op)(const struct bool_expr *self, const struct exec_env *exec);
	struct bool_expr *bool_expr;
	struct expr *expr;
	struct {
		struct setting *first;
		struct setting *last;
	} settings;
	struct setting *setting;
	struct list *list;
	struct list *list_item;

	struct {
		struct host_name *first;
		struct host_name *last;
	} host_names;
	struct host_name *host_name;
};


%token			START_RULES START_HOSTS START_MAP

%token			NOINDENT
%token			TOK_NOT TOK_AND TOK_OR TOK_IN
%token			TOK_EQ TOK_NE TOK_LT TOK_LE TOK_GT TOK_GE
%token	<s>		CFGNAME NAME STRING
%token	<n>		NUM IPv4

%type	<op>		relational_op
%type	<bool_expr>	condition or_expression and_expression not_expression
%type	<bool_expr>	condition_expression
%type	<expr>		value_expression primary_expression
%type	<list>		opt_list list list_item
%type	<settings>	settings
%type	<setting>	setting

%token			NL
%token	<s>		HOST
%type	<host_name>	host_name
%type	<host_names>	host_names

%%

all:
	START_RULES rules_file
	| START_HOSTS hosts_file
	| START_MAP map_file
	;


/* ===== Rules file ======================================================== */


rules_file:
	| first_rule
	| first_rule more_rules
	| more_rules
	;

more_rules:
	rule
	| more_rules rule
	;

first_rule:
	settings
		{
			add_rule(NULL, $1.first);
		}
	| condition ':' settings
		{
			add_rule($1, $3.first);
		}
	;

rule:
	NOINDENT
	| NOINDENT first_rule
	;

/* ----- Conditions -------------------------------------------------------- */

condition:
	or_expression
		{
			$$ = $1;
		}
	;

or_expression:
	and_expression
		{
			$$ = $1;
		}
	| or_expression TOK_OR and_expression
		{
			$$ = new_bool_op(op_or);
			$$->a.bool_expr = $1;
			$$->b.bool_expr = $3;
		}
	;

and_expression:
	not_expression
		{
			$$ = $1;
		}
	| and_expression TOK_AND not_expression
		{
			$$ = new_bool_op(op_and);
			$$->a.bool_expr = $1;
			$$->b.bool_expr = $3;
		}
	;

not_expression:
	condition_expression
		{
			$$ = $1;
		}
	| TOK_NOT condition_expression
		{
			$$ = new_bool_op(op_not);
			$$->a.bool_expr = $2;
		}
	;

condition_expression:
	value_expression
		{
			$$ = new_bool_op(op_bool);
			$$->a.expr = $1;
		}
	| value_expression TOK_IN STRING
		{
			if (!valid_file_name($3)) {
				yyerror("invalid file name");
				free_expr($1);
				free($3);
				YYABORT;
			}
			$$ = new_bool_op(op_in_file);
			$$->a.expr = $1;
			$$->b.s = $3;
		}
	| value_expression TOK_IN '(' opt_list ')'
		{
			$$ = new_bool_op(op_in_list);
			$$->a.expr = $1;
			$$->b.list = $4;
		}
	| value_expression relational_op value_expression
		{
			$$ = new_bool_op($2);
			$$->a.expr = $1;
			$$->b.expr = $3;
		}
	;

relational_op:
	TOK_EQ
		{
			$$ = op_eq;
		}
	| TOK_NE
		{
			$$ = op_ne;
		}
	| TOK_LT
		{
			$$ = op_lt;
		}
	| TOK_LE
		{
			$$ = op_le;
		}
	| TOK_GT
		{
			$$ = op_gt;
		}
	| TOK_GE
		{
			$$ = op_ge;
		}
	;

opt_list:
		{
			$$ = NULL;
		}
	| list
		{
			$$ = $1;
		}
	| list ','
		{
			$$ = $1;
		}
	;

list:
	list_item
		{
			$$ = $1;
		}
	| list ',' list_item
		{
			$3->next = $1;
			$$ = $3;
		}
	;

list_item:
	value_expression
		{
			$$ = new_list_item($1);
			$$->expr = $1;
			$$->next = NULL;
		}
	;

/* ----- Settings ---------------------------------------------------------- */

settings:
	setting
		{
			$$.first = $1;
			$$.last = $1;
		}
	| settings setting
		{
			$$.first = $1.first;
			$1.last->next = $2;
			$$.last = $2;
		}
	;

setting:
	CFGNAME '=' '{' '}'
		{
			$$ = new_setting(set_clear);
			$$->name = $1;
			$$->key = NULL;
		}
	| CFGNAME '=' value_expression
		{
			$$ = new_setting(set_cfg);
			$$->name = $1;
			$$->expr = $3;
			$$->key = NULL;
		}
	| CFGNAME '[' value_expression ']' '=' value_expression
		{
			$$ = new_setting(set_cfg);
			$$->name = $1;
			$$->expr = $6;
			$$->key = $3;
		}
	| NAME '=' value_expression
		{
			$$ = new_setting(set_var);
			$$->name = $1;
			$$->expr = $3;
			$$->key = NULL;
		}
	;

/* ----- Value ------------------------------------------------------------- */

value_expression:
	primary_expression
		{
			$$ = $1;
		}
	| value_expression '+' primary_expression
		{
			$$ = new_op(op_concat);
			$$->a.expr = $1;
			$$->b.expr = $3;
		}
	;

primary_expression:
	STRING
		{
			$$ = new_op(op_string);
			$$->a.s = $1;
		}
	| NUM
		{
			$$ = new_op(op_num);
			$$->a.s = $1.s;
			$$->b.n = $1.n;
		}
	| IPv4
		{
			$$ = new_op(op_num);
			$$->a.s = $1.s;
			$$->b.n = $1.n;
		}
	| CFGNAME
		{
			$$ = new_op(op_cfg);
			$$->a.s = $1;
			$$->key = NULL;
		}
	| NAME
		{
			$$ = new_op(op_var);
			$$->a.s = $1;
			$$->key = NULL;
		}
	| STRING '[' value_expression ']'
		{
			if (!valid_file_name($1)) {
				yyerror("invalid file name");
				free_expr($3);
				free($1);
				YYABORT;
			}
			$$ = new_op(op_map);
			$$->a.s = $1;
			$$->b.expr = $3;
		}
	;


/* ===== Hosts file ======================================================== */


hosts_file:
	| nls
	| more_hosts opt_nls
	| nls more_hosts nls
	;

more_hosts:
	host_line
	| more_hosts nls host_line
	;

host_line:
	IPv4 host_names
		{
			add_host($1.n, $2.first);
		}
	;

opt_nls:
	| nls
	;

nls:
	NL
	| nls NL
	;

host_names:
	host_name
		{
			$$.first = $1;
			$$.last = $1;
		}
	| host_names host_name
		{
			$$.first = $1.first;
			$1.last->next = $2;
			$$.last = $2;
		}
	;

host_name:
	HOST
		{
			$$ = alloc_type(struct host_name);
			$$->name = $1;
			$$->next = NULL;
		}
	;


/* ===== Map file ========================================================== */


map_file:
	| nls
	| more_mappings opt_nls
	| nls more_mappings nls
	;

more_mappings:
	mapping_line
	| more_mappings nls mapping_line
	;

mapping_line:
	STRING STRING
		{
			add_mapping($1, $2);
		}
	;

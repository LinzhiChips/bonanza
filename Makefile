#
# Copyright (C) 2022 Linzhi Ltd.
#
# This work is licensed under the terms of the MIT License.
# A copy of the license can be found in the file COPYING.txt
#

CFLAGS = -g -Wall -Wextra -Wshadow -Wno-unused-parameter \
	-Wmissing-prototypes -Wmissing-declarations
SLOPPY = -Wno-unused -Wno-implicit-function-declaration
LDFLAGS =
LDLIBS = -lfl -lmosquitto -lmd -ljson-c
OBJS = bonanza.o alloc.o lex.yy.o y.tab.o expr.o exec.o var.o host.o map.o \
       fds.o crew.o mqtt.o miner.o http.o web.o api.o config.o hash.o \
       validate.o error.o

include Makefile.c-common

all::		bonanza

bonanza:	$(OBJS)

bonanza.c:	y.tab.h

lex.yy.c:	lang.l y.tab.h
		$(LEX) lang.l

lex.yy.o: lex.yy.c y.tab.h
		$(CC) -o $@ -c $(CFLAGS) $(SLOPPY) lex.yy.c

y.tab.c y.tab.h: lang.y
#		$(YACC) -Wcounterexamples $(YYFLAGS) -t -d lang.y
		$(YACC) $(YYFLAGS) -t -d lang.y

y.tab.o: y.tab.c y.tab.h
		$(CC) -o $@ -c $(CFLAGS) $(SLOPPY) y.tab.c

clean::
		rm -f y.tab.c y.tab.h lex.yy.c

spotless::	clean
		rm -f bonanza

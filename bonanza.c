/*
 * bonanza.c - Linzhi operations daemon
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include "var.h"
#include "host.h"
#include "map.h"
#include "expr.h"
#include "exec.h"
#include "fds.h"
#include "mqtt.h"
#include "http.h"
#include "crew.h"
#include "exec.h"
#include "miner.h"
#include "api.h"

#include "y.tab.h"

#include "bonanza.h"


struct rule *active_rules = NULL;
const char *magic = NULL;
unsigned verbose = 0;
bool stop = 0;


static void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [-d] [-g address] [-j off|port] [-m host:[port]] [-p port]\n"
"       %*s[-r] [-u] [-v ...] [-Y] [rules__file]\n\n"
"-d, --dump\n"
"\tdon't enter daemon mode, run rules once, dump all data\n"
"-g address, --group=address\n"
"\tuse the specified IPv4 multicast group address, given as dotted quad\n"
"\tDefault: %s\n"
"-j off\n"
"\tdisable the JSON API and the Web user interface\n"
"-j port\n"
"\tnumber of the port the Web server (JSON API and user interface) uses.\n"
"\tDefault: %u\n"
"-p port, --port=port\n"
"\tnumber of the UDP port on which we receive crew messages\n"
"\tDefault: %u\n"
"-M var, --magic=var\n"
"\tdry-run mode; writing to the \"magic\" variable causes special effects:\n"
"\tskip: ignore miner; stop: exit; diff: show differences; dump: dump vars\n"
"-m host[:port]\n"
"\tConnect to the specified MQTT broker for the ops switch. If omitted, the\n"
"\tport defaults to %u. By default, only connection to brokers on miners\n"
"\tare made.\n"
"-r, --restart\n"
"\tautomatically restart miner if configuration update requires it\n"
"-u, --update\n"
"\tautomatically perform configuration updates\n"
"-v, --verbose\n"
"\tverbose operation. Repeating increases verbosity.\n"
"-Y, --yydebug\n"
"\tenable yydebug (for debugging of lsterm only)\n"
	    , name, (int) strlen(name) + 1, "",
	    DEFAULT_MC_ADDR, DEFAULT_HTTP_PORT, DEFAULT_CREW_PORT,
	    MQTT_DEFAULT_PORT);
	exit(1);
}


int main(int argc, char **argv)
{
	struct rule *rules = NULL;
	uint16_t crew_port = DEFAULT_CREW_PORT;
	uint16_t http_port = DEFAULT_HTTP_PORT;
	const char *crew_mc_addr = NULL;
	const char *broker = NULL;
	bool dump = 0;
	char *end;
	int longopt = 0;
	int c;

	const struct option longopts[] = {
		{ "dump",	0,	&longopt,	'd' },
		{ "group",	1,	&longopt,	'g' },
		{ "magic",	1,	&longopt,	'm' },
		{ "port",	1,	&longopt,	'p' },
		{ "restart",	0,	&longopt,	'r' },
		{ "update",	0,	&longopt,	'u' },
		{ "verbose",	0,	&longopt,	'v' },
		{ "yydebug",	0,	&longopt,	'Y' },
		{ NULL,		0,	NULL,		0 }
	};

	while ((c = getopt_long(argc, argv, "dg:M:m:p:r:uvY", longopts,
	    NULL)) != EOF)
		switch (c ? c : longopt) {
		case 'd':
			dump = 1;
			break;
		case 'g':
			crew_mc_addr = optarg;
			break;
		case 'j':
			if (!strcmp(optarg, "off")) {
				http_port = 0;
				break;
			}
			http_port = strtoul(optarg, &end, 0);
			if (*end)
				usage(*argv);
			break;
		case 'M':
			magic = optarg;
			break;
		case 'm':
			broker = optarg;
			break;
		case 'p':
			crew_port = strtoul(optarg, &end, 0);
			if (*end)
				usage(*argv);
			break;
		case 'r':
			auto_restart = 1;
			break;
		case 'u':
			auto_update = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'Y':
			yydebug = 1;
			break;
		default:
			usage(*argv);
		}

	switch (argc - optind) {
	case 1:
		rules = rules_file(argv[optind]);
		break;
	case 0:
		break;
	}

	if (dump) {
		printf("----- Rule files -----\n");
		dump_rules(rules);
		printf("----- Execution -----\n");
	}
	if (dump) {
		struct exec_env exec;

		exec_env_init(&exec, NULL, NULL);
		run(&exec, rules);
		/* @@@ unify with dumping in miner.c */
		printf("----- Host files -----\n");
		dump_host_files();
		printf("----- Map files -----\n");
		dump_map_files();
		printf("----- Configuration variables -----\n");
		dump_vars(exec.cfg_vars);
		printf("----- Variables -----\n");
		dump_vars(exec.script_vars);

		exec_env_free(&exec);
		free_rules(rules);
		free_host_files();
		free_map_files();
		return 0;
	} else {
		active_rules = rules;
	}

	mqtt_init(broker);
	crew_init(crew_port);
	crew_enable_multicast(crew_mc_addr);
	if (http_port)
		http_init(0, http_port);

	while (!stop) {
		fd_poll(1000);
		mqtt_idle();
	}

	miner_destroy_all();
	free_rules(rules);
	free_host_files();
	free_map_files();

	return 0;
}

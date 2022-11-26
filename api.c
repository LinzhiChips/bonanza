/*
 * api.c - HTTP/JSON API functions
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <json-c/json.h>

#include "lode.h"
#include "alloc.h"
#include "error.h"
#include "config.h"
#include "hash.h"
#include "host.h"
#include "map.h"
#include "exec.h"
#include "miner.h"
#include "api.h"


bool auto_update = 0;
bool auto_restart = 0;


/* ----- Helper functions -------------------------------------------------- */


static char *string_or_null(const char *s)
{
	char *res;

	if (!s)
		return stralloc("null");
	asprintf_req(&res, "\"%s\"", s);
	return res;
}


static char *quad_or_null(uint32_t ipv4)
{
	char *res;

	if (!ipv4)
		return stralloc("null");
	asprintf_req(&res, "\"" IPv4_QUAD_FMT "\"", IPv4_QUAD(ipv4));
	return res;

}


/* ----- /miners: list of miners and their state --------------------------- */


static const char *status_name[] = {
	[ms_connecting]	= "connecting",
	[ms_syncing]	= "sync",
	[ms_idle]	= "idle",
	[ms_shutdown]	= "shutdown"
};


static const char *delta_state(struct delta *d)
{
	bool add = 0;
	bool del = 0;

	while (d) {
		del |= d->old &&
		    (!d->new || strcmp(d->old, d->new));
		add |= (!d->old && d->new) ||
		    (d->old && d->new && strcmp(d->old, d->new));
		if (add && del)
			return "change";
		d = d->next;
	}
	if (add)
		return "add";
	if (del)
		return "del";
	return "same";
}


char *miners_json(void)
{
	const struct miner *m;
	char *s = stralloc("[ ");

	for (m = miners; m; m = m->next) {
		char *name = string_or_null(m->name);
		char *ipv4 = quad_or_null(m->ipv4);
		char *hash = m->config ? config_hash(m->config) : NULL;
		char *miner_hash = string_or_null(m->config ? hash : NULL);
		char *delta_hash = NULL;
		const char *state = status_name[m->state];
		char *error = string_or_null(m->error);
		char *restart = string_or_null(m->restart);
		char *new;

		free(hash);
		if (m->delta) {
			char *tmp;

			state = delta_state(m->delta);
			if (strcmp(state, "same") && m->cooldown > now)
				state = "updating";
			tmp = config_hash_delta(m->delta);
			delta_hash = string_or_null(tmp);
			free(tmp);
		}
		asprintf_req(&new,
		    "%s{ \"id\":%u, \"name\":%s, \"ipv4\":%s, "
		    "\"state\":\"%s\",\n"
		    "\"miner_hash\":%s,\n"
		    "\"delta_hash\":%s,\n"
		    "\"error\":%s,\n"
		    "\"restart\":%s,\n"
		    "\"last_seen\":%llu }",
		    m == miners ? "" : ",\n", m->id, name, ipv4, state,
		    miner_hash, delta_hash ? delta_hash : "null", error,
		    restart, (unsigned long long) m->last_seen);
		free(name);
		free(ipv4);
		free(miner_hash);
		free(delta_hash);
		free(error);
		free(restart);
		s = stralloc_append(s, new);
		free(new);
	}
	return stralloc_append(s, " ]\n");
}


/* ----- /miner?id=id: miner details -------------------------------------- */


static char *delta_string(const struct miner *m)
{
	char *s = stralloc("");
	struct delta *d;
	char *new;

	for (d = m->delta; d; d = d->next) {
		char *old_value = string_or_null(d->old);
		char *new_value = string_or_null(d->new);

		asprintf_req(&new,
		    "%s{ \"name\":\"%s\", \"old\":%s, \"new\":%s }",
		    d == m->delta ? "" : ",\n", d->name, old_value, new_value);
		free(old_value);
		free(new_value);
		s = stralloc_append(s, new);
		free(new);
	}
	return s;
}


static char *config_string(const struct miner *m)
{
	char *s = stralloc("");
	struct cfgvar *cv;
	char *new;

	if (!m->config)
		return s;
	for (cv = m->config->vars; cv; cv = cv->next) {
		asprintf_req(&new,
		    "%s{ \"name\":\"%s\", \"old\":\"%s\", \"new\":\"%s\" }",
		    cv == m->config->vars? "" : ",\n",
		    cv->name, cv->value, cv->value);
		s = stralloc_append(s, new);
		free(new);
	}
	return s;
}


char *miner_json(uint32_t id)
{
	const struct miner *m;
	char *s, *name, *serial0, *serial1, *hash, *list;

	m = miner_by_id(id);
	if (!m)
		return NULL;

	name = string_or_null(m->name);
	serial0 = string_or_null(m->serial[0]);
	serial1 = string_or_null(m->serial[1]);
	hash = config_hash_delta(m->delta);

	if (m->delta)
		list = delta_string(m);
	else
		list = config_string(m);

	asprintf_req(&s,
	    "{ \"id\":%u, \"name\":%s, \"serial\":[%s,%s], \"delta\":[\n"
	    "%s ],\n"
	    "\"delta_hash\":\"%s\" }\n",
		m->id, name, serial0, serial1, list, hash);
	free(name);
	free(serial0);
	free(serial1);
	free(list);
	free(hash);

	return s;
}


/* ----- GET /test-path ---------------------------------------------------- */


char *get_path(bool test)
{
	char host[256]; /* see man 3 gethostname */
	char *pwd = getcwd(NULL, 0); /* glibc extension */
	char *s;

	if (gethostname(host, sizeof(host) - 1) < 0) {
		perror("gethostname");
		exit(1);
	}
	if (!pwd) {
		perror("getcwd");
		exit(1);
	}
	asprintf_req(&s, "%s:%s/%s/%s",
	    host, pwd, test ? TEST_DIR : ACTIVE_DIR, SCRIPT_NAME);
	free(pwd);
	return s;
}


/* ----- POST /update (with id) -------------------------------------------- */


static char *do_update(uint32_t id, bool restart)
{
	struct miner *m;

	m = miner_by_id(id);
	if (!m)
		return NULL;
	return stralloc(consider_updating(m, 1, restart));
}


char *miner_update_group(const char *hash, bool restart)
{
	struct miner *m;
	unsigned n = 0;
	char *s;

	for (m = miners; m; m = m->next) {

		if (!m->delta)
			continue;
		if (delta_same(m->delta))
			continue;
		if (hash != NULL) {
			char *tmp;
			bool match;

			tmp = config_hash_delta(m->delta);
			match = !strcmp(hash, tmp);
			free(tmp);
			if (!match)
				continue;
		}
		consider_updating(m, 1, restart);
		n++;
	}
	asprintf_req(&s, "sent %u update%s", n, n == 1 ? "" : "s");
	return s;
}


char *miner_update_all(bool restart)
{
	return miner_update_group(NULL, restart);
}


char *miner_update(uint32_t id)
{
	return do_update(id, 0);
}


char *miner_update_restart(uint32_t id)
{
	return do_update(id, 1);
}


/* ----- POST /run (with id) ----------------------------------------------- */


static char *run_result(char *error, struct delta *delta)
{
	json_object *obj;
	json_object *string = NULL;
	json_object *delta_obj = NULL;
	const char *tmp;
	char *s;

	obj = json_object_new_object();
	if (!obj) {
		perror("json_object_new_object");
		exit(1);
	}
	if (error) {
		string = json_object_new_string(error);
		if (!string) {
			perror("json_object_new_string");
			exit(1);
		}
		free(error);
	} else {
		delta_obj = delta_to_json(delta);
	}
	config_free_delta(delta);
	if (json_object_object_add(obj, "error", string) < 0) {
		perror("json_object_object_add");
		exit(1);
	}
	if (delta_obj) {
		if (json_object_object_add(obj, "delta", delta_obj) < 0) {
			perror("json_object_object_add");
			exit(1);
		}
	}
	tmp = json_object_to_json_string(obj);
	if (!tmp) {
		perror("json_object_to_json_string");
		exit(1);
	}
	s = stralloc(tmp);
	json_object_put(obj);
	return s;
}


char *miner_run(uint32_t id)
{
	struct miner_env env;
	struct miner *m;
	struct rule *rules;
	char *error = NULL;
	struct delta *delta = NULL;

	m = miner_by_id(id);
	if (!m)
		return NULL;
	if (!miner_can_calculate(m))
		return run_result(stralloc("Wait for more miner data"),
		    NULL);

	report = report_store;
	rules = rules_file(TEST_DIR "/" SCRIPT_NAME);
	report = report_fatal;
	if (get_error()) {
		error = stralloc(get_error());
		clear_error();
		free_rules(rules);
		return run_result(error, NULL);
	}

	miner_calculate(&env, m, TEST_DIR, rules);
	free_rules(rules);
	miner_calculation_finish(&env, &error, &delta);

	return run_result(error, delta);
}


/* ----- POST /reload ------------------------------------------------------ */


char *miner_reload(void)
{
	struct rule *rules;
	struct miner *m;
	char *error;

	free_host_files();
	free_map_files();

	report = report_store;
	rules = rules_file(ACTIVE_DIR "/" SCRIPT_NAME);
	report = report_fatal;
	if (get_error()) {
		error = stralloc(get_error());
		clear_error();
		free_rules(rules);
		return error;
	}

	free_rules(active_rules);
	active_rules = rules;

	for (m = miners; m; m = m->next) {
		struct miner_env env;

		if (!miner_can_calculate(m))
			continue;
		miner_calculate(&env, m, ACTIVE_DIR, active_rules);
		free(m->error);
		config_free_delta(m->delta);
		miner_calculation_finish(&env, &m->error, &m->delta);
		consider_updating(m, 0, auto_restart);
	}
	return stralloc("");
}

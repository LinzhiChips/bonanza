/*
 * miner.c - Miner state tracking
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
#include <assert.h>

#include <json-c/json.h>

#include "lode.h"
#include "alloc.h"
#include "error.h"
#include "fds.h"
#include "mqtt.h"
#include "expr.h"
#include "var.h"
#include "exec.h"
#include "config.h"
#include "validate.h"
#include "api.h"
#include "miner.h"


struct miner *miners = NULL;


/* ----- Apply update ------------------------------------------------------ */


const char *consider_updating(struct miner *m, bool request, bool restart)
{
	json_object *obj, *change_obj;
	const char *s;

	if (verbose > 2)
		fprintf(stderr, "consider_updating (1, %u)\n",
		    (unsigned) m->cooldown);

	if (!m->delta)
		return "nothing to do";
	if (!request && m->cooldown > now)
		return "cooling down";
	if (!request && !auto_update)
		return "ready for update";

	change_obj = change_to_json(m->delta);
	if (!change_obj) {
		m->cooldown = now + COOLDOWN_ERROR_S;
		return "could not generate changes";
	}
	obj = json_object_new_object();
	if (!obj) {
		json_object_put(change_obj);
		m->cooldown = now + COOLDOWN_ERROR_S;
		return "could not generate changes";
	}
	if (json_object_object_add(obj, "change", change_obj) < 0) {
		json_object_put(change_obj);
		json_object_put(obj);
		m->cooldown = now + COOLDOWN_ERROR_S;
		return "could not generate changes";
	}
	if (restart) {
		json_object *restart_obj = json_object_new_boolean(1);

		if (!restart_obj) {
			json_object_put(obj);
			m->cooldown = now + COOLDOWN_ERROR_S;
			return "could not generate changes";
		}
		if (json_object_object_add(obj, "restart", restart_obj) < 0) {
			json_object_put(restart_obj);
			json_object_put(obj);
			m->cooldown = now + COOLDOWN_ERROR_S;
			return "could not generate changes";
		}
	}
	s = json_object_to_json_string(obj);
	if (!s) {
		json_object_put(obj);
		m->cooldown = now + COOLDOWN_ERROR_S;
		return "could not generate delta";
	}
	m->cooldown = now + COOLDOWN_UPDATE_S;

	mqtt_printf(m->mosq, "/config/bulk-set", qos_ack, 0, "%s", s);
	update_poll(m);

	/*
	 * the print buffer returned by json_object_to_json_string is stored in
	 * the object, so we must not free the object before we've finished
	 * using the buffer.
	 */
	json_object_put(obj);
	return "update sent";
}


/* ----- Calculate configuration ------------------------------------------- */


bool miner_can_calculate(const struct miner *m)
{
	if (!m->ipv4)
		return 0;
	if (!m->name || !m->serial[0] || !m->serial[1])
		return 0;

	if (!m->validate)
		return 0;
	if (!m->config)
		return 0;

	return 1;
}


static void initialize_vars(struct miner_env *env)
{
	char buf[4 * 3 + 3 + 1];
	const char *dest = NULL;
	const struct cfgvar *cv;
	const struct miner *m = env->miner;

	assert(!env->exec.cfg_vars);
	assert(!env->exec.script_vars);

	sprintf(buf, "0x%x", m->id);
	var_set(&env->exec.script_vars, "id", NULL, numeric_value(buf, m->id),
	    NULL);

	sprintf(buf, IPv4_QUAD_FMT, IPv4_QUAD(m->ipv4));
	var_set(&env->exec.script_vars, "ip", NULL,
	     numeric_value(buf, m->ipv4), NULL);

	var_set(&env->exec.script_vars, "name", NULL,
	    string_value(m->name), NULL);
	var_set(&env->exec.script_vars, "0/serial", NULL,
	    string_value(m->serial[0]), NULL);
	var_set(&env->exec.script_vars, "1/serial", NULL,
	    string_value(m->serial[1]), NULL);

	/*
	 * We don't bother with using keys here since we need to order the
	 * sequence numbers according to DEST anyway.
	 */
	for (cv = m->config->vars; cv; cv = cv->next) {
		if (strcmp(cv->name, "DEST"))
			var_set(&env->exec.cfg_vars, cv->name,  NULL,
			    string_value(cv->value), env->exec.validate);
		else
			dest = cv->value;
	}
	if (dest)
		var_set_keys(env->exec.cfg_vars, "DEST", dest);
}


static void finalize_vars(struct miner_env *env)
{
	char *dest_keys = var_get_keys(env->exec.cfg_vars, "DEST");

	if (dest_keys) {
		var_set(&env->exec.cfg_vars, "DEST",  NULL,
		    string_value(dest_keys), NULL);
		free(dest_keys);
	}
}


bool miner_calculate(struct miner_env *env, struct miner *m,
    const char *dir, const struct rule *rules)
{
	exec_env_init(&env->exec, dir, m->validate);
	env->miner = m;
	env->cfg_vars = NULL;
	env->vars = NULL;
	env->delta  = NULL;
	env->error = NULL;
	env->flags = 0;

	report = report_store;
	initialize_vars(env);
	if (!get_error())
		env->flags = run(&env->exec, rules);
	report = report_fatal;

	if (get_error()) {
		env->error = stralloc(get_error());
		clear_error();
		return 0;
	}

	if (verbose > 1 || (env->flags & mf_dump)) {
		printf("----- Configuration variables -----\n");
		dump_vars(env->exec.cfg_vars);
		printf("-----\n");
	}
	if (verbose > 2 || (env->flags & mf_dump)) {
		printf("----- Variables -----\n");
		dump_vars(env->exec.script_vars);
		printf("-----\n");
	}

	finalize_vars(env);
	env->delta = config_delta(env->miner->config, env->exec.cfg_vars);

	if (env->flags & mf_delta)
		dump_delta(env->delta);

	return 1;
}


void miner_calculation_finish(struct miner_env *env, char **error,
    struct delta **delta)
{
	if (delta)
		*delta = env->delta;
	else
		config_free_delta(env->miner->delta);
	exec_env_free(&env->exec);
	if (error)
		*error = env->error;
	else
		free(env->error);
}


static void consider_calculation(struct miner *m)
{
	struct miner_env env;

	if (!miner_can_calculate(m))
		return;

	miner_calculate(&env, m, ACTIVE_DIR, active_rules);

	free(m->error);
	config_free_delta(m->delta);

	miner_calculation_finish(&env, &m->error, &m->delta);

	if (env.flags & mf_stop) {
		stop = 1;
		return;
	}

	consider_updating(m, 0, auto_restart);
}


/* ----- Lookups ----------------------------------------------------------- */


struct miner *miner_by_id(uint32_t id)
{
	struct miner *m;

	for (m = miners; m; m = m->next)
		if (m->id == id)
			return m;
	return NULL;
}


struct miner *miner_by_ipv4(uint32_t ipv4)
{
	struct miner *m;

	for (m = miners; m; m = m->next)
		if (m->ipv4 == ipv4)
			return m;
	return NULL;
}


/* ----- Modification ------------------------------------------------------ */


void miner_set_name(struct miner *m, const char *name)
{
	if (m->name && !strcmp(m->name, name))
		return;
	free(m->name);
	m->name = stralloc(name);
	consider_calculation(m);
}


void miner_set_serial(struct miner *m, const char *serial0, const char *serial1)
{
	if (m->serial[0] && !strcmp(m->serial[0], serial0) &&
	    !strcmp(m->serial[1], serial1))
		return;
	free(m->serial[0]);
	free(m->serial[1]);
	m->serial[0] = stralloc(serial0);
	m->serial[1] = stralloc(serial1);
	consider_calculation(m);
}


/* ----- Configuration ----------------------------------------------------- */


static struct validate *process_validate(const char *s)
{
	struct validate *val = validate_new();
	char *tmp = stralloc(s);
	const char *p = tmp;

	while (1) {
		char *q, *eq;

		q = strchr(p, '\n');
		if (!q)
			break;
		*q = 0;
		eq = strchr(p, '=');
		if (!eq) {
			fprintf(stderr, "%s: no equal sign\n", s);
			exit(1);
		}
		*eq = 0;
		validate_add(val, p, eq + 1);
		p = q + 1;
	}
	free(tmp);
	return val;
}


static void process_config(struct miner *m, const char *s)
{
	struct json_object *obj;
	json_object_iter iter;
	enum json_tokener_error err;

	config_free(m->config);
	m->config = NULL;

	obj = json_tokener_parse_verbose(s, &err);
	if (!obj) {
		fprintf(stderr, "JSON \"%s\": %s\n",
		    s, json_tokener_error_desc(err));
		return;
	}
	json_object_object_foreachC(obj, iter) {
		if (!json_object_is_type(iter.val, json_type_string)) {
			fprintf(stderr,
			    "expected string value for JSON pair \"%s\"\n",
			    iter.key);
			continue;
		}
		if (!m->config)
			m->config = config_new();
		config_set(m->config, iter.key,
		    json_object_get_string(iter.val));
	}
	json_object_put(obj);

	if (m->config)
		consider_calculation(m);
}


void miner_deliver(struct miner *m, const char *topic, const char *payload)
{
	if (!strcmp(topic, "/config/bulk")) {
		process_config(m, payload);
		consider_calculation(m);
		return;
	}
	if (!strcmp(topic, "/config/accept")) {
		bool new = !m->validate;

		if (!new)
			validate_free(m->validate);
		m->validate = process_validate(payload);
		if (new)
			consider_calculation(m);
		return;
	}
	if (!strcmp(topic, "/config/restart-pending")) {
		free(m->restart);
		m->restart = strcmp(payload, "-") ? stralloc(payload) : NULL;
		return;
	}
}


/* ----- Reset (reconnect MQTT) -------------------------------------------- */


void miner_reset(struct miner *m)
{
	m->state = ms_connecting;
	config_reset(m->config);
	config_free_delta(m->delta);
	m->delta = NULL;
	if (m->validate) {
		validate_free(m->validate);
		m->validate = NULL;
	}
	free(m->error);
	m->error = NULL;
	free(m->restart);
	m->restart = NULL;
}


void miner_destroy(struct miner *m)
{
	free(m->name);
	free(m->serial[0]);
	free(m->serial[1]);
	if (m->mosq)
		mosquitto_destroy(m->mosq);
	if (m->fd)
		fd_del(m->fd);
	miner_reset(m);
	config_free(m->config);
	free(m->restart);
	free(m);
}


void miner_destroy_all(void)
{
	while (miners) {
		struct miner *m = miners;

		miners = m->next;
		miner_destroy(m);
	}
}


/* ----- Creation ---------------------------------------------------------- */


struct miner *miner_new(uint32_t id)
{
	struct miner *m;

	m = alloc_type(struct miner);
	m->id = id;
	m->ipv4 = 0;
	m->name = NULL;
	m->serial[0] = m->serial[1] = NULL;
	m->last_seen = now;

	m->state = ms_connecting;
	m->mosq = NULL;
	m->fd = NULL;
	m->validate = NULL;
	m->config = NULL;
	m->restart = NULL;

	m->delta = NULL;
	m->error = NULL;
	m->cooldown = 0;

	m->next = miners;
	miners = m;

	return m;
}

/*
 * sw.c - Operations switch control
 *
 * Copyright (C) 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

#include "error.h"
#include "var.h"
#include "mqtt.h"
#include "miner.h"
#include "sw.h"


#define	DEFAULT_SW_REFRESH_S	600	/* ops switch refresh interval */


/* ----- Registry of unique names ------------------------------------------ */


struct sw_topic {
	const char *topic;
	bool on;
	struct sw_topic *next;
};


static struct sw_topic *topics = NULL;


const struct sw_topic *sw_listen(const char *topic)
{
	struct sw_topic **anchor;

	for (anchor = &topics; *anchor; anchor = &(*anchor)->next)
		if (!strcmp((*anchor)->topic, topic))
			return *anchor;
	*anchor = alloc_type(struct sw_topic);
	(*anchor)->topic = stralloc(topic);
	(*anchor)->on = 1;
	(*anchor)->next = NULL;
	broker_subscribe(topic);
	return *anchor;
}


void sw_subscribe(void)
{
	const struct sw_topic *topic;

	for (topic = topics; topic; topic = topic->next)
		broker_subscribe(topic->topic);
}


void sw_cleanup(void)
{
	while (topics) {
		struct sw_topic *next = topics->next;

		free((void *) topics->topic);
		free(topics);
		topics = next;
	}
}


/* ----- Per-miner data ---------------------------------------------------- */


struct sw_miner {
	const struct sw_topic *topic;
	uint32_t mask;
	struct sw_miner *next;
};


static void sw_set_miner(struct miner *m, uint32_t mask, bool on)
{
	uint32_t old_value = m->sw_value;
	uint32_t old_mask = m->sw_mask;

	if (on)
		m->sw_value |= mask;
	else
		m->sw_value &= ~mask;
	m->sw_mask |= mask;
	if (m->sw_value != old_value || m->sw_mask != old_mask)
		miner_send_sw(m);
}


static void sw_miner_add(struct miner *m, const char *topic, uint32_t mask)
{
	struct sw_miner *sw = alloc_type(struct sw_miner);

	sw->topic = sw_listen(topic);
	sw->mask = mask;
	sw->next = m->sw;
	m->sw = sw;
	sw_set_miner(m, mask, sw->topic->on);
}


void sw_miner_reset(struct miner *m)
{
	m->sw_value = m->sw_mask = 0;
	m->sw_refresh_s = 0;	/* disable refresh until configured */
	m->sw_last_sent = 0;
	while (m->sw) {
		struct sw_miner *next = m->sw->next;

		free(m->sw);
		m->sw = next;
	}
}


static bool uint32_value(const struct var *var, uint32_t *res)
{
	const struct value *v = var->value;

	if (!v->num) {
		errorf("%s: value '%s' is not a number", var->name, v->s);
		return 0;
	}
	*res = v->n;
	return 1;
}


static bool opt_uint32_var(const struct var *vars, const char *name,
    uint32_t *res, uint32_t deflt)
{
	const struct var *var = var_get_var(vars, name, NULL);

	if (!var) {
		*res = deflt;
		return 1;
	}
	return uint32_value(var, res);
}


bool sw_miner_setup(struct miner *m, const struct var *vars)
{
	const struct var *var;
	uint32_t mask, tmp;

	sw_miner_reset(m);
	for (var = vars; var; var = var->next)
		if (!strncmp(var->name, "switch", 6) && var->name[6] == '_' &&
		    var->assoc) {
			if (!uint32_value(var, &mask))
				return 0;
			sw_miner_add(m, var->name + 7, mask);
		}
	if (!opt_uint32_var(vars, "switch_refresh", &tmp, DEFAULT_SW_REFRESH_S))
		return 0;
	m->sw_refresh_s = tmp;
	return 1;
}


/* ----- Process switch message -------------------------------------------- */


void sw_set(const char *topic, bool on)
{
	struct sw_topic *t;
	struct miner *m;
	const struct sw_miner *sw;

	for (t = topics; t; t = t->next)
		if (!strcmp(t->topic, topic))
			break;
	/* message was not a switch actuation */
	if (!t)
		return;

	t->on = on;
	for (m = miners; m; m = m->next) {
		for (sw = m->sw; sw; sw = sw->next)
			if (sw->topic == t)
				break;
		if (sw)
			sw_set_miner(m, sw->mask, on);
	}
}

/*
 * miner.h - Miner state tracking
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef MINER_H
#define	MINER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <mosquitto.h>

#include "fds.h"
#include "validate.h"
#include "exec.h"
#include "config.h"


enum miner_state {
	ms_connecting,	/* connecting to MQTT broker */
	ms_syncing,	/* synchronizing configuration */
	ms_idle,	/* configuration is synchronized, nothing to do */
	ms_shutdown	/* connection is shutting down */
};

struct miner {
	/* miner data from crew */
	uint32_t		id;
	uint32_t		ipv4;
	char			*name;		/* NULL if not yet seen */
	char			*serial[2];	/* NULL if not yet seen */
	time_t			last_seen;

	/* connection */
	enum miner_state	state;
	struct mosquitto	*mosq;
	struct fd		*fd;

	/* miner data from MQTT */
	struct validate		*validate;
	struct config		*config;
	char			*restart;	/* restart-pending */

	/* script result */
	struct delta		*delta;
	char			*error;

	/* update rate limit */
	time_t			cooldown;	/* don't allow next update
						   earlier */

	struct miner		*next;
};

struct miner_env {
	struct exec_env exec;
	const struct miner *miner;
	struct var *cfg_vars;	/* @@@ should not use global variable */
	struct var *vars;	/* @@@ should not use global variable */
	struct delta *delta;
	char *error;
	enum magic_flags flags;
};


extern struct miner *miners;



bool miner_can_calculate(const struct miner *m);
bool miner_calculate(struct miner_env *env, struct miner *m,
    const char *dir, const struct rule *rules);
void miner_calculation_finish(struct miner_env *env, char **error, 
    struct delta **delta);

const char *consider_updating(struct miner *m, bool request, bool restart);

struct miner *miner_by_id(uint32_t id);
struct miner *miner_by_ipv4(uint32_t ipv4);

void miner_set_name(struct miner *m, const char *name);
void miner_set_serial(struct miner *m,
    const char *serial0, const char *serial1);

void miner_deliver(struct miner *m, const char *topic, const char *payload);

void miner_reset(struct miner *m);
void miner_destroy(struct miner *m);
void miner_destroy_all(void);

struct miner *miner_new(uint32_t id);

#endif /* MINER_H */

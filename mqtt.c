/*
 * mqtt.c - MQTT interface
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define	_GNU_SOURCE	/* for vasprintf */
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <assert.h>

#include <mosquitto.h>

#include "bonanza.h"
#include "alloc.h"
#include "fds.h"
#include "miner.h"
#include "mqtt.h"


/* ----- Mosquitto loop ---------------------------------------------------- */


static short poll_flags(struct miner *m)
{
	return POLLHUP | POLLERR | POLLIN |
	    (mosquitto_want_write(m->mosq) ? POLLOUT : 0);
}


void update_poll(struct miner *m)
{
	fd_modify(m->fd, poll_flags(m));
}


static void mqtt_fd(void *user, int fd, short revents)
{
	struct miner *m = user;
	int res;

	if (revents & POLLIN) {
		res = mosquitto_loop_read(m->mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && verbose)
			fprintf(stderr, IPv4_QUAD_FMT
			    ": warning: mosquitto_loop_read: %s (%d)\n",
			    IPv4_QUAD(m->ipv4), mosquitto_strerror(res), res);
	}
	if (revents & POLLOUT) {
		res = mosquitto_loop_write(m->mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && verbose)
			fprintf(stderr, IPv4_QUAD_FMT
			    ": warning: mosquitto_loop_write: %s (%d)\n",
			    IPv4_QUAD(m->ipv4), mosquitto_strerror(res), res);
	}
}


static void mqtt_miner_idle(struct miner *m)
{
	int res;

	res = mosquitto_loop_misc(m->mosq);
	if (res != MOSQ_ERR_SUCCESS && verbose)
		fprintf(stderr,
		    IPv4_QUAD_FMT ": warning: mosquitto_loop_misc: %s (%d)\n",
		    IPv4_QUAD(m->ipv4), mosquitto_strerror(res), res);
}


/* ----- Publishing -------------------------------------------------------- */


void mqtt_vprintf(struct mosquitto *mosq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, va_list ap)
{
	const struct miner *m = mosquitto_userdata(mosq);
	char *s;
	int res;

	if (vasprintf(&s, fmt, ap) < 0) {
		perror("vasprintf");
		exit(1);
	}
	if (verbose)
		fprintf(stderr,
		    IPv4_QUAD_FMT ": MQTT \"%s\" -> \"%s\"\n",
		    IPv4_QUAD(m->ipv4), topic, s);
	res = mosquitto_publish(mosq, NULL, topic, strlen(s), s, qos,
	    retain);
	if (res != MOSQ_ERR_SUCCESS)
		fprintf(stderr,
		    IPv4_QUAD_FMT
		    ": warning: mosquitto_publish (%s): %s (%d)\n",
		    IPv4_QUAD(m->ipv4), topic, mosquitto_strerror(res), res);
	free(s);
}


void mqtt_printf(struct mosquitto *mosq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, ...)
{
	va_list ap;

	assert(!strchr(topic, '%'));
	va_start(ap, fmt);
	mqtt_vprintf(mosq, topic, qos, retain, fmt, ap);
	va_end(ap);
}


/* ----- Subscriptions and reception --------------------------------------- */


static void message(struct mosquitto *mosq, void *user,
    const struct mosquitto_message *msg)
{
	struct miner *m = user;
	char *buf;

	if (m->state == ms_shutdown)
		return;
	if (verbose > 1)
		fprintf(stderr, IPv4_QUAD_FMT ": MQTT \"%s\": \"%.*s\"\n",
		    IPv4_QUAD(m->ipv4), msg->topic, msg->payloadlen,
		    (const char *) msg->payload);

	buf = alloc_size(msg->payloadlen + 1);
	memcpy(buf, msg->payload, msg->payloadlen);
	buf[msg->payloadlen] = 0;

	miner_deliver(m, msg->topic, buf);
	update_poll(m);
	free(buf);
}


static void subscribe_one(struct miner *m, const char *topic, int qos)
{
	int res;

	res = mosquitto_subscribe(m->mosq, NULL, topic, qos);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": mosquitto_subscribe %s: %s (%d)\n",
		    IPv4_QUAD(m->ipv4), topic, mosquitto_strerror(res), res);
		exit(1);
	}
	update_poll(m);
}


/* ----- Connect and disconnect -------------------------------------------- */


static void connected(struct mosquitto *mosq, void *data, int result)
{
	struct miner *m = data;

	if (m->state == ms_shutdown)
		return;
	if (result) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": MQTT connect failed: %s (%d)\n",
		    IPv4_QUAD(m->ipv4), mosquitto_strerror(result), result);
		exit(1);
	}
	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT ": MQTT connected\n",
		    IPv4_QUAD(m->ipv4));
	assert(m->state == ms_connecting);
	m->state = ms_syncing;
	subscribe_one(m, "/config/+", 1);
	update_poll(m);
}


static void disconnected(struct mosquitto *mosq, void *data, int result)
{
	struct miner *m = data;
	int res;

	if (m->state == ms_shutdown)
		return;
	miner_reset(m);

	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT
		    ": warning: reconnecting MQTT (disconnect reason %s, %d)\n",
		    IPv4_QUAD(m->ipv4), mosquitto_strerror(result), result);
	if (result == MOSQ_ERR_KEEPALIVE) {
		/*
		 * There is a failure pattern that begins with a disconnect due
		 * to acommon error, like MOSQ_ERR_CONN_LOST, the reconnect
		 * seems to succeed, but then we get immediately disconnected
		 * with MOSQ_ERR_KEEPALIVE, try to reconnect, etc. This repeats
		 * forever.
		 *
		 * We break this loop by removing the miner entry, and letting
		 * the crew re-create it from scratch (if it's still there).
		 */
		m->state = ms_shutdown;
		return;
	}
	res = mosquitto_reconnect(mosq);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": mosquitto_reconnect: %s (%d)\n",
		    IPv4_QUAD(m->ipv4), mosquitto_strerror(res), res);
		m->state = ms_shutdown;
		return;
	}
	update_poll(m);
}


/* ----- Miner-level interface --------------------------------------------- */


void miner_ipv4(uint32_t id, uint32_t ipv4)
{
	struct mosquitto *mosq;
	struct miner *m;
	char buf[16]; /* 4 * 3 + 3 + 1 */
	int res;

	m = miner_by_id(id);
	if (!m) {
		fprintf(stderr, "miner 0x%x not found\n", id);
		return;
	}
	if (m->ipv4)
		return;

	m->ipv4 = ipv4;
	mosq = mosquitto_new(NULL, 1, m);
	if (!mosq) {
		fprintf(stderr, "mosquitto_new failed\n");
		exit(1);
	}

	mosquitto_connect_callback_set(mosq, connected);
	mosquitto_disconnect_callback_set(mosq, disconnected);
	mosquitto_message_callback_set(mosq, message);
//	mosquitto_publish_callback_set(mosq, published);

	sprintf(buf, IPv4_QUAD_FMT, IPv4_QUAD(m->ipv4));
	res = mosquitto_connect(mosq, buf, MQTT_DEFAULT_PORT,
	    MQTT_KEEPALIVE_S);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, IPv4_QUAD_FMT ": mosquitto_connect: %s (%d)\n",
		    IPv4_QUAD(m->ipv4), mosquitto_strerror(res), res);
		mosquitto_destroy(mosq);
		m->ipv4 = 0;
		return;
	}
	m->mosq = mosq;
	m->fd = fd_add(mosquitto_socket(mosq), poll_flags(m), mqtt_fd, m);
}


void miner_seen(uint32_t id)
{
	struct miner *m = miner_by_id(id);

	if (m) {
		if (verbose > 3)
			fprintf(stderr, "id %x (update)\n", id);
		m->last_seen = now;
	} else {
		if (verbose > 1)
			fprintf(stderr, "id %x (new)\n", id);
		miner_new(id);
	}
}


void mqtt_idle(void)
{
	struct miner **anchor = &miners;
	struct miner *m;

	while (*anchor) {
		m = *anchor;
		if (m->state == ms_shutdown) {
			*anchor = m->next;
			miner_destroy(m);
			continue;
		}
		if (m->mosq) {
			mqtt_miner_idle(m);
			update_poll(m);
		}
		if (*anchor == m)
			anchor = &(*anchor)->next;
	}
}


void mqtt_init(void)
{
	mosquitto_lib_init();
}

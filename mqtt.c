/*
 * mqtt.c - MQTT interface
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
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
#include "sw.h"
#include "miner.h"
#include "mqtt.h"


static struct mqtt_session broker_mqtt;
static bool broker_connected = 0;


/* ----- Mosquitto loop ---------------------------------------------------- */


static short poll_flags(struct mosquitto *mosq)
{
	return POLLHUP | POLLERR | POLLIN |
	    (mosquitto_want_write(mosq) ? POLLOUT : 0);
}


void update_poll(const struct mqtt_session *mq)
{
	fd_modify(mq->fd, poll_flags(mq->mosq));
}


static void mqtt_fd(void *user, int fd, short revents)
{
	struct mqtt_session *mq = user;
	int res;

	if (revents & POLLIN) {
		res = mosquitto_loop_read(mq->mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && verbose)
			fprintf(stderr, IPv4_QUAD_FMT
			    ": warning: mosquitto_loop_read: %s (%d)\n",
			    IPv4_QUAD(mq->ipv4), mosquitto_strerror(res), res);
	}
	if (revents & POLLOUT) {
		res = mosquitto_loop_write(mq->mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && verbose)
			fprintf(stderr, IPv4_QUAD_FMT
			    ": warning: mosquitto_loop_write: %s (%d)\n",
			    IPv4_QUAD(mq->ipv4), mosquitto_strerror(res), res);
	}
}


static void mqtt_session_idle(struct mqtt_session *mq)
{
	int res;

	res = mosquitto_loop_misc(mq->mosq);
	if (res != MOSQ_ERR_SUCCESS && verbose)
		fprintf(stderr,
		    IPv4_QUAD_FMT ": warning: mosquitto_loop_misc: %s (%d)\n",
		    IPv4_QUAD(mq->ipv4), mosquitto_strerror(res), res);
}


/* ----- Publishing -------------------------------------------------------- */


void mqtt_vprintf(struct mqtt_session *mq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, va_list ap)
{
	char *s;
	int res;

	if (vasprintf(&s, fmt, ap) < 0) {
		perror("vasprintf");
		exit(1);
	}
	if (verbose)
		fprintf(stderr,
		    IPv4_QUAD_FMT ": MQTT \"%s\" -> \"%s\"\n",
		    IPv4_QUAD(mq->ipv4), topic, s);
	res = mosquitto_publish(mq->mosq, NULL, topic, strlen(s), s, qos,
	    retain);
	if (res != MOSQ_ERR_SUCCESS)
		fprintf(stderr,
		    IPv4_QUAD_FMT
		    ": warning: mosquitto_publish (%s): %s (%d)\n",
		    IPv4_QUAD(mq->ipv4), topic, mosquitto_strerror(res), res);
	free(s);
}


void mqtt_printf(struct mqtt_session *mq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, ...)
{
	va_list ap;

	assert(!strchr(topic, '%'));
	va_start(ap, fmt);
	mqtt_vprintf(mq, topic, qos, retain, fmt, ap);
	va_end(ap);
}


/* ----- Subscriptions and reception --------------------------------------- */


static void message(struct mqtt_session *mq, void *user,
    const struct mosquitto_message *msg,
    void (*deliver)(void *user, const char *topic, const char *payload))
{
	char *buf;

	if (verbose > 1)
		fprintf(stderr, IPv4_QUAD_FMT ": MQTT \"%s\": \"%.*s\"\n",
		    IPv4_QUAD(mq->ipv4), msg->topic, msg->payloadlen,
		    (const char *) msg->payload);

	buf = alloc_size(msg->payloadlen + 1);
	memcpy(buf, msg->payload, msg->payloadlen);
	buf[msg->payloadlen] = 0;

	deliver(user, msg->topic, buf);
	update_poll(mq);
	free(buf);
}


static void miner_message(struct mosquitto *mosq, void *user,
    const struct mosquitto_message *msg)
{
	struct miner *m = user;

	if (m->state == ms_shutdown)
		return;
	message(&m->mqtt, user, msg, miner_deliver);
}


static void subscribe_one(struct mqtt_session *mq, const char *topic, int qos)
{
	int res;

	res = mosquitto_subscribe(mq->mosq, NULL, topic, qos);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": mosquitto_subscribe %s: %s (%d)\n",
		    IPv4_QUAD(mq->ipv4), topic, mosquitto_strerror(res), res);
		exit(1);
	}
	update_poll(mq);
}


/* ----- Ops switch -------------------------------------------------------- */


void miner_send_sw(struct miner *m)
{
	if (m->state == ms_shutdown || m->state == ms_connecting)
		return;
	mqtt_printf(&m->mqtt, "/power/on/ops-set", qos_ack, 1, "0x%x 0x%x",
	    (unsigned) m->sw_value, (unsigned) m->sw_mask);
	m->sw_last_sent = now;
}


/* ----- Connect and disconnect -------------------------------------------- */


static void miner_connected(struct mosquitto *mosq, void *data, int result)
{
	struct miner *m = data;

	if (m->state == ms_shutdown)
		return;
	if (result) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": MQTT connect failed: %s (%d)\n",
		    IPv4_QUAD(m->mqtt.ipv4), mosquitto_strerror(result),
		    result);
		exit(1);
	}
	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT ": MQTT connected\n",
		    IPv4_QUAD(m->mqtt.ipv4));
	assert(m->state == ms_connecting);
	m->state = ms_syncing;
	subscribe_one(&m->mqtt, "/config/+", 1);
	miner_send_sw(m);
	update_poll(&m->mqtt);
}


static void miner_disconnected(struct mosquitto *mosq, void *data, int result)
{
	struct miner *m = data;
	int res;

	if (m->state == ms_shutdown)
		return;
	miner_reset(m);	/* ms_connecting */

	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT
		    ": warning: reconnecting MQTT (disconnect reason %s, %d)\n",
		    IPv4_QUAD(m->mqtt.ipv4), mosquitto_strerror(result),
		    result);
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
		    IPv4_QUAD(m->mqtt.ipv4), mosquitto_strerror(res), res);
		m->state = ms_shutdown;
		return;
	}
	update_poll(&m->mqtt);
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
	if (m->mqtt.ipv4)
		return;

	m->mqtt.ipv4 = ipv4;
	mosq = mosquitto_new(NULL, 1, m);
	if (!mosq) {
		fprintf(stderr, "mosquitto_new failed\n");
		exit(1);
	}

	mosquitto_connect_callback_set(mosq, miner_connected);
	mosquitto_disconnect_callback_set(mosq, miner_disconnected);
	mosquitto_message_callback_set(mosq, miner_message);
//	mosquitto_publish_callback_set(mosq, published);

	sprintf(buf, IPv4_QUAD_FMT, IPv4_QUAD(m->mqtt.ipv4));
	res = mosquitto_connect(mosq, buf, MQTT_DEFAULT_PORT,
	    MQTT_KEEPALIVE_S);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, IPv4_QUAD_FMT ": mosquitto_connect: %s (%d)\n",
		    IPv4_QUAD(m->mqtt.ipv4), mosquitto_strerror(res), res);
		mosquitto_destroy(mosq);
		m->mqtt.ipv4 = 0;
		return;
	}
	m->mqtt.mosq = mosq;
	m->mqtt.fd =
	    fd_add(mosquitto_socket(mosq), poll_flags(mosq), mqtt_fd, &m->mqtt);
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


/* ----- Broker message ---------------------------------------------------- */


static void broker_deliver(void *user, const char *topic,
    const char *payload)
{
	if (strcmp(payload, "0") && strcmp(payload, "1")) {
		fprintf(stderr, "%s: value '%s' is neither 0 nor 1\n",
		    topic, payload);
		return;
	}
	sw_set(topic, !strcmp(payload, "1"));
}


static void broker_message(struct mosquitto *mosq, void *user,
    const struct mosquitto_message *msg)
{
	if (broker_connected)
		message(&broker_mqtt, NULL, msg, broker_deliver);
}


void broker_subscribe(const char *topic)
{
	if (broker_connected)
		subscribe_one(&broker_mqtt, topic, qos_ack);
}


/* ----- Broker connect and disconnect ------------------------------------- */


static void broker_connect(struct mosquitto *mosq, void *data, int result)
{
	if (result) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": MQTT connect failed: %s (%d)\n",
		    IPv4_QUAD(broker_mqtt.ipv4), mosquitto_strerror(result),
		    result);
		exit(1);
	}
	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT ": MQTT connected\n",
		    IPv4_QUAD(broker_mqtt.ipv4));
	broker_connected = 1;
	sw_subscribe();
	update_poll(&broker_mqtt);
}


static void broker_disconnect(struct mosquitto *mosq, void *data, int result)
{
	struct miner *m = data;
	int res;

	broker_connected = 0;
	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT
		    ": warning: reconnecting MQTT (disconnect reason %s, %d)\n",
		    IPv4_QUAD(broker_mqtt.ipv4), mosquitto_strerror(result),
		    result);
	if (result == MOSQ_ERR_KEEPALIVE) {
		/*
		 * There is a failure pattern that begins with a disconnect due
		 * to acommon error, like MOSQ_ERR_CONN_LOST, the reconnect
		 * seems to succeed, but then we get immediately disconnected
		 * with MOSQ_ERR_KEEPALIVE, try to reconnect, etc. This repeats
		 * forever.
		 */
		return;
	}
	res = mosquitto_reconnect(mosq);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr,
		    IPv4_QUAD_FMT ": mosquitto_reconnect: %s (%d)\n",
		    IPv4_QUAD(m->mqtt.ipv4), mosquitto_strerror(res), res);
		/* @@@ should try later */
		return;
	}
	update_poll(&broker_mqtt);
}


static void setup_broker(const char *broker)
{
	struct mosquitto *mosq;
	char *host = NULL;
	const char *colon;
	char *end;
	int port = MQTT_DEFAULT_PORT;
	int res;

	colon = strchr(broker, ':');
	if (colon) {
		port = strtoul(colon + 1, &end, 0);
		if (*end) {
			fprintf(stderr, "invalid port \"%s\"\n", colon + 1);
			exit(1);
		}
	}
	host = strndup(broker,
	    colon ? (size_t) (colon - broker) : strlen(broker));
	if (!host) {
		perror("strndup");
		exit(1);
	}

	mosq = mosquitto_new(NULL, 1, NULL);
	if (!mosq) {
		fprintf(stderr, "mosquitto_new failed\n");
		exit(1);
	}

	mosquitto_connect_callback_set(mosq, broker_connect);
	mosquitto_disconnect_callback_set(mosq, broker_disconnect);
	mosquitto_message_callback_set(mosq, broker_message);

	if (verbose)
		fprintf(stderr, "connecting to MQTT broker (%s:%u)\n",
		    host, port);
	res = mosquitto_connect(mosq, host, port, 3600);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "mosquitto_connect: %s\n",
		    mosquitto_strerror(res));
		exit(1);
	}
	free(host);

	broker_mqtt.mosq = mosq;
	broker_mqtt.ipv4 = 0; /* @@@ */
	broker_mqtt.fd =
	    fd_add(mosquitto_socket(mosq), poll_flags(mosq), mqtt_fd,
	    &broker_mqtt);
}


/* ----- Idle and initialization ------------------------------------------- */


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
		if (m->mqtt.mosq) {
			mqtt_session_idle(&m->mqtt);
			update_poll(&m->mqtt);
		}
		if (*anchor != m)
			continue;
		anchor = &(*anchor)->next;
		if (m->sw_refresh_s && m->sw_last_sent + m->sw_refresh_s < now)
			miner_send_sw(m);
	}
}


void mqtt_init(const char *broker)
{
	mosquitto_lib_init();
	if (broker)
		setup_broker(broker);
}

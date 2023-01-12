/*
 * mqtt.h - MQTT interface
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef MQTT_H
#define	MQTT_H


#include <stdbool.h>
#include <stdint.h>

#include <mosquitto.h>

#include "fds.h"


#define	MQTT_DEFAULT_PORT	1883
#define	MQTT_KEEPALIVE_S	600


struct miner;

enum mqtt_qos {
	qos_be		= 0,
	qos_ack		= 1,
	qos_once	= 2
};

struct mqtt_session {
	struct mosquitto *mosq;
	struct fd *fd;
	uint32_t ipv4;
};


void mqtt_vprintf(struct mqtt_session *mq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, va_list ap);
void mqtt_printf(struct mqtt_session *mq, const char *topic, enum mqtt_qos qos,
    bool retain, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

void miner_send_sw(struct miner *m);

void miner_ipv4(uint32_t id, uint32_t ipv4);
void miner_seen(uint32_t id);

void update_poll(const struct mqtt_session *mq);

void mqtt_idle(void);
void mqtt_init(void);

#endif /* !MQTT_H */

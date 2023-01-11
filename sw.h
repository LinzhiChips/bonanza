/*
 * sw.h - Operations switch control
 *
 * Copyright (C) 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef SW_H
#define	SW_H

#include <stdbool.h>
#include <stdint.h>


struct miner;
struct sw_topic;
struct exec_env;


const struct sw_topic *sw_listen(const char *topic);
void sw_cleanup(void);

void sw_miner_add(struct miner *m, const char *topic, uint32_t mask);
void sw_miner_reset(struct miner *m);
bool sw_miner_setup(struct miner *m, const struct var *vars);

/* from MQTT */
void sw_set(const char *topic, bool on);

#endif /* !SW_H */

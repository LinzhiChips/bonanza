/*
 * api.h - HTTP/JSON API functions
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef API_H
#define	API_H

#include <stdbool.h>


struct miner;


extern bool auto_update;
extern bool auto_restart;


char *miners_json(void);
char *miner_json(uint32_t id);

char *get_path(bool test);

char *miner_update_all(bool restart);
char *miner_update_group(const char *hash, bool restart);
char *miner_update(uint32_t id);
char *miner_update_restart(uint32_t id);

char *miner_run(uint32_t id);

char *miner_reload(void);

#endif /* MINER_H */

/*
 * crew.h - Crew listener
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef CREW_H
#define CREW_H

#include <stdint.h>


#define	DEFAULT_MC_ADDR		"239.255.49.44"
#define	DEFAULT_CREW_PORT	12588


void crew_enable_multicast(const char *addr);
void crew_init(uint16_t port);

#endif /*! CREW_H */

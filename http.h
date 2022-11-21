/*
 * http.h - HTTP server
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef HTTP_H
#define	HTTP_H

#include <stdbool.h>
#include <stdint.h>


#define	DEFAULT_HTTP_PORT	8003


void http_init(bool local, uint16_t port);

#endif /* !HTTP_H */

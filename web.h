/*
 * web.h - Parser and dispatcher for GET and POST requests
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef WEB_H
#define	WEB_H

char *web_get(const char *uri, const char *version, unsigned *len);
char *web_post(const char *uri, const char *version, const char *req,
    unsigned *len);

#endif /* !WEB_H */

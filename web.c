/*
 * web.c - Parser and dispatcher for GET and POST requests
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "bonanza.h"
#include "alloc.h"
#include "validate.h"
#include "api.h"
#include "web.h"


#define	DEFAULT_HTTP_VERSION	"HTTP/1.0"

#define	CONTENT_TYPE_JSON	"application/json"
#define	CONTENT_TYPE_HTML	"text/html"
#define	CONTENT_TYPE_JS		"text/javascript"
#define	CONTENT_TYPE_CSS	"text/css"
#define	CONTENT_TYPE_ICON	"image/x-icon"
#define	CONTENT_TYPE_PLAIN	"text/plain"


/* ----- Reponse ---------------------------------------------------------- */


static char *response_size(const char *version, unsigned status,
    const char *response, const char *content_type, const char *content,
    size_t size, unsigned *len)
{
	char *buf;
	int res;

	res = asprintf_req(&buf, "%s %u %s\r\nContent-Type: %s\r\n"
	    "Content-Length: %u\r\n\r\n",
	    version ? version : DEFAULT_HTTP_VERSION, status, response,
	    content_type, (unsigned) size);

	buf = realloc_size(buf, res + size + 2 + 1);
	memcpy(buf + res, content, size);
	strcpy(buf + res + size, "\r\n");
	*len = res + size + 2;

	return buf;
}


static char *response(const char *version, unsigned status,
    const char *response, const char *content_type, const char *content,
    unsigned *len)
{
	return response_size(version, status, response, content_type,
	    content, strlen(content), len);
}


static char *read_file(FILE *file, size_t *size)
{
	struct stat st;
	size_t got;
	char *s;

	if (fstat(fileno(file), &st) < 0) {
		perror("fstat");
		return NULL;
	}
	*size = st.st_size;
	s = alloc_size(*size + 1);
	got = fread(s, 1, *size, file);
	if (got != *size) {
		fprintf(stderr, "bad fread (%u != %u)\n",
		    (unsigned) *size, (unsigned) got);
		free(s);
		return NULL;
	}
	s[got] = 0;
	return s;
}


/* ----- GET --------------------------------------------------------------- */


static bool valid_path(const char *path)
{
	if (!strncmp(path, "ui/", 3) && valid_file_name(path + 3))
		return 1;
	if (!strncmp(path, "active/", 7) && valid_file_name(path + 7))
		return 1;
	if (!strncmp(path, "test/", 5) && valid_file_name(path + 5))
		return 1;
	return valid_file_name(path);
}


static char *consider_file(const char *path, const char *version, unsigned *len)
{
	const char *p;
	const char *type;
	char *s, *res;
	size_t size;

	if (*path == '/')
		return NULL;

	if (!*path || !strcmp(path, "index.html"))
		path = "ui/index.html";
	else if (!strcmp(path, "favicon.ico"))
		path = "ui/favicon.ico";
	else if (!valid_path(path)) {
		fprintf(stderr, "invalid file name \"%s\"\n", path);
		return NULL;
	}
#if 0
	bool dot = 0;

	for (p = path; *p; p++) {
		if (*p == '.') {
			if (dot)
				return NULL;
			dot = 1;
		} else {
			dot = 0;
		}
	}
#endif

	p = strrchr(path, '.');
	if (!p)
		return NULL;

	if (!strcmp(p, ".html"))
		type = CONTENT_TYPE_HTML;
	else if (!strcmp(p, ".js"))
		type = CONTENT_TYPE_JS;
	else if (!strcmp(p, ".css"))
		type = CONTENT_TYPE_CSS;
	else if (!strcmp(p, ".ico"))
		type = CONTENT_TYPE_ICON;
	else
		type = CONTENT_TYPE_PLAIN;

	FILE *file = fopen(path, "rb");

	if (!file)
		return NULL;
	s = read_file(file, &size);
	(void) fclose(file);

	res = response_size(version, 202, "OK", type, s, size, len);
	free(s);
	return res;
}


static char *run_with_id(const char *s, char *(*fn)(uint32_t id))
{
	unsigned id;

	if (sscanf(s, "id=0x%x", &id) == 1 ||
	    sscanf(s, "id=%u", &id) == 1 ||
	    sscanf(s, "id=%x", &id) == 1)
		return fn(id);
	return NULL;
}


char *web_get(const char *uri, const char *version, unsigned *len)
{
	char *s = NULL;
	char *res;

	if (verbose)
		fprintf(stderr, "GET %s\n", uri);

	if (!strcmp(uri, "/miners")) {
		s = miners_json();
	} else if (!strncmp(uri, "/miner?", 7)) {
		s = run_with_id(uri + 7, miner_json);
	} else if (!strcmp(uri, "/path?type=test")) {
		s = get_path(1);
	} else if (!strcmp(uri, "/path?type=active")) {
		s = get_path(0);
	} else {
		res = consider_file(*uri == '/' ? uri + 1 : uri, version, len);
		if (res)
			return res;
	}

	if (s)
		res = response(version, 202, "OK", CONTENT_TYPE_JSON, s, len);
	else
		res = response(version, 404, "Not Found", CONTENT_TYPE_HTML,
		    "", len);
	free(s);
	return res;
}


/* ----- POST -------------------------------------------------------------- */


char *web_post(const char *uri, const char *version, const char *body,
    unsigned *len)
{
	char *s = NULL;
	char *res;

	if (verbose)
		fprintf(stderr, "POST %s (%s)\n", uri, body);

	if (!strcmp(uri, "/update")) {
		bool restart;

		s = strchr(body, '&');
		if (!s)
			s = strchr(body, 0);
		restart = !strcmp(s, "&restart");
		if (s - body == 3 && !strncmp(body, "all", 3))
			s = miner_update_all(restart);
		else if (!strncmp(body, "group=", 6)) {
			char *tmp = strnalloc(body + 6, s - body - 6);

			s = miner_update_group(tmp, restart);
			free(tmp);
		} else {
			s = run_with_id(body, restart ?  miner_update_restart :
			    miner_update);
		}
	} else if (!strcmp(uri, "/run")) {
		s = run_with_id(body, miner_run);
	} else if (!strcmp(uri, "/reload")) {
		s = miner_reload();
	}

	if (s)
		res = response(version, 202, "OK", CONTENT_TYPE_JSON, s, len);
	else
		res = response(version, 404, "Not Found", CONTENT_TYPE_HTML,
		    "", len);
	free(s);
	return res;
}

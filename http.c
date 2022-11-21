/*
 * http.c - HTTP server
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>

#include "lode.h"
#include "alloc.h"
#include "fds.h"
#include "web.h"
#include "http.h"


#define	BUF_SIZE	1024


struct session {
	int		sock;
	uint32_t	ipv4;
	bool		rx;
	char		*req;
	unsigned	req_len;
	char		*resp;
	unsigned	resp_len;
	int		nl;
	unsigned	resp_pos;
	struct fd	*fd;
};


static void disconnect(struct session *sess, bool failed)
{
	free(sess->req);
	free(sess->resp);
	fd_del(sess->fd);
	if (failed && shutdown(sess->sock, SHUT_RDWR) < 0)
		perror("shutdown");
	if (close(sess->sock) < 0)
		perror("close");
	free(sess);
}


static bool response(struct session *sess)
{
	const char *p;
	int size;
	ssize_t wrote;

	p = sess->resp + sess->resp_pos;
	size = sess->resp_len - sess->resp_pos;

	wrote = write(sess->sock, p, size);
	if (wrote < 0) {
		perror("wrote");
		return 0;
	}
	if (!wrote) {
		fprintf(stderr, "short write");
		return 0;
	}
	sess->resp_pos += wrote;
	if (wrote == size)
		disconnect(sess, 0);
	return 1;
}


static bool request(struct session *sess)
{
	char buf[BUF_SIZE];
	ssize_t got;
	char *p;

	got = read(sess->sock, buf, sizeof(buf));
	if (got < 0) {
		perror("read");
		return 0;
	}
	if (!got) {
		fprintf(stderr, IPv4_QUAD_FMT ": short read\n",
		    IPv4_QUAD(sess->ipv4));
		return 0;
	}

	sess->req = realloc_size(sess->req, sess->req_len + got + 1);
	memcpy(sess->req + sess->req_len, buf, got);
	sess->req_len += got;
	sess->req[sess->req_len] = 0;

	for (p = buf; p != buf + got; p++) {
		if (*p == '\n') {
			if (sess->nl++)
				break;
		} else {
			if (*p != '\r')
				sess->nl = 0;
		}
	}

	if (sess->nl != 2)
		return 1;

	p = sess->req + sess->req_len - got + (p - buf);

	/* parse request */

	char *uri, *version, *end;
	bool get;

	uri = strchr(sess->req, ' ');
	if (!uri) {
		fprintf(stderr, "no URI in request \"%s\"\n", sess->req);
		return 0;
	}
	*uri++ = 0;
	if (!strcmp(sess->req, "GET")) {
		get = 1;
	} else if (!strcmp(sess->req, "POST")) {
		get = 0;
	} else {
		fprintf(stderr, "\"%s\" is neither GET not POST\n", sess->req);
		return 0;
	}
	version = uri + strcspn(uri, " \t\r\n");
	if (strchr("\r\n", *version)) {
		*version = 0;
		end = version + 1;
		version = NULL;
	} else {
		*version++ = 0;
		end = version;
		strsep(&end, " \t\r\n");
		*end = 0;
	}

	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT ": %s %s\n",
		    IPv4_QUAD(sess->ipv4), get ? "GET" : "POST", uri);

	sess->resp = get ? web_get(uri, version, &sess->resp_len) :
	    web_post(uri, version, p + 1, &sess->resp_len);
	if (!sess->resp)
		return 0;
	sess->rx = 0;
	fd_modify(sess->fd, POLLHUP | POLLERR | POLLOUT);
	return 1;
}


static void http_fd(void *user, int fd, short revents)
{
	struct session *sess = user;

	if (revents & (POLLHUP | POLLERR)) {
		disconnect(sess, 1);
		return;
	}
	if (sess->rx) {
		if (!request(sess))
			disconnect(sess, 1);
	} else {
		if (revents & POLLOUT) {
			if (!response(sess))
				disconnect(sess, 1);
			return;
		}
		/* must be POLLIN, which can't be good news if we're here */
		disconnect(sess, 1);
	}
}


static void http_accept(void *user, int fd, short revents)
{
	struct session *sess;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int sock;

	(void) user;
	assert(revents & POLLIN);
	sock = accept(fd, (struct sockaddr *) &addr, &addrlen); if (sock < 0) {
		perror("accept");
		return;
	}

	sess = alloc_type(struct session);
	sess->sock = sock;
	sess->ipv4 = ntohl(addr.sin_addr.s_addr);
	sess->rx = 1;
	sess->req = NULL;
	sess->req_len = 0;
	sess->nl = 0;
	sess->resp = NULL;
	sess->resp_pos = 0;
	sess->fd = fd_add(sock, POLLHUP | POLLERR | POLLIN, http_fd, sess);

	if (verbose)
		fprintf(stderr, IPv4_QUAD_FMT ": new connection\n",
		    IPv4_QUAD(sess->ipv4));
}


void http_init(bool local, uint16_t port)
{
	struct sockaddr_in addr = {
		.sin_family		= AF_INET,
		.sin_addr.s_addr	= htonl(local ?
					  INADDR_LOOPBACK : INADDR_ANY),
		.sin_port		= htons(port)
	};
	int opt = 1;
	int sock;

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &opt,
	    sizeof(opt)) < 0) {
		perror("setsockopt SO_REUSEADDR");
		exit(1);
	}
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(sock, 5) < 0) {
		perror("listen");
		exit(1);
	}
	fd_add(sock, POLLIN, http_accept, NULL);
}

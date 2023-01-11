/*
 * fds.c - File descriptor polling and dispatch
 *
 * Copyright (C) 2022, 2023 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdbool.h>
#include <stdlib.h>
#include <poll.h>
#include <time.h>
#include <assert.h>

#include "alloc.h"
#include "fds.h"


struct fd {
	int fd;
	short events;
	void (*cb)(void *user, int fd, short revents);
	void *user;
	struct fd *next;
};


time_t now;


static struct fd *fds = NULL;
static unsigned n_fds = 0;
static struct pollfd *pollfds = NULL;
static unsigned n_pollfds;
static bool need_update = 1;


static void update_pollfd(void)
{
	const struct fd *f;
	unsigned i;

	pollfds = realloc_type_n(pollfds, n_fds);
	n_pollfds = 0;
	for (f = fds; f; f = f->next) {
		for (i = 0; i != n_pollfds; i++)
			if (pollfds[i].fd == f->fd)
				break;
		if (i == n_pollfds) {
			pollfds[n_pollfds].fd = f->fd;
			pollfds[n_pollfds].events = f->events;
			n_pollfds++;
		} else {
			pollfds[i].events |= f->events;
		}
	}
}


struct fd *fd_add(int fd, short events,
    void (*cb)(void *user, int fd, short revents), void *user)
{
	struct fd *f;

	f = alloc_type(struct fd);
	f->fd = fd;
	f->events = events;
	f->cb = cb;
	f->user = user;
	f->next = fds;
	fds = f;
	n_fds++;
	need_update = 1;
	return f;
}


void fd_modify(struct fd *f, short events)
{
	int fd = f->fd;
	unsigned i;

	if (!(~f->events & events)) {
		f->events = events;
		return;
	}

	f->events = events;
	for (f = fds; f; f = f->next)
		if (f->fd == fd)
			events |= f->events;
	for (i = 0; i != n_pollfds; i++)
		if (pollfds[i].fd == fd) {
			pollfds[i].events = events;
			break;
		}
}


int fd_del(struct fd *f)
{
	struct fd **anchor;
	int fd;

	assert(n_fds);
	for (anchor = &fds; *anchor; anchor = &(*anchor)->next)
		if (*anchor == f)
			break;
	assert(*anchor);

	fd = f->fd;
	*anchor = f->next;
	free(f);
	n_fds--;
	need_update = 1;

	return fd;
}


bool fd_poll(int timeout_ms)
{
	unsigned i;
	int got;

	if (need_update) {
		update_pollfd();
		need_update = 0;
	}
	got = poll(pollfds, n_pollfds, timeout_ms);
	if (got < 0) {
		perror("poll");
		exit(1);
	}
	if (time(&now) == (time_t) -1)
		perror("time");
	if (!got)
		return 0;
	for (i = 0; i != n_pollfds && got; i++)
		if (pollfds[i].revents) {
			const struct fd *f, *next;

			for (f = fds; f; f = next) {
				next = f->next;
				if (pollfds[i].fd == f->fd &&
				    (pollfds[i].revents & f->events))
					f->cb(f->user, f->fd,
					    pollfds[i].revents);
			}
			got--;
		}
	return 1;
}

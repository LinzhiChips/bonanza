/*
 * fds.h - File descriptor polling and dispatch
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef FDS_H
#define	FDS_H

#include <stdbool.h>
#include <sys//types.h>


struct fd;


extern time_t now;


struct fd *fd_add(int fd, short events,
    void (*cb)(void *user, int fd, short revents), void *user);
void fd_modify(struct fd *f, short events);
int fd_del(struct fd *f);

bool fd_poll(int timeout_ms);

#endif /* !FDS_H */

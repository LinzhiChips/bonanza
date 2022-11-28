/*
 * crew.c - Crew listener
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "alloc.h"
#include "bonanza.h"
#include "fds.h"
#include "mqtt.h"
#include "miner.h"
#include "crew.h"


#define	MAX_MSG_BYTES		450
#define	MSG_HEADER_ALIGNED	((sizeof(struct msg_header) + 3) & ~3)
#define	MSG_ITEM_ALIGNED	((sizeof(struct msg_miner) + 3) & ~3)
#define	MAX_MSG_ITEMS \
    ((MAX_MSG_BYTES - MSG_HEADER_ALIGNED) / MSG_ITEM_ALIGNED)

#define	MY_MAJOR	1

#define	IPv4_PORT_FMT	"%u.%u.%u.%u:%u"
#define	IPv4_ADDR(a)	((const uint8_t *) &(a).sin_addr.s_addr)
#define	IPv4_PORT(a)	IPv4_ADDR(a)[0], IPv4_ADDR(a)[1], IPv4_ADDR(a)[2], \
			IPv4_ADDR(a)[3], ntohs((a).sin_port)

#define	MINER_NAME_LEN	16

struct status_1 {
	char		name[MINER_NAME_LEN];
					/* zero-padded */
};

struct status_2 {
	uint32_t	uptime;		/* system uptime, in seconds */
	uint32_t	sys_time;	/* system "real" time */
	uint32_t	fw_date;	/* firmware build date */
	uint32_t	ipv4;		/* IPv4 address */
};

#define SERIAL_LEN      8

struct status_5 {
	char		serial_0[SERIAL_LEN];	/* hashboard serial number */
	char		serial_1[SERIAL_LEN];	/* zero-padded */
};

/* 24 bytes */

struct msg_header {
	uint64_t	hash;   /* SHA3(PBKDF2(secret) + rest_of_msg) */
	uint64_t	seed;
	uint8_t		major;
	uint8_t		minor;
	uint16_t	reserved_1;
	uint32_t	reserved_2;
} __attribute__((packed, aligned(4)));

/* 24 bytes */

struct msg_miner {
	uint32_t	id;
	uint8_t		page;
	uint8_t		reserved;
	uint16_t	seq;    /* sequence numbers are per page ! */
	union {
		struct status_1	status_1;
		struct status_2	status_2;
		struct status_5	status_5;
	} u;
} __attribute__((packed, aligned(4)));


static int sock = -1;


/* ----- Message processing ------------------------------------------------ */


#define	GET_STRING(var, from, max_len) \
	size_t var##_len = strnlen((from), (max_len)); \
	char var[var##_len + 1]; \
	memcpy(var, (from), var##_len); \
	var[var##_len] = 0


static void process_msg(const uint8_t *buf, unsigned items)
{
	unsigned i;

	for (i = 0; i != items; i++) {
		const struct msg_miner *msg = (const struct msg_miner *)
		    (buf + MSG_HEADER_ALIGNED + i * MSG_ITEM_ALIGNED);
		struct miner *miner;

		miner_seen(msg->id);
		switch (msg->page) {
		case 1:
			miner = miner_by_id(msg->id);
			if (!miner)
				break;
			{
				GET_STRING(name, msg->u.status_1.name,
				    MINER_NAME_LEN);
				miner_set_name(miner, name);
			}
			break;
		case 2:
			if (msg->u.status_2.ipv4)
				miner_ipv4(msg->id, msg->u.status_2.ipv4);
			break;
		case 5:
			miner = miner_by_id(msg->id);
			if (!miner)
				break;
			{
				GET_STRING(serial0, msg->u.status_5.serial_0,
				    SERIAL_LEN);
				GET_STRING(serial1, msg->u.status_5.serial_1,
				    SERIAL_LEN);
				miner_set_serial(miner, serial0, serial1);
			}
			break;
		}
	}
}


/* ----- Message reception ------------------------------------------------- */


static void receive(int fd)
{
	uint8_t buf[MAX_MSG_BYTES];
	struct msg_header *hdr = (void *) buf;
	struct sockaddr_in addr;
	uint8_t ctrl_buf[256];
	struct iovec iov = {
		.iov_base	= buf,
		.iov_len	= MAX_MSG_BYTES,
	};
	struct msghdr mh = {
		.msg_name	= &addr,
		.msg_namelen	= sizeof(addr),
		.msg_iov	= &iov,
		.msg_iovlen	= 1,
		.msg_control	= ctrl_buf,
		.msg_controllen	= sizeof(ctrl_buf),
	};
	ssize_t got;
	unsigned misaligned;

	got = recvmsg(fd, &mh, MSG_DONTWAIT);
	if (got < 0) {
		perror("recvmsg");
		exit(1);
	}
	if (IN_MULTICAST(ntohl(addr.sin_addr.s_addr))) {
		fprintf(stderr, "not accepting FROM a multicast address (%s)\n",
		    inet_ntoa(addr.sin_addr));
		return;
	}
	if (got < (ssize_t) MSG_HEADER_ALIGNED) {
		fprintf(stderr, "message too short (%u < %u)\n",
		    (unsigned) got, (unsigned) MSG_HEADER_ALIGNED);
		return;
	}
	if (mh.msg_flags & MSG_CTRUNC) {
		fprintf(stderr, "control data truncated");
		return;
	}
	if (mh.msg_flags & MSG_TRUNC)
		fprintf(stderr,  "message truncated\n");
	misaligned = (got - MSG_HEADER_ALIGNED) % MSG_ITEM_ALIGNED;
	if (misaligned)
		fprintf(stderr,
		    "bad message length: %u is %u bytes off from %u + N * %u\n",
		    (unsigned) got, misaligned,
		    (unsigned) MSG_HEADER_ALIGNED, (unsigned) MSG_ITEM_ALIGNED);

	if (hdr->major != MY_MAJOR) {
		fprintf(stderr, "incompatible message version %u.%u\n",
		    hdr->major, hdr->minor);
		return;
	}

#ifdef LATER
	if (!auth_check(msg, got)) {
		fprintf(stderr, IPv4_PORT_FMT
		    ": bad authentication seed 0x%016llx hash 0x%016llx",
		    IPv4_PORT(*addr), (unsigned long long) hdr->seed,
		    (unsigned long long) hdr->hash);
		return NO_MSG;
	}
#endif

	if (verbose > 3) {
		struct cmsghdr *cmsg;

		for (cmsg = CMSG_FIRSTHDR(&mh); cmsg;
		    cmsg = CMSG_NXTHDR(&mh, cmsg)) {
			if (cmsg->cmsg_level != IPPROTO_IP ||
			    cmsg->cmsg_type != IP_ORIGDSTADDR)
				continue;

			const struct sockaddr_in *dst =
			    (void *) CMSG_DATA(cmsg);
			const in_addr_t *ina = &dst->sin_addr.s_addr;
			const uint8_t *ip = (const void *) ina;

			fprintf(stderr,
			    "crew: " IPv4_PORT_FMT " -> " IPv4_QUAD_FMT "\n",
			    IPv4_PORT(addr), ip[0], ip[1], ip[2], ip[3]);
			break;
		}
	}

	process_msg(buf, (got - MSG_HEADER_ALIGNED) / MSG_ITEM_ALIGNED);
}


static void crew_rx(void *user, int fd, short revents)
{
	(void) user;
	if (revents & POLLIN)
		receive(fd);
}


/* ----- Setup ------------------------------------------------------------- */


void crew_enable_multicast(const char *addr)
{
	struct in_addr multicast_addr;
	struct ip_mreq mreq;

	if (!addr)
		addr = DEFAULT_MC_ADDR;
	if (!inet_aton(addr, &multicast_addr)) {
		fprintf(stderr, "%s: bad address\n", addr);
		exit(1);
	}
	mreq.imr_multiaddr = multicast_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
	    &mreq, sizeof(mreq)) < 0)
		perror("setsockopt IP_ADD_MEMBERSHIP");
}


void crew_init(uint16_t port)
{
	struct sockaddr_in addr = {
		.sin_family		= AF_INET,
		.sin_addr.s_addr	= htonl(INADDR_ANY),
		.sin_port		= htons(port),
	};
	int opt = 1;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(sock, IPPROTO_IP, IP_RECVORIGDSTADDR, &opt, sizeof(opt))
	    < 0) {
		perror("setsockopt IP_RECVORIGDSTADDR");
		exit(1);
	}
	if (bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}
	fd_add(sock, POLLIN, crew_rx, NULL);
}

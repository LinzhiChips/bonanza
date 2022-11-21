/*
 * hash.c - Wrapper for MD5
 *
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <sys/types.h>
#include <md5.h>

#include "alloc.h"
#include "hash.h"


static MD5_CTX ctx;


void hash_begin(void)
{
	MD5Init(&ctx);
}


void hash_add(const void *data, size_t len)
{
	MD5Update(&ctx, data, len);
}


char *hash_end(void)
{
	char buf[MD5_DIGEST_STRING_LENGTH];

	MD5End(&ctx, buf);
	return stralloc(buf);
}

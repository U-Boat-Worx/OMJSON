/*
 * File: jsonCredential.h
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 *
 * This file is part of OMJSON, licensed under the MIT License.
 */

/* Login credentials: SHA-256, password check and the login request parser.
 * Plain C with no B&R headers, so test/host can build it on a PC. */

#ifndef __JSON_CREDENTIAL_H__
#define __JSON_CREDENTIAL_H__

#include <stddef.h>
#include <stdint.h>

/* Room for the longest password a login request may carry (63 characters) */
#define JSON_CREDENTIAL_PASSWORDSIZE 64

typedef struct jsonSha256_ctx {
	uint32_t state[8];
	uint64_t bitLength;
	uint8_t block[64];
	uint32_t blockLength;
} jsonSha256_ctx;

void jsonSha256Init(jsonSha256_ctx *ctx);
void jsonSha256Update(jsonSha256_ctx *ctx, const uint8_t *data, size_t length);
void jsonSha256Final(jsonSha256_ctx *ctx, uint8_t digest[32]);

/* Zero length bytes at p through a volatile pointer, so the compiler keeps the
 * stores even when p is never read again (a plain memset there may be dropped) */
void jsonWipe(void *p, size_t length);

/* 1 if SHA-256(salt followed by password) equals hexHash (64 hex digits,
 * either case). An empty password or a malformed hash never matches.
 * The digests are compared in constant time. */
int jsonPasswordMatches(const char *salt, const char *password, const char *hexHash);

/* Parse the data of a login request, {"user":"...","password":"..."}.
 * Both strings are required and must be printable ASCII without '"' or
 * '\\' that fits the buffer; anything else, and unknown or repeated keys,
 * fails rather than being cut or skipped. 1 = parsed. */
int jsonParseCredentials(const char *data, char *user, size_t userSize, char *password, size_t passwordSize);

#endif

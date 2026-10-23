/*
 * File: jsonCredential.c
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 *
 * This file is part of OMJSON, licensed under the MIT License.
 */

#include <string.h>
#include "jsonCredential.h"

/************************************************************************/
/* SHA-256 (FIPS 180-4)													*/
/************************************************************************/

#define JSON_SHA_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t jsonSha256K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void jsonSha256Block(jsonSha256_ctx *ctx, const uint8_t *block)
{
	uint32_t w[64];
	uint32_t a, b, c, d, e, f, g, h, t1, t2;
	unsigned int i;

	for (i = 0; i < 16; i++) {
		w[i] = ((uint32_t)block[4 * i] << 24) | ((uint32_t)block[4 * i + 1] << 16)
			| ((uint32_t)block[4 * i + 2] << 8) | (uint32_t)block[4 * i + 3];
	}
	for (i = 16; i < 64; i++) {
		uint32_t s0 = JSON_SHA_ROTR(w[i - 15], 7) ^ JSON_SHA_ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = JSON_SHA_ROTR(w[i - 2], 17) ^ JSON_SHA_ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

	for (i = 0; i < 64; i++) {
		t1 = h + (JSON_SHA_ROTR(e, 6) ^ JSON_SHA_ROTR(e, 11) ^ JSON_SHA_ROTR(e, 25))
			+ ((e & f) ^ (~e & g)) + jsonSha256K[i] + w[i];
		t2 = (JSON_SHA_ROTR(a, 2) ^ JSON_SHA_ROTR(a, 13) ^ JSON_SHA_ROTR(a, 22))
			+ ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void jsonSha256Init(jsonSha256_ctx *ctx)
{
	ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
	ctx->bitLength = 0;
	ctx->blockLength = 0;
}

void jsonSha256Update(jsonSha256_ctx *ctx, const uint8_t *data, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++) {
		ctx->block[ctx->blockLength++] = data[i];
		if (ctx->blockLength == 64) {
			jsonSha256Block(ctx, ctx->block);
			ctx->bitLength += 512;
			ctx->blockLength = 0;
		}
	}
}

void jsonSha256Final(jsonSha256_ctx *ctx, uint8_t digest[32])
{
	uint64_t bits = ctx->bitLength + (uint64_t)ctx->blockLength * 8;
	uint32_t i = ctx->blockLength;

	// Pad with 0x80 and zeros so the length lands in the last 8 bytes of a block
	ctx->block[i++] = 0x80;
	if (i > 56) {
		while (i < 64) ctx->block[i++] = 0;
		jsonSha256Block(ctx, ctx->block);
		i = 0;
	}
	while (i < 56) ctx->block[i++] = 0;
	for (i = 0; i < 8; i++) ctx->block[63 - i] = (uint8_t)(bits >> (8 * i));
	jsonSha256Block(ctx, ctx->block);

	for (i = 0; i < 32; i++) digest[i] = (uint8_t)(ctx->state[i / 4] >> (24 - 8 * (i % 4)));
}

/************************************************************************/
/* Password check														*/
/************************************************************************/

void jsonWipe(void *p, size_t length)
{
	volatile uint8_t *v = (volatile uint8_t*)p;
	while (length--) *v++ = 0;
}

static int jsonHexValue(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

int jsonPasswordMatches(const char *salt, const char *password, const char *hexHash)
{
	jsonSha256_ctx ctx;
	uint8_t digest[32];
	uint8_t expected[32];
	uint8_t diff = 0;
	unsigned int i;

	if (salt == 0 || password == 0 || hexHash == 0 || password[0] == 0) return 0;

	// The stored hash is configuration, not a secret: validating it first may exit early
	if (strlen(hexHash) != 64) return 0;
	for (i = 0; i < 32; i++) {
		int hi = jsonHexValue(hexHash[2 * i]);
		int lo = jsonHexValue(hexHash[2 * i + 1]);
		if (hi < 0 || lo < 0) return 0;
		expected[i] = (uint8_t)((hi << 4) | lo);
	}

	jsonSha256Init(&ctx);
	jsonSha256Update(&ctx, (const uint8_t*)salt, strlen(salt));
	jsonSha256Update(&ctx, (const uint8_t*)password, strlen(password));
	jsonSha256Final(&ctx, digest);

	for (i = 0; i < 32; i++) diff |= (uint8_t)(digest[i] ^ expected[i]);

	jsonWipe(&ctx, sizeof(ctx));
	jsonWipe(digest, sizeof(digest));
	return diff == 0;
}

/************************************************************************/
/* Login request parser													*/
/************************************************************************/

static const char *jsonSkipSpace(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
	return p;
}

// One JSON string of printable ASCII, without escapes. Returns the position after
// the closing quote, or 0 if the string is malformed or does not fit dest.
static const char *jsonParseCredentialString(const char *p, char *dest, size_t size)
{
	size_t n = 0;

	if (*p != '"') return 0;
	p++;
	while (*p != '"') {
		unsigned char c = (unsigned char)*p;
		if (c < 0x20 || c > 0x7e || c == '\\') return 0; // also stops at the end of the data
		if (n + 1 >= size) return 0;
		dest[n++] = (char)c;
		p++;
	}
	dest[n] = 0;
	return p + 1;
}

int jsonParseCredentials(const char *data, char *user, size_t userSize, char *password, size_t passwordSize)
{
	char key[16];
	const char *p;
	int haveUser = 0, havePassword = 0;

	if (data == 0 || user == 0 || password == 0 || userSize == 0 || passwordSize == 0) return 0;
	user[0] = 0;
	password[0] = 0;

	p = jsonSkipSpace(data);
	if (*p != '{') return 0;
	p = jsonSkipSpace(p + 1);
	if (*p == '}') return 0;

	for (;;) {
		p = jsonParseCredentialString(p, key, sizeof(key));
		if (p == 0) break;
		p = jsonSkipSpace(p);
		if (*p != ':') break;
		p = jsonSkipSpace(p + 1);

		if (strcmp(key, "user") == 0 && !haveUser) {
			p = jsonParseCredentialString(p, user, userSize);
			haveUser = 1;
		} else if (strcmp(key, "password") == 0 && !havePassword) {
			p = jsonParseCredentialString(p, password, passwordSize);
			havePassword = 1;
		} else {
			break;
		}
		if (p == 0) break;

		p = jsonSkipSpace(p);
		if (*p == ',') { p = jsonSkipSpace(p + 1); continue; }
		if (*p == '}' && *jsonSkipSpace(p + 1) == 0 && haveUser && havePassword
			&& user[0] != 0 && password[0] != 0) return 1;
		break;
	}

	// Leave nothing of a rejected request behind
	jsonWipe(user, userSize);
	jsonWipe(password, passwordSize);
	return 0;
}

/*
 * File: test_credential.c
 * Host tests for jsonCredential.c: SHA-256, the password check and the
 * login request parser. Build and run with run.bat.
 */

#include <stdio.h>
#include <string.h>
#include "jsonCredential.h"
#include "jsonName.h"

static int checks = 0, failures = 0;

#define CHECK(cond, what) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what); } } while (0)

static void toHex(const unsigned char *digest, char *hex)
{
	static const char digits[] = "0123456789abcdef";
	int i;
	for (i = 0; i < 32; i++) {
		hex[2 * i] = digits[digest[i] >> 4];
		hex[2 * i + 1] = digits[digest[i] & 15];
	}
	hex[64] = 0;
}

static void sha256Hex(const char *data, size_t length, char *hex)
{
	jsonSha256_ctx ctx;
	unsigned char digest[32];
	jsonSha256Init(&ctx);
	jsonSha256Update(&ctx, (const uint8_t*)data, length);
	jsonSha256Final(&ctx, digest);
	toHex(digest, hex);
}

// Same data one byte per update, to cross every block boundary from inside Update
static void sha256HexBytewise(const char *data, size_t length, char *hex)
{
	jsonSha256_ctx ctx;
	unsigned char digest[32];
	size_t i;
	jsonSha256Init(&ctx);
	for (i = 0; i < length; i++) jsonSha256Update(&ctx, (const uint8_t*)data + i, 1);
	jsonSha256Final(&ctx, digest);
	toHex(digest, hex);
}

static void testSha256(void)
{
	// Expected values from Node.js crypto
	static const struct { size_t n; const char *hex; } as[] = {
		{ 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
		{ 1, "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb" },
		{ 55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318" },
		{ 56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a" },
		{ 57, "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6" },
		{ 63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34" },
		{ 64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb" },
		{ 65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0" },
		{ 119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb" },
		{ 120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c" },
		{ 128, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e" }
	};
	static char buffer[1000];
	char hex[65];
	size_t i;
	jsonSha256_ctx ctx;
	unsigned char digest[32];

	memset(buffer, 'a', sizeof(buffer));
	for (i = 0; i < sizeof(as) / sizeof(as[0]); i++) {
		sha256Hex(buffer, as[i].n, hex);
		CHECK(strcmp(hex, as[i].hex) == 0, "sha256 of n times 'a'");
		sha256HexBytewise(buffer, as[i].n, hex);
		CHECK(strcmp(hex, as[i].hex) == 0, "sha256 of n times 'a', one byte per update");
	}

	sha256Hex("abc", 3, hex);
	CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256 abc");

	sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
	CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0, "sha256 NIST 448-bit");

	jsonSha256Init(&ctx);
	for (i = 0; i < 1000; i++) jsonSha256Update(&ctx, (const uint8_t*)buffer, 1000);
	jsonSha256Final(&ctx, digest);
	toHex(digest, hex);
	CHECK(strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0, "sha256 one million a");
}

static void testPasswordMatches(void)
{
	const char *hash = "c252ce8ac5e16c42f032889c36b4c36459d1725b6c94b883e84886049345ee65"; // "s1-service" + "111111"
	const char *upper = "C252CE8AC5E16C42F032889C36B4C36459D1725B6C94B883E84886049345EE65";

	CHECK(jsonPasswordMatches("s1-service", "111111", hash) == 1, "right password");
	CHECK(jsonPasswordMatches("s1-service", "111111", upper) == 1, "upper-case hash");
	CHECK(jsonPasswordMatches("s1-service", "111112", hash) == 0, "wrong password");
	CHECK(jsonPasswordMatches("s1-factory", "111111", hash) == 0, "other salt");
	CHECK(jsonPasswordMatches("s1-service", "", hash) == 0, "empty password");
	CHECK(jsonPasswordMatches("", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 1, "empty salt");
	CHECK(jsonPasswordMatches("s1-service", "111111", "c252ce8ac5e16c42f032889c36b4c36459d1725b6c94b883e84886049345ee6") == 0, "63-digit hash");
	CHECK(jsonPasswordMatches("s1-service", "111111", "g252ce8ac5e16c42f032889c36b4c36459d1725b6c94b883e84886049345ee65") == 0, "non-hex hash");
	CHECK(jsonPasswordMatches("s1-service", "111111", "") == 0, "empty hash");
	CHECK(jsonPasswordMatches(0, "111111", hash) == 0, "null salt");
	CHECK(jsonPasswordMatches("s1-service", 0, hash) == 0, "null password");
	CHECK(jsonPasswordMatches("s1-service", "111111", 0) == 0, "null hash");
}

static int parse(const char *data, char *user, size_t userSize, char *password, size_t passwordSize)
{
	return jsonParseCredentials(data, user, userSize, password, passwordSize);
}

static void testParseCredentials(void)
{
	char user[32], password[64], small[8];
	char longPassword[80], json[160];

	CHECK(parse("{\"user\":\"service\",\"password\":\"111111\"}", user, sizeof(user), password, sizeof(password)) == 1, "plain request");
	CHECK(strcmp(user, "service") == 0 && strcmp(password, "111111") == 0, "plain request values");

	CHECK(parse(" { \"password\" : \"p w\" , \"user\" : \"u\" } \r\n", user, sizeof(user), password, sizeof(password)) == 1, "spaces, reversed keys");
	CHECK(strcmp(user, "u") == 0 && strcmp(password, "p w") == 0, "spaces, reversed keys values");

	CHECK(parse("{\"user\":\"service\"}", user, sizeof(user), password, sizeof(password)) == 0, "no password");
	CHECK(parse("{\"password\":\"111111\"}", user, sizeof(user), password, sizeof(password)) == 0, "no user");
	CHECK(parse("{\"user\":\"service\",\"password\":\"\"}", user, sizeof(user), password, sizeof(password)) == 0, "empty password");
	CHECK(parse("{\"user\":\"\",\"password\":\"1\"}", user, sizeof(user), password, sizeof(password)) == 0, "empty user");
	CHECK(parse("{\"user\":\"a\",\"password\":\"1\",\"level\":\"9\"}", user, sizeof(user), password, sizeof(password)) == 0, "unknown key");
	CHECK(parse("{\"user\":\"a\",\"user\":\"b\",\"password\":\"1\"}", user, sizeof(user), password, sizeof(password)) == 0, "repeated key");
	CHECK(parse("{\"user\":\"a\",\"password\":\"x\\\"y\"}", user, sizeof(user), password, sizeof(password)) == 0, "escape in a value");
	CHECK(parse("{\"user\":\"a\",\"password\":\"\xc3\xa9\"}", user, sizeof(user), password, sizeof(password)) == 0, "non-ASCII");
	CHECK(parse("{\"user\":\"a\",\"password\":\"1\"} x", user, sizeof(user), password, sizeof(password)) == 0, "trailing data");
	CHECK(parse("{\"user\":\"a\",\"password\":\"1\"", user, sizeof(user), password, sizeof(password)) == 0, "unterminated object");
	CHECK(parse("{\"user\":\"a\",\"password\":1}", user, sizeof(user), password, sizeof(password)) == 0, "number value");
	CHECK(parse("{}", user, sizeof(user), password, sizeof(password)) == 0, "empty object");
	CHECK(parse("null", user, sizeof(user), password, sizeof(password)) == 0, "null");
	CHECK(parse(0, user, sizeof(user), password, sizeof(password)) == 0, "null data");
	CHECK(parse("   ", user, sizeof(user), password, sizeof(password)) == 0, "whitespace only");
	CHECK(parse("{\"user\":\"a\",\"password\":\"1\",}", user, sizeof(user), password, sizeof(password)) == 0, "trailing comma");
	CHECK(parse("{\"user\":{\"x\":\"y\"},\"password\":\"1\"}", user, sizeof(user), password, sizeof(password)) == 0, "object as a value");
	CHECK(parse("{\"user_name_longer_than_15\":\"a\",\"password\":\"1\"}", user, sizeof(user), password, sizeof(password)) == 0, "key longer than 15");
	// What the server hands over when a client appends a newline after the frame's '}'
	CHECK(parse("{\"user\":\"a\",\"password\":\"1\"}}", user, sizeof(user), password, sizeof(password)) == 0, "outer brace left over");

	// Too long is refused, not cut: 7 characters fit 8 bytes, 8 do not
	CHECK(parse("{\"user\":\"1234567\",\"password\":\"1\"}", small, sizeof(small), password, sizeof(password)) == 1, "user that just fits");
	CHECK(parse("{\"user\":\"12345678\",\"password\":\"1\"}", small, sizeof(small), password, sizeof(password)) == 0, "user one too long");
	memset(longPassword, 'p', 63);
	longPassword[63] = 0;
	sprintf(json, "{\"user\":\"a\",\"password\":\"%s\"}", longPassword);
	CHECK(parse(json, user, sizeof(user), password, sizeof(password)) == 1, "63-character password");
	memset(longPassword, 'p', 64);
	longPassword[64] = 0;
	sprintf(json, "{\"user\":\"a\",\"password\":\"%s\"}", longPassword);
	CHECK(parse(json, user, sizeof(user), password, sizeof(password)) == 0, "64-character password");

	// A rejected request leaves nothing behind
	parse("{\"user\":\"leak\",\"password\":\"secret\",\"x\":\"y\"}", user, sizeof(user), password, sizeof(password));
	CHECK(user[0] == 0 && password[0] == 0 && memcmp(password, "\0\0\0\0\0\0\0", 7) == 0, "buffers wiped after a rejection");
}

static void testNames(void)
{
	static const char *canonical[] = {
		"HMI_Cmd", "HMI_Cmd[5]", "HMI_Cmd[5].kind", "HMI_Pack[0].cells[199]", "HMI_Cmd[0]",
		"Task:var", "Task:var.m[10]", "_x", "a1.b2", "gArray[1][2]"
	};
	static const char *refused[] = {
		"", "HMI_Cmd[05]", "HMI_Cmd[ 5]", "HMI_Cmd[5] ", " HMI_Cmd", "HMI_Cmd[-5]", "HMI_Cmd[+5]",
		"HMI_Cmd[5", "HMI_Cmd[]", "HMI_Cmd[0x5]", "HMI_Cmd[1,2]", "HMI_Cmd.", "HMI_Cmd..kind",
		".x", "1abc", "::x", "a::b", "a:b:c", "a[1]x", "a b", "*p", "a.1b", "HMI_Cmd[5]\n"
	};
	size_t i;

	for (i = 0; i < sizeof(canonical) / sizeof(canonical[0]); i++) {
		CHECK(jsonNameIsCanonical(canonical[i]) == 1, canonical[i]);
	}
	for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
		CHECK(jsonNameIsCanonical(refused[i]) == 0, refused[i]);
	}
	CHECK(jsonNameIsCanonical(0) == 0, "null name");

	CHECK(jsonNameIsBelow("HMI_Cmd", "HMI_Cmd") == 1, "entry itself");
	CHECK(jsonNameIsBelow("HMI_Cmd[5].kind", "HMI_Cmd") == 1, "element below");
	CHECK(jsonNameIsBelow("HMI_Cmd.x", "HMI_Cmd") == 1, "member below");
	CHECK(jsonNameIsBelow("hmi_cmd[5]", "HMI_Cmd[5]") == 1, "case ignored");
	CHECK(jsonNameIsBelow("HMI_CmdX", "HMI_Cmd") == 0, "longer name");
	CHECK(jsonNameIsBelow("HMI_Cmd[50]", "HMI_Cmd[5]") == 0, "longer index");
	CHECK(jsonNameIsBelow("HMI_Cmd", "HMI_Cmd[5]") == 0, "shorter name");
	CHECK(jsonNameIsBelow("HMI_Cmd", "") == 0, "empty entry");
}

int main(void)
{
	testSha256();
	testPasswordMatches();
	testParseCredentials();
	testNames();
	printf("%d checks, %d failed\n", checks, failures);
	return failures != 0;
}

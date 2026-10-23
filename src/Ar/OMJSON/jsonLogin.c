/*
 * File: jsonLogin.c
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 *
 * This file is part of OMJSON, licensed under the MIT License.
 */

#include <bur/plctypes.h>
#ifdef __cplusplus
	extern "C"
	{
#endif

	#include "OMJSON.h"
	#include "jsonInternal.h"
	#include "jsonCredential.h"
	#include <string.h>

#ifdef __cplusplus
	};
#endif

/************************************************************************/
/* Internal: WebSocket client login										*/
/************************************************************************/

// A login belongs to one connection. It ends on logout, on a failed login,
// after loginTimeout without a write or touch, and when the connection closes.

// ms as TON_10ms ticks, rounded up so a short time does not become no time
static UDINT jsonTicks(UDINT ms)
{
	return ms / 10 + (ms % 10 != 0);
}

static void jsonAppend(char* out, UDINT size, const char* text)
{
	UDINT used = strlen(out), add = strlen(text);
	if (used + add >= size) add = size - used - 1;
	memcpy(out + used, text, add);
	out[used + add] = 0;
}

// Copy a string into a fixed buffer, zero-filling the rest so no stale bytes remain
static void jsonCopyString(char* dest, UDINT size, const char* src)
{
	memset(dest, 0, size);
	strncpy(dest, src, size - 1);
}

static void jsonAppendNumber(char* out, UDINT size, DINT value)
{
	char number[12];
	brsitoa(value, (UDINT)number);
	jsonAppend(out, size, number);
}

// Level the credentials grant, 0 if they grant none
static USINT jsonUserLevel(jsonAccess_typ* access, const char* user, const char* password)
{
	jsonUser_typ* users = (jsonUser_typ*)access->pUsers;
	UINT i;

	for (i = 0; i < access->numUsers; i++) {
		if (users[i].level == 0 || strcmp(users[i].name, user) != 0) continue;
		return jsonPasswordMatches(users[i].salt, password, users[i].hash) ? users[i].level : 0;
	}

	// No such user: hash anyway, so the answer takes as long as for a known user
	jsonPasswordMatches("", password, "0000000000000000000000000000000000000000000000000000000000000000");
	return 0;
}

void jsonInternalLoginEnd(struct jsonWebSocketServer* t, UINT index)
{
	t->ClientInfo[index].Level = 0;
	memset(t->ClientInfo[index].User, 0, sizeof(t->ClientInfo[index].User));
	t->internal.client[index].loginTimer.IN = 0;
	TON_10ms(&t->internal.client[index].loginTimer);
}

void jsonInternalLoginActivity(struct jsonWebSocketServer* t, UINT index)
{
	// Restart the count; jsonInternalLoginCycle runs the timer again
	t->internal.client[index].loginTimer.IN = 0;
	TON_10ms(&t->internal.client[index].loginTimer);
}

void jsonInternalLoginCycle(struct jsonWebSocketServer* t, UINT index)
{
	jsonAccess_typ* access = (jsonAccess_typ*)t->pAccess;
	struct TON_10ms* timer = &t->internal.client[index].loginTimer;

	if (t->ClientInfo[index].Level == 0) return;

	if (!t->internal.client[index].wsStream.out.active || access == 0 || access->pUsers == 0) {
		jsonInternalLoginEnd(t, index);
		return;
	}

	timer->IN = 1;
	timer->PT = jsonTicks(access->loginTimeout != 0 ? access->loginTimeout : JSON_DEFAULT_LOGINTIMEOUT);
	TON_10ms(timer);
	if (timer->Q) {
		access->loginTimeoutCount++;
		jsonInternalLoginEnd(t, index);
	}
}

// Failed logins count until loginLockout passes without one. A success does not
// clear them, or any valid login would reset the count for guessing another.
void jsonInternalLockoutCycle(struct jsonWebSocketServer* t)
{
	jsonAccess_typ* access = (jsonAccess_typ*)t->pAccess;
	UDINT lockout = (access != 0 && access->loginLockout != 0) ? access->loginLockout : JSON_DEFAULT_LOGINLOCKOUT;

	t->internal.lockoutTimer.IN = t->internal.loginFailures != 0;
	t->internal.lockoutTimer.PT = jsonTicks(lockout);
	TON_10ms(&t->internal.lockoutTimer);
	if (t->internal.lockoutTimer.Q) {
		t->internal.loginFailures = 0;
		t->internal.lockoutTimer.IN = 0;
		TON_10ms(&t->internal.lockoutTimer);
	}
}

static void jsonLoginFailed(struct jsonWebSocketServer* t)
{
	if (t->internal.loginFailures < 255) t->internal.loginFailures++;
	// Restart the window: the count clears loginLockout after the last failure
	t->internal.lockoutTimer.IN = 0;
	TON_10ms(&t->internal.lockoutTimer);
}

// {"type":"login","data":{"user":"...","password":"..."}}, {"type":"logout","data":{}}
// or {"type":"touch","data":{}}. Writes the response data, {"ok":true,"level":L,"client":C}
// or {"ok":false,"error":"...","level":L,"client":C}, to pResponse. pData, the
// request data in the receive buffer, is wiped: it may hold a password.
void jsonInternalLoginRequest(struct jsonWebSocketServer* t, UINT index, const char* requestType,
	char* pData, char* pResponse, UDINT responseSize)
{
	jsonAccess_typ* access = (jsonAccess_typ*)t->pAccess;
	jsonWSS_client_info_typ* info = &t->ClientInfo[index];
	char user[sizeof(info->User)];
	char password[JSON_CREDENTIAL_PASSWORDSIZE];
	const char* error = 0;
	USINT level;

	user[0] = 0;
	password[0] = 0;

	if (strcmp(requestType, "logout") == 0) {
		if (info->Level != 0 && access != 0) access->logoutCount++;
		jsonInternalLoginEnd(t, index);
	}
	else if (strcmp(requestType, "touch") == 0) {
		if (info->Level != 0) jsonInternalLoginActivity(t, index);
	}
	else if (access == 0 || access->pUsers == 0 || access->numUsers == 0) {
		error = "disabled";
	}
	else if (t->internal.loginFailures >= JSON_LOGIN_MAX_FAILURES) {
		error = "locked";
	}
	else if (!jsonParseCredentials(pData, user, sizeof(user), password, sizeof(password))) {
		error = "request";
	}
	else {
		jsonCopyString(access->lastLoginUser, sizeof(access->lastLoginUser), user);
		jsonCopyString(access->lastLoginClientIP, sizeof(access->lastLoginClientIP), info->ClientIP);

		level = jsonUserLevel(access, user, password);
		if (level != 0) {
			info->Level = level;
			jsonCopyString(info->User, sizeof(info->User), user);
			jsonInternalLoginActivity(t, index);
			access->loginCount++;
		} else {
			// A failed login also ends the session the connection had
			jsonInternalLoginEnd(t, index);
			access->loginFailCount++;
			jsonLoginFailed(t);
			error = "denied";
		}
	}

	jsonWipe(password, sizeof(password));
	if (pData != 0) jsonWipe(pData, strlen(pData));

	pResponse[0] = 0;
	if (error) {
		jsonAppend(pResponse, responseSize, "{\"ok\":false,\"error\":\"");
		jsonAppend(pResponse, responseSize, error);
		jsonAppend(pResponse, responseSize, "\",\"level\":");
	} else {
		jsonAppend(pResponse, responseSize, "{\"ok\":true,\"level\":");
	}
	jsonAppendNumber(pResponse, responseSize, info->Level);
	jsonAppend(pResponse, responseSize, ",\"client\":");
	jsonAppendNumber(pResponse, responseSize, index);
	jsonAppend(pResponse, responseSize, "}");
}

(*
 * File: OMJSON.typ
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 * 
 * This file is part of OMJSON, licensed under the MIT License.
 *)

TYPE
	JSON_ERR_enum : 
		(
		JSON_ERR_INVALIDINPUT := 50000,
		JSON_ERR_MEMALLOC,
		JSON_ERR_MAXLEVELREACHED,
		JSON_ERR_PARSE,
		JSON_ERR_HUH,
		JSON_ERR_CACHEFULLITEMS,
		JSON_ERR_CACHEFULLVARS,
		JSON_ERR_CACHEEMPTY,
		JSON_ERR_MAXITERATIONS,
		JSON_ERR_WS_KEYNOTFOUND,
		JSON_ERR_WS_FRAGMENT,
		JSON_ERR_WS_RSV,
		JSON_ERR_WS_OPCODE,
		JSON_ERR_WS_MASK,
		JSON_ERR_WS_PAYLOADLENGTH,
		JSON_ERR_TCP_FRAGMENT,
		JSON_ERR_TCP_BAD_PACKET,
		JSON_ERR_NO_LICENSE,
		JSON_ERR_WS_MAX_CLIENTS,
		JSON_ERR_WS_INVALID_MAX_CLIENTS,
		JSON_ERR_ACCESSDENIED,
		JSON_ERR_WS_,
		JSON_ERR_
		);
	JSON_ACCESS_enum :
		(
		JSON_ACCESS_NONE := 0,
		JSON_ACCESS_READ,
		JSON_ACCESS_WRITE,
		JSON_ACCESS_READWRITE
		);
	jsonAccessEntry_typ : 	STRUCT
		name : STRING[VAR_STRLEN_NAME]; (*Variable name. Grants access to the variable and everything below it (members, array elements)*)
		access : JSON_ACCESS_enum; (*Access granted to clients*)
		writeLevel : USINT; (*Login level a client needs to write here. 0 = any client, logged in or not. Reads are not affected*)
	END_STRUCT;
	jsonUser_typ : 	STRUCT
		name : STRING[JSON_STRLEN_USER]; (*Login name, case-sensitive*)
		salt : STRING[JSON_STRLEN_SALT]; (*Any text, different for every user*)
		hash : STRING[JSON_STRLEN_HASH]; (*SHA-256 of salt followed by the password, as 64 hex digits*)
		level : USINT; (*Login level a client gets as this user. 0 disables the user*)
	END_STRUCT;
	jsonAccess_typ : 	STRUCT
		pEntries : UDINT; (*Address of an array of jsonAccessEntry_typ. Variables that are not covered by an entry are not accessible*)
		numEntries : UINT; (*Number of elements in the array at pEntries. 0 denies all access*)
		disableWrites : BOOL; (*Reject all writes, regardless of entries*)
		allowedClientIP : ARRAY[0..JSON_MAI_CLIENTS]OF STRING[TCPCOMM_STRLEN_IPADDRESS]; (*Client IP addresses allowed to connect. All empty allows any client*)
		deniedReadCount : UDINT;
		deniedWriteCount : UDINT;
		deniedConnectCount : UDINT;
		lastDeniedName : STRING[VAR_STRLEN_NAME];
		lastDeniedClientIP : STRING[TCPCOMM_STRLEN_IPADDRESS];
		pUsers : UDINT; (*Address of an array of jsonUser_typ for WebSocket logins. 0 disables login: every client stays at level 0*)
		numUsers : UINT; (*Number of elements in the array at pUsers*)
		loginTimeout : UDINT; (*ms without a write or touch before a logged-in client drops back to level 0. 0 = JSON_DEFAULT_LOGINTIMEOUT*)
		loginLockout : UDINT; (*ms after the last failed login until failures are forgotten; JSON_LOGIN_MAX_FAILURES of them refuse logins until then. 0 = JSON_DEFAULT_LOGINLOCKOUT*)
		loginCount : UDINT; (*Successful logins*)
		loginFailCount : UDINT; (*Failed logins; attempts refused during a lockout are not counted*)
		logoutCount : UDINT; (*Sessions ended by the client's logout*)
		loginTimeoutCount : UDINT; (*Sessions ended by loginTimeout*)
		lastLoginUser : STRING[JSON_STRLEN_USER]; (*User name of the last login attempt, failed or not*)
		lastLoginClientIP : STRING[TCPCOMM_STRLEN_IPADDRESS]; (*Client of the last login attempt*)
	END_STRUCT;
	jsonWSS_client_info_typ : 	STRUCT
		Connected : BOOL;
		TimeSinceLastRequest_ms : UDINT;
		ClientIP : STRING[TCPCOMM_STRLEN_IPADDRESS];
		ClientPort : UINT;
		Level : USINT; (*Login level of the connection. 0 = not logged in. Reset when the connection closes*)
		User : STRING[JSON_STRLEN_USER]; (*User the connection is logged in as*)
	END_STRUCT;
	jsonWSS_client_debug_typ : 	STRUCT 
		socketConnectCount : DINT;
		websocketConnectCount : DINT;
		socketDisconnectCountTimeout : DINT;
		socketDisconnectCountOpcode : DINT;
		socketDisconnectCountError : DINT;
		socketDisconnectCountRecvLength0 : DINT;
		socketInvalidCountError : DINT;
		noDataCount : DINT;
		dataCount : DINT;
		justRightCount : DINT;
		tooBigCount : DINT;
		tooSmallCount : DINT;
		doubleDataCount : DINT;
		oldDataReceived : BOOL;
	END_STRUCT;
	jsonWSServer_int_client_typ : 	STRUCT 
		wsStream : WSStream_typ;
		pReceiveData : UDINT;
		pSendData : UDINT;
		messageBuffer : datbufBuffer_typ;
		wsConnected : BOOL;
		writeVariable : jsonWriteVariable;
		readVariableList : jsonReadVariableList;
		debug : jsonWSS_client_debug_typ;
		requestTimer : TON_10ms;
		pCache : REFERENCE TO jsonCache_typ;
		loginTimer : TON_10ms; (*Time since the last write or touch of a logged-in client*)
	END_STRUCT;
	jsonWSServer_Internal_typ : 	STRUCT 
		initialized : BOOL;
		wsServer : WSConnectionManager_typ;
		client : ARRAY[0..JSON_MAI_CLIENTS]OF jsonWSServer_int_client_typ;
		requestTimer : TON_10ms;
		iClient : UINT;
		connectedClients : UINT;
		maxClients : UINT;
		loginFailures : USINT; (*Failed logins in a row, over all clients*)
		lockoutTimer : TON_10ms;
	END_STRUCT;
	jsonHTTPServer_Int_Debug_typ : 	STRUCT
		readSendCommand : BOOL;
	END_STRUCT;
	jsonHTTPServer_Int_Write_typ : 	STRUCT 
		webService : httpService;
		pUri : UDINT;
		pRequestData : UDINT;
		ResponseHeader : httpResponseHeader_t;
		processed : BOOL;
		writeVariable : jsonWriteVariable;
	END_STRUCT;
	jsonHTTPServer_Int_Read_typ : 	STRUCT 
		webService : httpService;
		pUri : UDINT;
		pRequestData : UDINT;
		ResponseHeader : httpResponseHeader_t;
		processed : BOOL;
		readVariableList : jsonReadVariableList;
	END_STRUCT;
	jsonHTTPServer_Internal_typ : 	STRUCT 
		initialized : BOOL;
		read : jsonHTTPServer_Int_Read_typ;
		write : jsonHTTPServer_Int_Write_typ;
		debug : jsonHTTPServer_Int_Debug_typ;
	END_STRUCT;
	jsonWSConnect_Internal_typ : 	STRUCT 
		encodeBase64 : httpEncodeBase64;
	END_STRUCT;
	JSON_TYPE_enum : 
		(
		JSON_TYPE_INVALID := 0,
		JSON_TYPE_OBJECT_ELEMENT,
		JSON_TYPE_ARRAY_ELEMENT
		);
	jsonStructLevel_typ : 	STRUCT 
		variable : varVariable_typ;
		iItem : UINT;
		jsonType : JSON_TYPE_enum;
	END_STRUCT;
	jsonWriteVariable_Int_typ : 	STRUCT 
		initialized : BOOL;
		pJSONObject : UDINT;
		pChar : UDINT;
		iStructLevel : INT;
		structLevel : ARRAY[0..JSON_MAI_STRUCTLEVEL]OF jsonStructLevel_typ;
	END_STRUCT;
	jsonReadVariableList_Int_typ : 	STRUCT 
		initialized : BOOL;
		outputBuffer : datbufBuffer_typ;
		readVariable : jsonReadVariable;
	END_STRUCT;
	jsonReadVariable_Int_typ : 	STRUCT 
		initialized : BOOL;
		outputBuffer : datbufBuffer_typ;
		variableName : STRING[VAR_STRLEN_NAME];
		addVariableToCache : jsonAddVariableToCache;
	END_STRUCT;
	jsonAddVariableToCache_Int_typ : 	STRUCT 
		initialized : BOOL;
		iStructLevel : INT;
		structLevel : ARRAY[0..JSON_MAI_STRUCTLEVEL]OF jsonStructLevel_typ;
	END_STRUCT;
	jsonCache_typ : 	STRUCT 
		iVariable : UINT;
		variable : ARRAY[0..JSON_MAI_CACHEVAR]OF jsonCacheVariable_typ;
		iItem : UINT;
		item : ARRAY[0..JSON_MAI_CACHEITEM]OF jsonCacheItem_typ;
	END_STRUCT;
	jsonCacheVariable_typ : 	STRUCT 
		name : STRING[VAR_STRLEN_NAME];
		iStart : UINT;
		iEnd : UINT;
	END_STRUCT;
	jsonCacheItem_typ : 	STRUCT 
		prefix : STRING[JSON_STRLEN_PREFIX];
		variable : varVariable_typ;
		suffix : STRING[JSON_STRLEN_SUFFIX];
	END_STRUCT;
	jsonReadVarNoCache_Int_typ : 	STRUCT 
		initialized : BOOL;
		outputBuffer : datbufBuffer_typ;
		iStructLevel : INT;
		structLevel : ARRAY[0..JSON_MAI_STRUCTLEVEL]OF jsonStructLevel_typ;
	END_STRUCT;
END_TYPE

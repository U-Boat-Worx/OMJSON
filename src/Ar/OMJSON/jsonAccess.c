/*
 * File: jsonAccess.c
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
	#include "jsonName.h"
	#include <string.h>

#ifdef __cplusplus
	};
#endif


/************************************************************************/
/* Internal: Check a variable name against an access list			 	*/
/************************************************************************/

// Returns 1 if access is granted, 0 if it is denied
// required must be JSON_ACCESS_READ or JSON_ACCESS_WRITE
// level is the client's login level; a write also needs the entry's writeLevel

plcbit jsonInternalCheckAccess(UDINT pAccess, char* name, UDINT required, USINT level)
{

	jsonAccess_typ* access = (jsonAccess_typ*)pAccess;

	// No access list means no restrictions
	if( access == 0 ) return 1;

	UDINT granted = JSON_ACCESS_NONE;
	USINT writeLevel = 0;

	// Only the canonical spelling is matched: another one (HMI_Cmd[05]) could reach
	// the same variable past a more specific entry. Case is ignored, as IEC does.
	if( jsonNameIsCanonical(name) && access->pEntries != 0 ){

		jsonAccessEntry_typ* entry = (jsonAccessEntry_typ*)access->pEntries;
		UDINT i, length, matchLength = 0;

		for( i = 0; i < access->numEntries; i++ ){

			length = strlen(entry[i].name);
			if( length == 0 || length < matchLength ) continue;

			// name has to be the entry itself or something below it.
			// 'gHmi' covers 'gHmi', 'gHmi.cmd' and 'gHmi[0]', but not 'gHmiOther'
			if( !jsonNameIsBelow(name, entry[i].name) ) continue;

			// Most specific entry wins, its write level included
			matchLength = length;
			granted = entry[i].access;
			writeLevel = entry[i].writeLevel;

		}

	}

	plcbit allowed;

	if( required == JSON_ACCESS_WRITE ){
		allowed = (granted == JSON_ACCESS_WRITE || granted == JSON_ACCESS_READWRITE) && !access->disableWrites
			&& level >= writeLevel;
		if( !allowed ) access->deniedWriteCount++;
	}
	else{
		allowed = (granted == JSON_ACCESS_READ || granted == JSON_ACCESS_READWRITE);
		if( !allowed ) access->deniedReadCount++;
	}

	if( !allowed ){
		memset( access->lastDeniedName, 0, sizeof(access->lastDeniedName) );
		if( name != 0 ) strncpy( access->lastDeniedName, name, sizeof(access->lastDeniedName) - 1 );
	}

	return allowed;

} // End Fn //

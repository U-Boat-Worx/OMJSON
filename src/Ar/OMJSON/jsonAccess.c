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
	#include <string.h>

#ifdef __cplusplus
	};
#endif


/************************************************************************/
/* Internal: Check a variable name against an access list			 	*/
/************************************************************************/

// Returns 1 if access is granted, 0 if it is denied
// required must be JSON_ACCESS_READ or JSON_ACCESS_WRITE

plcbit jsonInternalCheckAccess(UDINT pAccess, char* name, UDINT required)
{

	jsonAccess_typ* access = (jsonAccess_typ*)pAccess;

	// No access list means no restrictions
	if( access == 0 ) return 1;

	UDINT granted = JSON_ACCESS_NONE;

	if( name != 0 && access->pEntries != 0 ){

		jsonAccessEntry_typ* entry = (jsonAccessEntry_typ*)access->pEntries;
		UDINT i, length, matchLength = 0;

		for( i = 0; i < access->numEntries; i++ ){

			length = strlen(entry[i].name);
			if( length == 0 || length < matchLength ) continue;

			// name has to be the entry itself or something below it.
			// 'gHmi' covers 'gHmi', 'gHmi.cmd' and 'gHmi[0]', but not 'gHmiOther'
			if( strncmp(name, entry[i].name, length) != 0 ) continue;
			if( name[length] != 0 && name[length] != '.' && name[length] != '[' ) continue;

			// Most specific entry wins
			matchLength = length;
			granted = entry[i].access;

		}

	}

	plcbit allowed;

	if( required == JSON_ACCESS_WRITE ){
		allowed = (granted == JSON_ACCESS_WRITE || granted == JSON_ACCESS_READWRITE) && !access->disableWrites;
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

/*
 * File: jsonName.h
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 *
 * This file is part of OMJSON, licensed under the MIT License.
 */

/* Variable name checks for the access list. Plain C with no B&R headers, so
 * test/host can build it on a PC. */

#ifndef __JSON_NAME_H__
#define __JSON_NAME_H__

/* 1 if name is spelled the one way OMJSON spells names it builds itself:
 * [Task:]identifier, then any number of .identifier or [index] parts, an
 * index being a decimal number without sign, spaces or leading zeros.
 * The access list compares names as text, so another spelling of the same
 * variable (HMI_Cmd[05], HMI_Cmd[ 5]) must be refused before it can match a
 * shorter entry than the one meant for it. */
int jsonNameIsCanonical(const char *name);

/* 1 if name is entry or something below it ('gHmi' covers 'gHmi', 'gHmi.cmd'
 * and 'gHmi[0]', not 'gHmiOther'), ignoring ASCII case as IEC names do */
int jsonNameIsBelow(const char *name, const char *entry);

#endif

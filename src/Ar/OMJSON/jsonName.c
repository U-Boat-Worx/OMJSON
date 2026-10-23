/*
 * File: jsonName.c
 * Copyright (c) 2023 Loupe
 * https://loupe.team
 *
 * This file is part of OMJSON, licensed under the MIT License.
 */

#include "jsonName.h"

static int jsonIsLetter(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int jsonIsDigit(char c)
{
	return c >= '0' && c <= '9';
}

static char jsonLower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

// One identifier; returns the position after it, or 0 if there is none
static const char *jsonIdentifier(const char *p)
{
	if (!jsonIsLetter(*p)) return 0;
	p++;
	while (jsonIsLetter(*p) || jsonIsDigit(*p)) p++;
	return p;
}

int jsonNameIsCanonical(const char *name)
{
	const char *p;

	if (name == 0) return 0;

	p = jsonIdentifier(name);
	if (p == 0) return 0;
	if (*p == ':') { // Task:variable
		p = jsonIdentifier(p + 1);
		if (p == 0) return 0;
	}

	while (*p) {
		if (*p == '.') {
			p = jsonIdentifier(p + 1);
			if (p == 0) return 0;
		} else if (*p == '[') {
			p++;
			if (*p == '0') {
				p++;
			} else if (jsonIsDigit(*p)) {
				while (jsonIsDigit(*p)) p++;
			} else {
				return 0;
			}
			if (*p != ']') return 0;
			p++;
		} else {
			return 0;
		}
	}
	return 1;
}

int jsonNameIsBelow(const char *name, const char *entry)
{
	if (name == 0 || entry == 0 || *entry == 0) return 0;
	while (*entry) {
		if (jsonLower(*name) != jsonLower(*entry)) return 0;
		name++;
		entry++;
	}
	return *name == 0 || *name == '.' || *name == '[';
}

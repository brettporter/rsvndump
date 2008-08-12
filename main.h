/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _MAIN_H
#define _MAIN_H


#include "list.h"

#include <stdio.h>


// Application-specific constants
#define APPNAME "rsvndump"
#define APPVERSION "0.1"
#define APPAUTHOR "Jonas Gehring <jonas.gehring@boolsoft.org>"

// Other constants
#define DUMPFORMAT_VERSION "2"
#define HEAD_REVISION -1


// Enumerations
typedef enum {
	NK_DIRECTORY, NK_FILE
} nodekind_t;

typedef enum {
	NK_CHANGE, NK_ADD, NK_DELETE, NK_REPLACE
} nodeaction_t;


// Data structures
typedef struct {
	char *key, *value;
} prop_t;

typedef struct {
	char *path;
	nodekind_t kind;
	nodeaction_t action;
} change_entry_t;


// Globals
extern char *repo_url;
extern char *repo_uuid;
extern char *repo_username;
extern char *repo_password;
extern int repo_prefix_len;
extern char quiet;
extern FILE *input, *output;


#endif

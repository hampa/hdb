/*************************************************************************
 * HDB, Copyright (C) 2006 Hampus Soderstrom                             *
 * All rights reserved.  Email: hampus@sxp.se                            *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of The GNU Lesser General Public License as *
 * published by the Free Software Foundation; either version 2.1 of the  *
 * License, or (at your option) any later version. The text of the GNU   *
 * Lesser General Public License is included with this library in the    *
 * file LICENSE.TXT.                                                     *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT for more details.                                         *
 *************************************************************************/

#include "hdb.h"

//structures for hdbd implementations

typedef struct {
	char *name;
	char *param;
	char *info;
	int func;
} hdb_cmd_t;

//TODO.. create static hash converst between commands and numbers. GETRAW->HDB_GET_VAL 
static hdb_cmd_t hdb_cmds[] =
{
	{"GET",  "<list> <key>", "Get value from list.\nOutput:<value>", HDB_GET_VAL},
	{"GETRAW",  "<list> <key>", "Get raw value from list.\n"
		"Output:<type> <value>\n"
		"Valid types are 0=str, 1=link, 2=int, 3=exec, 4=file" , HDB_GET_RAW},
	{"SETRAW",  "<list> <key> <value> <type>", "Set raw value.\nOutput:none", HDB_SET_RAW},
	{"SET",  "<list> <key> <value>", "Set value.\nOutput:none", HDB_SET_VAL},
	{"UPDATE", "<list> <key> <value>", "If key exist set value.\nOutput:none", HDB_UPDATE_VAL},
	{"DEL",  "<list> <key>", "Delete key.\nOutput:none", HDB_DEL_VAL},
	{"DUMP","[<list>]", "Dump list in hxml format\nOutput:\n<list>\nkey value</list>",HDB_DUMP},
	{"DUMPFLAT","[<list>]", "Dump list in flat format\nOutput:\nlist/key=value",HDB_DUMP_FLAT},
	{"DUMPFLATREGEX","<list> <key> <value> [<lkv>]", "Dump list in flat format matching regexp\n",HDB_DUMP_FLAT_REGEX},
	{"DUMPFLATGLOB","<list> <key> <value>", "Dump list in flat format matching glob\nOutput: list/key=value",HDB_DUMP_FLAT_GLOB},
	{"CUR",  "<list> <cursor>", "Get cursor", HDB_GET_CUR},
	{"REMOVE",  "<list>", "Remove list.", HDB_DELETE_LIST},
	{"CREATE", "<list>", "Create new list.", HDB_CREATE_LIST},
	{"PRINT", "<list>", "Print all values from list\nOutput:key value", HDB_PRINT},
	{"PRINTFULL", "<list>", "Print all values from list with full path\n.Output:list/key value", HDB_PRINT_FULL},
	{"SUBLIST", "<list>", "Print all sublist to list.\nOutput: list", HDB_PRINT_SUBLIST},
	{"SUBLISTFULL", "<list>", "Print all sublist to list with full path.\nOutput: list/sublist", HDB_PRINT_SUBLIST_FULL},
	{"GETSUBLIST", "<list> <index>", "Get the nth value from list starting.", HDB_GET_SUBLIST},
	{"GETSUBLISTFULL", "<list> <index>", "Get the nth value from list with full path.", HDB_GET_SUBLIST_FULL},
	{"GETSUBLISTCUR", "<list> <cursor>", "Use a cursor to get sublists.\n3=first, 7=next", HDB_GET_SUBLIST_CUR},
	{"GETSUBLISTCURFULL", "<list> <cursor>", "Use a cursor to get sublists with full path.\n3=first, 7=next", HDB_GET_SUBLIST_CUR_FULL},
	{"GETREC", "<list> <index>", "Get the nth record from list", HDB_GET_REC},
	{"SIZE", "<list>", "Get number of sublists to this list", HDB_GET_SIZE},
	{"EXIST", "<list> [key]", "Returns error if list or list key does not exist", HDB_EXIST},
	{"ADD", "<list> <key> <amount>", "Do addition to the value in key", HDB_ADD},
	{"LIST","" , "print all lists one level", HDB_PRINT_LIST},
	{"LISTR","" , "print all lists recursivly", HDB_PRINT_LISTR},
	{"WIPE","" , "delete all databases (use with care)", HDB_WIPE},
	{"MOVE","<source> <target>" , "move list to target", HDB_MOVE},
	{"SYNC","" , "Syncronize database to make sure you have the latest values", HDB_SYNC},
	{"EXEC","<list> <key> \"<command>\"" , "Create an exec value", HDB_SET_EXEC},
	{"LINK","<list> <key> <list/key>" , "Create a link value", HDB_SET_LINK},
	{"FILE","<list> <key> <path>" , "Create a file value", HDB_SET_FILE},
	{"ROOT","[<list>]", "show or set HDB root", HDB_ROOT},
	{"STAT","<list> [<key>]", "Fetch stat info key or list", HDB_STAT},
	{
		"PRINTSTAT","<list> [format]", 
		"Print all stat values from list\nFormat options:\nlist=1 key=2 value=4 mtime=8 atime=16 time=24 full=31 reltime=32 fullrt=63", 
		HDB_PRINT_STAT
	},
	{"CONFIG","<param> [<value>]", "Set or get config value", HDB_CONFIG},
	{"VERSION","", "Print version", HDB_VERSION},
	{"LOG","[syslog,file,console,none]", "Enable logging. No arg will return current log type", HDB_LOG},
	{"DEBUG","", "Print debug messages. All messages begin with #", HDB_DEBUG},
	{"STATUS", "" ,"print daemon status and statistics", HDB_STATUS},
	{"HELP", "[command]" ,"I am putting myself to the fullest possible use, which is all I think that any conscious entity can ever hope to do.", HDB_HELP},
	{"QUIT","", "Quit from HDB", HDB_QUIT},
	{NULL,NULL, NULL, HDB_NONE}
};


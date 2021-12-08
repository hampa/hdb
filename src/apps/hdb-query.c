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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <unistd.h>
#include "hdb.h"

#define CMD_VERSION "$Id: hdb-query.c,v 1.2 2006/04/05 00:54:48 hampusdb Exp $"

//#define DEBUG 1
#ifdef DEBUG
#define DBG(fmt...) if (1) fprintf(stderr,fmt)
#else
#define DBG(fmt...)
#endif

//make options avail 
int lflag=0; // List regex
int kflag=0; // Key regex
int vflag=0; // Value regex
int gflag=0; // GLOB list flag
int rflag=0; // Set root flag
int dflag=0; // Output debug information
int iflag=0; // Ignore stdin

int do_dump(char *target, char *list, char *key, char *value);

void usage(){
	fprintf(stderr, "hdb-query %s\n", CMD_VERSION);
	fprintf(stderr,
		"Usage hdb-query [switch \"args ..\"] [list/key=value ..]\n"
		"   -l <regex>   Find list matching regex\n"
		"   -k <regex>   Find key matching regex\n" 
		"   -v <regex>   Find value matching regex\n"
		"   -g <glob>    Find list matching glob\n"
		"   -r <DIR>     Use DIR as hdb root\n"
		"   -i           Ignore stdin\n"
		"   -d           Print debug information\n"
		"   -h           Show this help\n"
		);
	exit(1);
}

int main(int argc, char **argv){
	char line[MAXLENGTHX2];
	char hdbroot[MAXLENGTH];
	char list[MAXLENGTH];
	char key[MAXLENGTH];
	char value[MAXLENGTH];
	int op = 0;
	int hdb_net=0;
	int ret = 0;
#if HDB_NET_SWITCH
	int found_lock=0;
#endif

	strcpy(line, "");
	strcpy(list, ".*");
	strcpy(key, ".*");
	strcpy(value, ".*");

	while ((op = getopt(argc, argv, "idhg:r:l:k:v:")) != EOF){
		switch(op){
		case 'h':
			usage();
			break;
		case 'r':
			rflag++;
			strncpy(hdbroot, optarg, sizeof(hdbroot));
			break;
		case 'g':
			gflag++;
			strncpy(list, optarg, sizeof(list));
			break;
		case 'i':
			iflag++;
			break;
		case 'l':
			lflag++;
			strncpy(list, optarg, sizeof(list));
			break;
		case 'k':
			kflag++;
			strncpy(key, optarg, sizeof(key));
			break;
		case 'v':
			vflag++;
			strncpy(value, optarg, sizeof(value));
			break;
		case 'd':
			dflag++;
			break;
		default:
			usage();
			break;
		}	
	}
	DBG("optind=%i argc=%i\n", optind, argc); 
	hdb_net=hdb_get_access();

#if HDB_NET_SWITCH
        found_lock=hdb_check_lock(HDB_LOCKFILE);

        //if the daemon isn't started. Pass on the light-hdb
        if(hdb_net == HDB_ACCESS_NETWORK && found_lock){
                return execvp("lhdb-query", argv);
        }
        //if the daemon is running. Use it.
        if(hdb_net == HDB_ACCESS_FILE && !found_lock){
                return execvp("nhdb-query", argv);
        }
#endif

	if(hdb_net && hdb_connect("127.0.0.1", 12429, NULL, NULL, 0)){
        	fprintf(stderr, "ERR - Unable to connect to HDB daemon.\n");
		return 1;
	}

	if(rflag && hdb_set_root(hdbroot)){
		hdb_close();
		return HDB_ROOT_ERROR;
	}

	if (optind < argc) {
		while (optind < argc) {
			if(do_dump(argv[optind++], list, key, value)){
				ret=1;
			}
		}
	}
	//else if(lflag && !strcmp(list, "-")){
	//else if(!iflag && !isatty(fileno(stdin))){
	//else if (!lflag){
	else if (!lflag && !isatty(fileno(stdin) && !iflag)){
		if(dflag){
			fprintf(stderr, "reading from stdin\n");
		}
		while(fgets(line,sizeof(line),stdin)){
			line[strlen(line)-1]=0;
			if(do_dump(line, list, key, value)){
				ret=1;
			}
		}
	}
	else if(lflag || kflag || vflag){
		if(do_dump(line, list, key, value)){
			ret=1;
		}
	}
	else {
		usage();
	}
	hdb_close();
	
	return ret;
}
	

int do_dump(char *target, char *list, char *key, char *value){
	char tlist[MAXLENGTH];
	char tkey[MAXLENGTH];
	char tvalue[MAXLENGTH];
	int ret=0;

	
	if(!hdb_cut(target, sizeof(tlist), tlist, tkey, tvalue)){
		if(!strcmp(value,"")){
			strcpy(value, ".*");
		}
		if(!lflag){
			list=tlist;		
		}
		if(!kflag){
			key=tkey;		
		}
		if(!vflag){
			value=tvalue;		
		}
		if(dflag){
			fprintf(stderr, "do_dump target=%s list=%s key=%s value=%s\n", target, list, key, value);
		}
		if(gflag){
			ret = hdb_dump_glob(list, key, value);
		}
		else {
			ret = hdb_dump_regex(list, key, value);
		}
	}
	else {
		if(dflag){
			fprintf(stderr, "bad input unable to cut %s\n", target);
		}
		ret=3;
	}
	return ret;
}


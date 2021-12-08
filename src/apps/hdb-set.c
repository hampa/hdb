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
#include <libgen.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "hdb.h"

#define CMD_VERSION "$Id: hdb-set.c,v 1.1 2006/03/12 13:32:13 hampusdb Exp $"

#define HDB_LEGACY_SUPPORT 1

//#define DEBUG 1
#ifdef DEBUG
#define DBG(fmt...) if (1) fprintf(stderr,fmt)
#else
#define DBG(fmt...)
#endif

void path_to_listkey(char *target, char *list, char *key);
int do_set(char *hdbpath);

//make options avail 
int lflag=0;
int oflag=0;
int rflag=0;

void usage(){
	fprintf(stderr, "hdb-set %s\n", CMD_VERSION);
	fprintf(stderr,
		"Usage hdb-set [switch \"args ..\"] list/key=value\n"
		"   -r <DIR>      Use DIR as hdb root\n"
		"   -h            Show this help\n");
	exit(1);
}

//File was last accessed n minutes ago.
int main(int argc, char **argv){
	char line[MAXLENGTHX2];
	char hdbroot[MAXLENGTH];
	int op = 0;
	int ret = 0;
	int hdb_net=0;
#if HDB_NET_SWITCH
	int found_lock=0;
#endif
	
	while ((op = getopt(argc, argv, "hr:")) != EOF){
		switch(op){
		case 'h':
			usage();
			break;
		case 'r':
			rflag++;
			strncpy(hdbroot, optarg, sizeof(hdbroot));
			break;
		default:
			usage();
			break;
		}	
	}


	hdb_net=hdb_get_access();
#if HDB_NET_SWITCH
        found_lock=hdb_check_lock(HDB_LOCKFILE);

        //if the daemon isn't started. Pass on the light-hdb
        if(hdb_net == HDB_ACCESS_NETWORK && found_lock){
                return execvp("lhdb-set", argv);
        }
        //if the daemon is running. Use it.
        if(hdb_net == HDB_ACCESS_FILE && !found_lock){
                return execvp("nhdb-set", argv);
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
#ifdef HDB_LEGACY_SUPPORT
		// older version hdb-set list/key value
		if(!strstr(argv[optind], "=") && optind + 2 == argc){
			snprintf(line, sizeof(line), "%s=%s", argv[optind], argv[optind+1]);
			if(do_set(line)){
				ret=1;
			}
			hdb_close();
			return ret;
		}
#endif
		while (optind < argc) {
			if(do_set(argv[optind++])){
				ret=1;
			}
		}
	}
	else if(!isatty(fileno(stdin))){
		while(fgets(line,sizeof(line),stdin)){
			line[strlen(line)-1]=0;
			if(do_set(line)){
				ret=1;
			}
		}
	}
	else {
		usage();
	}
	hdb_close();
	
	return ret;
}
	

int do_set(char *target){
	char key[MAXLENGTH];
	char value[MAXLENGTH];
	char list[MAXLENGTH];

	if(!hdb_cut(target, MAXLENGTH, list, key, value)){
		if(!strcmp(key,"") && !strcmp(value,"")){
			if(!hdb_create_list(list)){
				return 0;
			}
		}
	   	else if(!hdb_set_val(list, key, value)){
			return 0;
		}
	}
	return 1;
}


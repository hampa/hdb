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

#define CMD_VERSION "$Id: hdb-find.c,v 1.1 2006/03/12 13:32:13 hampusdb Exp $"

//#define DEBUG 1
#ifdef DEBUG
#define DBG(fmt...) if (1) fprintf(stderr,fmt)
#else
#define DBG(fmt...)
#endif

void print_output(time_t now, time_t mt, time_t at, char *list, char *key, char *value);
int do_find(char *hdbpath, int format);
void parse_time(int *flag, int *tim, char *arg);
enum {NONE,EQ,LE,GT};

//make options avail 
int mflag=0;
int fflag=0;
int aflag=0;
int oflag=0;
int lflag=0;

//globals used in functions
int outputfmt=0; //output all
int atime=0;
int mtime=0;
int rflag=0;
int hdb_net=0;

void usage(){
	fprintf(stderr, "hdb-find %s\n", CMD_VERSION);
	fprintf(stderr,
		"Usage hdb-find [switch \"args ..\"] list/key\n"
		"   -m <mtime>    Only print if last modified was n minutes ago\n"
		"   -a <atime>    Only print if last accessed was n minutes ago\n"
		"   -l            Check list time instead of key times\n"
		"   -o [amr]      Output (m=No mtime a=No atime r=relative)\n"
		"   -r <DIR>      Use DIR as hdb root\n"
		"   -h            Show this help\n");
	exit(1);
}

//File was last accessed n minutes ago.
int main(int argc, char **argv){
	char line[MAXLENGTHX2];
	char hdbroot[MAXLENGTH];
	FILE *f = stdin;
	int op = 0;
#if HDB_NET_SWITCH
	int found_lock=0;
#endif
	
	outputfmt = (HDB_OUTPUT_LIST | HDB_OUTPUT_KEY | HDB_OUTPUT_VALUE | HDB_OUTPUT_ATIME | HDB_OUTPUT_MTIME);

	while ((op = getopt(argc, argv, "lm:a:r:o:h")) != EOF){
		switch(op){
		case 'h':
			usage();
			break;
		case 'r':
			rflag++;
			strncpy(hdbroot, optarg, sizeof(hdbroot));
			break;
		case 'o':
			//printf("optarg = %s\n", optarg);

			if(strstr(optarg, "a")){
				outputfmt &= ~HDB_OUTPUT_ATIME;
			}
			if(strstr(optarg, "m")){
				outputfmt &= ~HDB_OUTPUT_MTIME;
			}
			if(strstr(optarg, "r")){
				outputfmt |= HDB_OUTPUT_RELTIME;
			}
			break;
		case 'a':
			parse_time(&aflag, &atime, optarg);	
			break;
		case 'm':
			parse_time(&mflag, &mtime, optarg);	
			break;
		case 'l':
			outputfmt &= ~HDB_OUTPUT_KEY;
			outputfmt &= ~HDB_OUTPUT_VALUE;
			//outputfmt |= HDB_OUTPUT_LIST;
			lflag++;
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
                return execvp("lhdb-find", argv);
        }
        //if the daemon is running. Use it.
        if(hdb_net == HDB_ACCESS_FILE && !found_lock){
                return execvp("nhdb-find", argv);
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
			do_find(argv[optind++], outputfmt);
		}
	}
	else if(!isatty(fileno(stdin))){
		while(fgets(line,sizeof(line),f)){
			line[strlen(line)-1]=0;
			do_find(line, outputfmt);
		}
	}
	else {
		usage();
	}
	
	return 0;
}
	

int do_find(char *target, int format){
	//HDBC *hdbc;
	int cursor = HDB_FIRST;
	HDB hdbp;
	HDBS stat;
	char key[MAXLENGTH];
	char value[MAXLENGTH];
	char list[MAXLENGTH];
	//char cmd[MAXLENGTH];
	int ret=0;
	int now = time(NULL);

	DBG("do_find target=%s\n", target);

	//TODO.. move this to lib iwth a hdbd_open routine
	memset(&hdbp,0, sizeof(HDB));
	strcpy(hdbp.root, "/var/db/hdb/");
	hdbp.file=stdout;

	//dont update atime 
	if(hhdb_set_configi(&hdbp, HDB_CONFIG_STAT, HDB_DISABLE)){
		return 1;
	}

	//prase out input
	if(hdb_cut(target, sizeof(list), list, key, value)){
		return 1;
	}

	//only print list stat
	if(lflag){
		if(!(ret=hdb_list_stat(list, &stat))){
			print_output(now, stat.mtime, stat.atime, target, "", "");
		}
	}
	//no key present.. print all values from list 
	else if(!strcmp(key, "")){
		while((hdb_get_cur(list, cursor, key, value))==0){
			cursor = HDB_NEXT;
			if(!hdb_stat(list,key,&stat)){
				print_output(now, stat.mtime, stat.atime, target, key, value);
			}
		}
	}
	//fetch single value list
	else if(!(hhdb_get_nval(&hdbp,list, key, sizeof(value), value))){
		//fetch stat 
		if(!(ret=hdb_stat(list,key,&stat))){
			print_output(now, stat.mtime, stat.atime, list, key, value);
		}
	}
	//some error
	else {
		ret=1;	
	}

	return ret;
}

void print_output(time_t now, time_t mt, time_t at, char *list, char *key, char *value){
	//printf("(now-mt)/60=%li mtime=%i mtime/60=%i\n", (now-mt)/60, mtime, mtime/60);
	if( (!mflag && !aflag) || 			//no flags just print
	    (mflag == GT && (now - mt > mtime)) ||
	    (mflag == EQ && ((now - mt)/60 == mtime/60)) ||
	    (mflag == LE && (now - mt < mtime)) ||
	    (aflag == GT && (now - at > atime)) ||
	    (aflag == EQ && ((now - at)/60 == atime/60)) ||
	    (aflag == LE && (now - at < atime)))
	{
		hdb_print_output(stdout, outputfmt, at, mt, list, key, value);
	}
}

void parse_time(int *flag, int *tim, char *arg){
	assert(arg);
	if(*arg =='+')
		*flag=GT;
	else if (*arg=='-')
		*flag=LE;
	else if (*arg=='=')
		*flag=EQ;
	else 
		usage();
	*tim = atoi(arg+1) * 60;
}


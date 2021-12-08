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

#define CMD_VERSION "$Id: hdb-cut.c,v 1.1 2006/03/12 13:32:12 hampusdb Exp $"

//#define DEBUG 1
#ifdef DEBUG
#define DBG(fmt...) if (1) fprintf(stderr,fmt)
#else
#define DBG(fmt...)
#endif

int do_cut(int format, char *sformat, char *hdbpath);
void lkv_printf(char *fmt, char *list, char *key, char *value);

void usage(){
	fprintf(stderr, "hdb-cut %s\n", CMD_VERSION);
	fprintf(stderr,
		"Usage hdb-cut [switch \"args ..\"] list/key=value\n"
		"   -o [lkv]      output l=list, k=key, v=value\n"
		"   -p format     printf format %%l %%v %%k\n"
		"   -h            Show this help\n"
		);
	exit(1);
}

int pflag=0;

//File was last accessed n minutes ago.
int main(int argc, char **argv){
	char line[MAXLENGTHX2];
	char format[MAXLENGTH];
	int op = 0;
	int ret = 0;
	int outputfmt=0;
	
	strcpy(format, "");	
	while ((op = getopt(argc, argv, "p:o:h")) != EOF){
		switch(op){
		case 'h':
			usage();
			break;
		case 'o':
			outputfmt=0;
			if(strstr(optarg, "l")){
				outputfmt |= HDB_OUTPUT_LIST;
			}
			if(strstr(optarg, "k")){
				outputfmt |= HDB_OUTPUT_KEY;
			}
			if(strstr(optarg, "v")){
				outputfmt |= HDB_OUTPUT_VALUE;
			}
			break;
		case 'p':
			pflag++;
			strncpy(format, optarg, sizeof(format));
			break;
		default:
			usage();
			break;
		}	
	}


	if (optind < argc) {
		while (optind < argc) {
			if(do_cut(outputfmt, format, argv[optind++])){
				ret=1;
			}
		}
	}
	else if(!isatty(fileno(stdin))){
		while(fgets(line,sizeof(line),stdin)){
			line[strlen(line)-1]=0;
			if(do_cut(outputfmt, format, line)){
				ret=1;
			}
		}
	}
	else {
		usage();
	}
	
	return ret;
}

int do_cut(int format, char *sformat, char *target){
	char key[MAXLENGTH];
	char value[MAXLENGTH];
	char list[MAXLENGTH];

	DBG("do_cut list=%s key=%s value=%s\n", list, key, value);

	if(hdb_cut(target, MAXLENGTH, list, key, value)){
		return 1;
	}
	if(pflag){
		lkv_printf(sformat, list, key, value);
	}
	else {
		hdb_print_output(stdout, format, 0, 0, list, key, value);
	}

	return 0;
}

void lkv_printf(char *fmt, char *list, char *key, char *value){
	char *p=fmt;
	int i=0;
	char *s=NULL;
	char output[MAXLENGTHX2];

	//char *fmt="key=%k value=%v value=%v list=%l\n";
	*output='\0';

	while(*p){
		s=NULL;
		if(*p == '%' && p[1] == 'k')
			s=key;
		else if(*p== '%' && p[1]=='l')
			s=list;
		else if(*p== '%' && p[1]=='v') 
			s=value;
		
		if(s){
			do { 
				output[i++]=*s;
			}
			while(*++s);
			p+=2;
		}
		else {
			output[i++]=*p++;
		}
	}
	output[i]=0;
	printf("%s\n", output);
}


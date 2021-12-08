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
#include <libgen.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "hdb.h"

void _time_print( char *buf, int flags, time_t t);

void _time_print(char *buf, int flags, time_t t){
        struct tm result;

	assert(buf);

	if(flags == 0){ //output IS08601
        	localtime_r(&t, &result);
        	//ISO8601 YYYY-MM-DD and hh:mm:ss
        	snprintf(buf, MAXLENGTH, "%02i-%02i-%02i %02i:%02i:%02i",
        	result.tm_year % 100, result.tm_mon, result.tm_mday,
        	result.tm_hour, result.tm_min, result.tm_sec);
	}
	else { //output minutes since now
		snprintf(buf, MAXLENGTH, "%li", (time(NULL) - t)/60);
	}
}

void hdb_print_output(FILE *f, int format, int atime, int mtime, char *list, char *key, char *value){
	int lflag = 1; 
	int kflag = 1; 
	int vflag = 1; 
	int taflag = 0;
	int tmflag = 0;
	int rflag = 0;
	int didprint = 0;
	int len=0;
	char abuf[MAXLENGTH];
	char mbuf[MAXLENGTH];

	assert(list);
	assert(key);
	assert(value);

//$1 = {tm_sec = 11, tm_min = 8, tm_hour = 3, tm_mday = 14, tm_mon = 10, tm_year = 105, tm_wday = 1, tm_yday = 317, tm_isdst = 0, tm_gmtoff = 0,
 // tm_zone = 0x83166e "GMT"}

	if(format){
		lflag = format & HDB_OUTPUT_LIST;
		kflag = format & HDB_OUTPUT_KEY;
		vflag = format & HDB_OUTPUT_VALUE;
		taflag = format & HDB_OUTPUT_ATIME;
		tmflag = format & HDB_OUTPUT_MTIME;
		rflag = format & HDB_OUTPUT_RELTIME;
	}

	if(!strcmp(list, "") && !strcmp(key, "") && !strcmp(value, "")){
		return;
	}

	//print any meta information output
	if(taflag && tmflag){
		_time_print(mbuf, rflag, mtime);
		_time_print(abuf, rflag, atime);
		didprint++;
		fprintf(f, "[%s, %s] ", mbuf, abuf); 
	}
	else if(taflag){
		didprint++;
		_time_print(abuf, rflag, atime);
		fprintf(f, "[%s] ", abuf); 
	}
	else if (tmflag){
		didprint++;
		_time_print(mbuf, rflag, mtime);
		fprintf(f, "[%s] ", mbuf); 
	}

	if(lflag){
		len = strlen(list);
		if(list[len-1] == '/'){
			fprintf(f, "%s", list);		
		
		}
		else {
			fprintf(f, "%s/", list);		
		}
		didprint++;
	}	
	if(kflag && strcmp(key, "")){
		didprint++;
		fprintf(f,"%s", key);
	}
	if(vflag){
		if(lflag || kflag){
			didprint++;
			fprintf(f,"=");
		}
		if(strcmp(value, "")){
			didprint++;
			fprintf(f, "%s", value);
		}
	}
	if(didprint){
		fprintf(f,"\n");
	}
}

int hdb_cut(const char *lkv, int size, char *list, char *key, char *value){
        char *l=NULL;
        char *k=NULL;
        char *eq=NULL;
        int klen=0;
	int lkvlen=0;

	assert(list && key && value);
	strcpy(list,"");
	strcpy(key, "");
	strcpy(value, "");

	lkvlen=strlen(lkv);
	
	//dont return an error on zero input
	if(lkvlen == 0){
		return 0;
	}

	// Ending with / and have no =
	// list/sublist/ -> list=list/sublist/ key= value=
	if(lkv[lkvlen-1] == '/' && !strstr(lkv, "=")){
		strcpy(list, lkv);	
		return 0;
	}

	// Starting with = and no / 
	// =value -> list= key= value=value
	if(lkv[0] == '=' && !strstr(lkv, "/")){
		strcpy(value, lkv+1);
		return 0;
	}

        l = strdup(lkv);
        snprintf(list, size, "%s/", dirname(l));
        if(!strcmp(list, "./")){
                strcpy(list, lkv);
        }
	else {
        	k = strdup(lkv);
        	snprintf(key, size, "%s", basename(k));
        	if((eq=strstr(key, "=")) || (eq=strstr(key, " "))){
                	strcpy(value, eq+1);
                	klen=eq-key;
                	key[klen]=0;
        	}
	}
        free(l);
        free(k);

	return 0;
}


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
#define HDB_NET 1
#include "hdb.h"
#include "urlencode.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#define MAXDATASIZE  512 // max number of bytes we can get at once 

#ifdef DEBUG
#define DBG(fmt...) if (1) fprintf(stderr,fmt)
//#define DBG(fmt...) if (1) fprintf(stdout,fmt)
#else
#define DBG(fmt...)
#endif

int use_trigger=1; 
int server;
int connected=0;
int neterrcode=0;
int hdberrno=0;
int is_ready(int fd);

void print_cache(){}

int is_ready(int fd){
	int rc;
	fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(fd,&fds);
	tv.tv_sec = HDB_NET_TIMEOUTSEC;
	tv.tv_usec = 0;
	
	rc = select(fd+1, &fds, NULL, NULL, &tv);
	if(rc < 0){
		return -1;
	}

	return FD_ISSET(fd, &fds) ? 1 : 0;
}

//return 0 on success
int hdb_connect(const char *host, int port, char *user, char *pwd,  int crypt){
	//int sockfd, 
	int byte=0;
	long arg;
	int res;
	int bytes_read=0;
	int size=0;
	char request[MAXDATASIZE];
	struct hostent *he;
	struct sockaddr_in their_addr; // connector's address information 

	if ((he=gethostbyname(host)) == NULL) {  // get the host info 
		//perror("gethostbyname");
		return 1;
	}

	if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		//perror("socket");
		return 2;
	}

	their_addr.sin_family = AF_INET;    // host byte order 
	their_addr.sin_port = htons(port);  // short, network byte order 
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	memset(&(their_addr.sin_zero), '\0', 8);  // zero the rest of the struct 

	// set non-blocking
	if(( arg = fcntl(server, F_GETFL, NULL)) < 0){
		perror("fcntl");
		return 1;
	}

	arg |= O_NONBLOCK;
	if( fcntl(server, F_SETFL, arg) < 0 ){
		perror("fcntl");
		return 1;
	}

	// connect with timeout
	res = connect(server, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
	if(res < 0){
		if(errno == EINPROGRESS){
			if(!is_ready(server)){
				return HDB_NETWORK_ERROR;
			}
		}
	}
	
	// turn back blocking mode again
	if((arg = fcntl(server, F_GETFL, NULL)) < 0){
		perror("fcntl");
		return HDB_NETWORK_ERROR;
	}

	arg &= ~O_NONBLOCK;

	if(fcntl(server, F_SETFL, arg) < 0){
		perror("fcntl");
		return HDB_NETWORK_ERROR;
	}

	while(byte != '\n'){
		if((size = recv(server, &byte, 1, 0)) <= 0){
			DBG("woot\n");
			return 4;
		}
		if(bytes_read<MAXDATASIZE){
			request[bytes_read++] = byte;
		}
	}
	request[bytes_read-1] = '\0';
	DBG("got request %s\n", request);
	if (strncmp(request, "#WELCOME", 8)){
		DBG("did not match exiting\n");
		close(server);
		return 5;
	}
  
	if(getenv("HDBROOT")){
	  	if(hdb_set_root(getenv("HDBROOT"))){
		  	DBG("ERR Unable to set root\n");
			close(server);
			return 5;
	  	}
  	}	

	connected=1;
	return 0;
}	

int hdb_close(){
	return 0;
}

int hdb_disconnect(){
	hdb_net_set("QUIT\n");
	connected=0;
	return close(server);
}	
int hdb_set_user(char *user){ 
	return 0;
}

int hdb_get_access(){
	return HDB_ACCESS_NETWORK;
}

int hdb_open_log(int facility, char *name){

	switch(facility){	
	case HDB_LOG_CONSOLE: 	return hdb_net_set("LOG CONSOLE\n");
	case HDB_LOG_SYSLOG: 	return hdb_net_set("LOG SYSLOG\n");
	case HDB_LOG_FILE: 	return hdb_net_set("LOG FILE\n");
	default: break;
	}
	return HDB_ERROR;	
}

int hdb_print_log(const char *format, ...){
	char request[MAXDATASIZE];

	va_list ap;

	va_start(ap, format);
	snprintf(request, sizeof(request), "LOG PRINT %s %s", format, ap);
	return hdb_net_set(request);
}
int hdb_close_log(){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "LOG NONE\n");
	return hdb_net_set(request);
}

int hdb_lock(const char *list, int timeout){
	return 0; //TODO
}
int hdb_unlock(const char *list){
	return 0;
}
//commands must end with \n

//use this for setting.. returns 0 if #OK
int hdb_net_set(const char *cmd){
	char request[MAXDATASIZE];
	int byte=0;
	int bytes_read=0;
	int size=strlen(cmd);
	int errcode=0;
	
	if(size>MAXDATASIZE){
		DBG("hdb_net_set ERR request to big %i>512\n",size); 
		return 1;	
	}

	DBG("hdb_net_set sending line #%s#", cmd); 
	if(send(server, cmd, size, 0)== -1){
		perror("hdb_net_set");
		return 12;
	}

	while(1){
		byte=bytes_read=0;

		// check if timeout
		if(!is_ready(server)){
			return HDB_NETWORK_ERROR;
		}

		while(byte != '\n'){
			if((size = recv(server, &byte, 1, 0)) <= 0){
				DBG("error\n");
				return 11;
			}
			if(byte==0){
				DBG("nullbyte\n");
				continue;		
			}
			if(bytes_read<128){
				request[bytes_read++] = byte;
			}
			//DBG("bytes_read=%i %c %i\n", bytes_read, byte, byte);
			//keep on reading until we get \n
			//but only save first 128 bytes
		}
		request[bytes_read-1] = '\0';
		DBG("hdb_net_set got response #%s#\n", request);
		if (!strncmp(request, "#ERR", 4)){
			DBG("in=%sout=%s\n", cmd, request);
			if(bytes_read != 4){
				errcode = atoi(request+4);
				if(errcode > 0){
					return errcode;
				}
				return 1;
			}
			return 1;
		}
		else if(!strncmp(request, "#OK", 3)){
			return 0;
		}
		else if(!strncmp(request, "#BYE", 4)){
			return 0;
		}
		else if (!strncmp(request, "#", 1)){ //debug info
			continue;
		}
		else {
			//what to do? your breaking the rules
			continue;
		}
	}
	DBG("hdb_net_set failed\n");
	return 1;
}

/* hdb_net_print_fmt()
 * 
 * Enhanced version of hdb_net_print(), which allows "bash friendly"
 * output.
 */
int hdb_net_print_fmt(FILE *fd, const char *cmd, int bash_friendly /* 0 = no, !0 = yes */){

	char request[1024];
	char byte=0;
	int bytes_read=0;
	int size=strlen(cmd);
	int hdberrno=0;

	if(size>MAXDATASIZE){
		DBG("hdb_net_print ERR request to big %i>512\n",size); 
		return 1;	
	}

	DBG("hdb_net_print - sending request: %s", cmd);
	if(send(server, cmd, size, 0)== -1){
		perror("send");
		return 1;
	}
	while(1){
		byte=bytes_read=0;
		// check if timeout
		if(!is_ready(server)){
			return HDB_NETWORK_ERROR;
		}

		while(byte != '\n'){
			if((size = recv(server, &byte, 1, 0)) <= 0){
				DBG("hdb_net_print - ERR pipe closed\n");
				return 2;
			}
			DBG("after bytes recv %i got %c\n", size, byte);
            		if(bytes_read>MAXDATASIZE){
				DBG("hdb_net_print - ERR bytes_read>512\n");
                		return 3;
            		}
			request[bytes_read++] = byte;
		}
		request[bytes_read-1] = '\0';
		if(bytes_read > MAXDATASIZE){
			DBG("hdb_net_print - bytes_read > %i\n", MAXDATASIZE);
			return HDB_ERROR;
		}
		else if (!strncmp(request, "#ERR", 4)){
			if(bytes_read != 5){
				hdberrno = atoi(request+5);
				assert(hdberrno>0);
				DBG("hdb_net_print - got error code %i\n", hdberrno);
				return hdberrno;
			}
			return HDB_ERROR;
		}
		else if(!strncmp(request, "#OK", 3)){
			break;
		}
		else if(!strncmp(request, "#BYE", 4)){
			DBG("hdb_net_print - INFO server said #BYE\n");
			return 0;
		}
		else if(!strncmp(request, "#", 1)){
			continue;
		}
		
		if(bash_friendly != 0) {
			char *first_space;
			if((first_space = strchr(request, ' ')) != NULL) {
				*first_space = '=';
			}
		}
		
		fprintf(fd, "%s\n", request);
	}
	return 0;
}
int hdb_net_print(FILE *fd, const char *cmd){
	return hdb_net_print_fmt(fd, cmd, 0);
}

int hdb_dump_glob(char *list, char *key, char *value){
	return hhdb_dump_glob(NULL, list, key, value);
}

int hhdb_dump_glob(HDB *hdbp, char *list, char *key, char *value){
	char cmd[MAXLENGTH];
	snprintf(cmd, MAXLENGTH, "DUMPFLATGLOB \"%s\" \"%s\" \"%s\"\n", list, key, value); 
	return hdb_net_print(stdout, cmd);
}

int hdb_list_stat(const char *list, HDBS *stat){
	return hhdb_list_stat(&hdb_default, list, stat);
}

int hhdb_list_stat(HDB *hdbp, const char *list, HDBS *stat){
	return 1; 
}

int hdb_stat(const char *list, const char *key, HDBS *stat){
	return hhdb_stat(&hdb_default, list, key, stat);
}

int hhdb_stat(HDB *hdbp, const char *list, const char *key, HDBS *stat){
	char cmd[MAXLENGTH];
	char *value=NULL;

	stat->mtime = stat->atime = -1;

	if(!strcmp(key, "")){
		snprintf(cmd, MAXLENGTH, "STAT \"%s\"\n", list); 
	}
	else {
		snprintf(cmd, MAXLENGTH, "STAT \"%s\" \"%s\"\n", list, key); 
	}
	if((value = hdb_net_get(cmd)) == NULL){
		return 1;
	}
	sscanf(value, "%li %li", &stat->mtime, &stat->atime);	
	free(value);
	return 0;
}

int hdb_islist(const char *list){
	return 1;
}

int hdb_dump_regex(char *list, char *key, char *value){
	return hhdb_dump_regex(NULL, list, key, value);
}

int hhdb_dump_regex(HDB *hdbp, char *list, char *key, char *value){
	char cmd[MAXLENGTH];
	snprintf(cmd, MAXLENGTH, "DUMPFLATREGEX \"%s\" \"%s\" \"%s\"\n", list, key, value);
	return hdb_net_print(stdout, cmd);
}

HDBC *hhdb_copen(HDB *hdbp, HDBC *hdbc, char *list){
	return NULL;
}
HDBC *hdb_copen(HDBC *hdbc, char *list){
	return NULL;
}


int hdb_cget(HDBC *hdbc, char *list, char *value){
	return 1;
}

int hdb_cclose(HDBC *hdbc){
	return 1;
}

int hhdb_get_nval(HDB *hdbp, const char *list, const char *key, int size, char *val){
	return hdb_get_nval(list, key, size, val);
}

//will returned malloced char
char *hdb_net_get(const char *cmd){
	return hhdb_net_get(NULL, cmd); //dont update global.. this OK?
}

char *hhdb_net_get(HDB *hdbp, const char *cmd){
	char request[MAXLENGTH];
	char value[2048];
	char *val_decoded;
	char byte=0;
	int bytes_read=0;
	int size =strlen(cmd);

	//reset global error code
	hdberrno=0;

	if(size>MAXDATASIZE){
		DBG("hdb_net_get ERR request to big %i>512\n",size); 
		hdberrno=HDB_INVALID_INPUT;
		return NULL;	
	}
	DBG("hdb_net_get() sending cmd #%s#", cmd);
	if(send(server, cmd, size, 0)== -1){
		perror("send");
		hdberrno=HDB_NETWORK_ERROR;
		return NULL;
	}
	strcpy(value, "");
	while(1){
		//printf("waiting for data...\n");
		byte=bytes_read=0;
		// check if timeout
		if(!is_ready(server)){
			hdberrno=HDB_NETWORK_ERROR;
			return NULL;
		}
		while(byte != '\n'){
			if((size = recv(server, &byte, 1, 0)) <= 0){
				//the closed
				DBG("hdb_net_get ERR - got -1 from recv\n");
				hdberrno=HDB_NETWORK_ERROR;
				return NULL;
			}
            		if(bytes_read>1024){
				DBG("hdb_net_get ERR - bytes_read > 1024\n");
				hdberrno=HDB_NETWORK_ERROR;
                		return NULL;
            		}
			request[bytes_read++] = byte;
		}
		request[bytes_read] = '\0';
		DBG("hdb_net_get got response #%s#\n", request);
		if(bytes_read > 1024){
			DBG("hdb_net_get ERR - bytes_read > 1024\n");
			hdberrno=HDB_ERROR;
			return NULL;
		}
		else if (!strncmp(request, "#ERR", 4)){
			DBG("hdb_net_get ERR - got #ERR response\n");
			if(bytes_read != 5){
				hdberrno = atoi(request+5);
				assert(hdberrno>0);
			}
			return NULL;
		}
		else if(!strncmp(request, "#OK", 3)){
			break;
		}
		else if(!strncmp(request, "#META", 5)){
			if(hdbp!=NULL && bytes_read != 6){
				sscanf(request+6, "%li %li", &hdbp->mtime, &hdbp->atime);	
			}
		} 
		else if(!strncmp(request, "#BYE", 4)){
			return "";
		}
		else if(!strncmp(request, "#", 1)){
			continue;
		}
		//TODO.. value must be dynamic
		strncat(value, request, 1024);
	}
	DBG("hdb_net_get size=%i value='%s'\n", strlen(value), value);
	value[strlen(value)-1] = '\0'; //remove last \n

	val_decoded = malloc(strlen(value)+1);
	strcpy(val_decoded, "");
	urldecode(val_decoded, value);
	return val_decoded;
}

int hdb_mv( const char *list, const char *dest){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "MOVE %s %s\n", list, dest);
	return hdb_net_set(request);
}

int hdb_get_nval( const char *list, const char*key, int size, char *val){

	char *value = NULL;

	if((value = hdb_get_val(list, key)) == NULL){
		strcpy(val, "");
		assert(hdberrno>0);
		return hdberrno;
	}

	strncpy(val, value, size);
	free(value);

	return  0;
}

char *hdb_get_pval( const char *list, const char *key){
    static char buf[255];
    hdb_get_nval(list, key, 255, buf);
    return buf;
}

char *hdb_get_val( const char *list, const char*key){
	char request[MAXDATASIZE];
	if(key==NULL)
		return NULL;
	snprintf(request, sizeof(request), "GET %s \"%s\"\n", list, key);
	return hdb_net_get(request);
}

int hdb_set_root(const char *root){
	char request[MAXDATASIZE];
	
	int i = 0;
	snprintf(request, sizeof(request), "ROOT \"%s\"\n", root);
	i = hdb_net_set(request);
	//printf("setting root %s returned(%i)\n", root, i);
	return i;

}

char *hdb_get_root(){
    return hdb_net_get("ROOT\n");
}


char *hdb_get_raw( const char *list, const char *key, int *type){
	char request[MAXDATASIZE];
	char *result=NULL;
	
	snprintf(request, sizeof(request), "GETRAW %s %s\n", list, key);
	if((result = hdb_net_get(request))==NULL){
		type=0;
		return NULL;
	}
	sscanf(result, "%i %*s", type);
	return result;
}

int hdb_set_link( const char *list, const char *key, const char *link){
	return hdb_set_raw(list, key, link, HDB_TYPE_LINK);
}

int hdb_set_exec( const char *list, const char *key, const char *cmd){
	return hdb_set_raw(list, key, cmd, HDB_TYPE_EXEC);
}

int hdb_set_file(const char *list, const char *key, const char *file){
	return hdb_set_raw(list, key, file, HDB_TYPE_FILE);
}

int hdb_get_bool( const char *list, const char *key){

	char bufval[5];

	if(hdb_get_nval(list, key, 5, bufval) == 0){
		if((!strcasecmp(bufval, "true")) || 
		   (!strcasecmp(bufval, "yes")) || 
		   (!strcasecmp(bufval, "on")) ||
		   atoi(bufval)){
			return 1;
		}
	}

	return 0;
}

int hdb_get_int( const char *list, const char*key){

	char *value = NULL;
	int ivalue = 0;

	if((value = hdb_get_val(list, key)) != NULL){
		ivalue = atoi(value);
		free(value);
	}
	return ivalue;

}

int hdb_set_int( const char *list, const char *key, int val){
	char value[MAXLENGTH];

	snprintf(value, sizeof(value), "%i", val);
	return hdb_set_val(list, key, value);

}

long long hdb_get_long( const char *list, const char*key){

	char *value = NULL;
	long long ivalue = 0;

	if((value = hdb_get_val(list, key)) != NULL){
		ivalue = atoll(value);
		free(value);
	}
	return ivalue;

}



int hdb_set_long( const char *list, const char *key, long long val){
	char value[MAXLENGTH];

	snprintf(value, sizeof(value), "%lld", val);
	return hdb_set_val(list, key, value);

}

int hdb_add( const char *list, const char *key, int amount){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "ADD %s %s %i\n", list, key, amount);
	return hdb_net_set(request);
}

int hdb_incr( const char *list, const char *key){
	return hdb_add(list, key, 1);
}

int hhdb_incr(HDB *hdbp, const char *list, const char *key){
	return hdb_add(list, key, 1);
}

int hdb_sync(){
	return hdb_net_set("SYNC\n");
}

int hdb_set_val( const char *list, const char *key, const char *val){
	return hdb_set_raw(list, key, val, HDB_TYPE_STR);
}

int hdb_set_raw( const char *list, const char *key, const char *val, int type){
	char request[MAXDATASIZE];
	char val_encoded[MAXLENGTH*3+1];
	if(!urlencode(val_encoded, val)){
		return HDB_INVALID_INPUT;
	}

	snprintf(request, sizeof(request), "SETRAW %s %s \"%s\" %i\n", list, key, val_encoded, type);
	return hdb_net_set(request);
}

int hdb_wipe(){
	return hdb_net_set("WIPE\n");
}
int hdb_del_val(const char *list, const char *key){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "DEL %s %s\n", list, key);
	return hdb_net_set(request);
}

int hdb_get_sublist_full( const char *parent_list, int recno, char *list){
	char request[MAXDATASIZE];
	char *l=NULL;
	snprintf(request, sizeof(request), "GETSUBLISTFULL %s %i\n", parent_list, recno);
	if((l = hdb_net_get(request))==NULL){
		strcpy(list, "");
		return 1;
	}
	strcpy(list, l);
	free(l);
	return 0;
}

int hdb_get_sublist( const char *parent_list, int recno, char *list){
	char request[MAXDATASIZE];
	char *l=NULL;
	snprintf(request, sizeof(request), "GETSUBLIST %s %i\n", parent_list, recno);
	if((l = hdb_net_get(request))==NULL){
		strcpy(list, "");
		return 1;
	}
	strncpy(list, l, 255);
	free(l);
	return 0;
}

int hdb_set_configi(int parameter, int value){
	return hhdb_set_configi(&hdb_default, parameter, value);
}

int hhdb_set_configi(HDB *hdbp, int parameter, int value){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "CONFIG %i %i\n", parameter, value);
	return hdb_net_set(request);
}

int hdb_get_configi(int parameter){
	return hhdb_get_configi(&hdb_default, parameter);	
}

int hhdb_get_configi(HDB *hdbp, int parameter){
	char request[MAXDATASIZE];
	char *l=NULL;
	int value = 0;
	snprintf(request, sizeof(request), "CONFIG %i\n", parameter);
	if((l = hdb_net_get(request)) == NULL){
		return -1;
	}
	value = atoi(l);
	free(l);
	return value;
	
}

//TODO.. how to implement this over network?
HDBC *hdb_sublist_copen(HDBC *hdbc, char *parent_list){
	return NULL;
}

int hdb_sublist_cget(HDBC *hdbc, char *list, int size){
	return 1;
}

int hdb_sublist_cget_full(HDBC *hdbc, char *list, int size){
	return 1;
}

int hdb_sublist_cclose(HDBC *hdbc){
	return 1;
}

int hdb_scan_sublist(const char *parent_list, char ***lists){
	return -1;
}

int hdb_scan_sublist_full(const char *parent_list, char ***lists){
	return -1;
}

int hdb_scan_sublist_close(char **list){
	return 1;
}

int hdb_get_sublist_cur_full( const char *parent_list, int recno, char *list){
	char request[MAXDATASIZE];
	char *l=NULL;
	snprintf(request, sizeof(request), "GETSUBLISTCURFULL \"%s\" %i\n", parent_list, recno);
	if((l = hdb_net_get(request))==NULL){
		strcpy(list, "");
		return 1;
	}
	strncpy(list, l, 255);
	free(l);
	return 0;
}

// only support for DB_FIRST, DB_NEXT, DB_CLOSE
int hdb_get_sublist_cur( const char *parent_list, int cursor, char *list){
	char request[MAXDATASIZE];
	char *l=NULL;
	snprintf(request, sizeof(request), "GETSUBLISTCUR \"%s\" %i\n", parent_list, cursor);
	if((l = hdb_net_get(request))==NULL){
		strcpy(list, "");
		return 1;
	}
	strncpy(list, l, 255);
	free(l);
	return 0;
}

int hdb_get_rec( const char *list, int recno, char *key, char *val){
	char request[MAXDATASIZE];
	char *value=NULL;	
	snprintf(request, sizeof(request), "GETREC %s %i\n", list, recno);
	strcpy(key, "");
	strcpy(val, "");
	if(!(value = hdb_net_get(request))){
		return 1;
	}
	sscanf(value, "%s %s", key, val);	
	free(value);
	return 0;
}

int hdb_get_cur(const char *list, int cursor, char *key, char *val){
	char request[MAXDATASIZE];
	char *value=NULL;	
	snprintf(request, sizeof(request), "CUR %s %i\n", list, cursor);
	strcpy(key, "");
	strcpy(val, "");
	if(!(value = hdb_net_get(request))){
		assert(hdberrno>0);
		return hdberrno;
	}
	sscanf(value, "%s %s", key, val);	
	free(value);
	return 0;
}

int hdb_key_exist(const char *list, const char *key){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "EXIST %s %s\n", list, key);
	return hdb_net_set(request);
}
	

int hdb_exist( const char *list){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "EXIST %s\n", list);
	return hdb_net_set(request);
}

int hdb_get_size( const char *list){
	char request[MAXDATASIZE];
	char *val=NULL;
	int  size = 0;
	snprintf(request, sizeof(request), "SIZE %s\n", list);
	if((val = hdb_net_get(request))==NULL){
		return 0;
	}
	size = atoi(val);
	free(val);
	return size;
}

int hdb_create_list( const char *list){
	char request[MAXDATASIZE];
	snprintf(request, sizeof(request), "CREATE %s\n", list);
	return hdb_net_set(request);
}

int hdb_delete_list( const char *list){
	char request[MAXDATASIZE];
	if(!list){
		return 1;
	}
	snprintf(request, sizeof(request), "REMOVE %s\n", list);
	return hdb_net_set(request);
}

int hdb_trig_disable(int type, const char *list, const char *key){
    use_trigger=0;
    return 0;
}

int hdb_trig_enable(int type, const char *list, const char *key){
    use_trigger=1;
    return 0;
}

#ifdef TAPIR
int main(){
	char *p = NULL;
	int i = 0;
	hdb_connect("127.0.0.1", HDB_PORT , NULL, NULL, 0);
	//hdb_wipe();
	hdb_delete_list("pval");
	printf("INFO - emtpy value %s\n", hdb_get_pval("pval", "pungen"));
	printf("INFO - emtpy list and value %s\n", hdb_get_pval("pvalxxxx", "pungen"));

	return 0;
	i = hdb_set_root("/tsdfasdfmp/asdf");
	printf("%i\n", i);
	return 0;
	hdb_set_val("hej", "yasdf", "adsf");
	hdb_set_val("hej", "yasdf", "adsf");
	hdb_set_val("hej", "yasdf", "adsf");
	p = hdb_get_val("hej", "yasdf");
	printf("got %s\n", p);
	free(p);
	p = hdb_get_val("hej", "yasdf");
	printf("got %s\n", p);
	free(p);

	return 0;
}
#endif

/**********************************************************
hdb_check_lock - return 0 (file is locked)
               - return !0 file is not locked or on other error
TODO.. this code is duplicated in libhdb.c
**********************************************************/
int hdb_check_lock(const char *file){
    struct flock lock;
	int fd;

	if(file == NULL){
		return HDB_INVALID_INPUT;
	}

	if((fd = open(file, O_RDONLY)) == -1) {
		DBG("hdb_lock - unable to open file %s\n", file);
		//perror("hdb_check_lock");
		return HDB_OPEN_ERROR;
	}
	//we only lock with WRITE lock
	//we choose a READ lock to check
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if(!fcntl(fd, F_SETLK, &lock)) {
            DBG("got lock. file is not locked\n");
            return 1;
    }
	return 0;
}

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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <assert.h>
#include <signal.h>
#include <libgen.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <syslog.h>
#include "hdbd.h"
#include "urlencode.h"

#define APP_VERSION "$Id: phdbd.c,v 1.10 2006/05/26 10:54:46 hampusdb Exp $"

//define this for profiling (gcc -pg) compilation
//profiling does not handle threads properly
//#define NOTHREADS 1

int lock(char *file);
int fd_unlock(int fd);
void unlock();
void write_lock();
void read_lock();
//int x_sync(HDB *hdbp);
char *strip(char *);
int hdb_list();

#define OPTS 8
int unescape(char *in, char out[OPTS][HDB_VALUE_MAX]);

int dflag = 0;
int fflag = 0;
int lflag = 0;

#define SOCKSIZE 4096 
void sig_handler(int s);

void * thread_main(void *arg);

typedef struct {
	char hdbroot[HDB_PATH_MAX];
	int requests;
	int state;            /* 0=waiting 1=serving client */
	time_t contime;       /* connection time */
	pthread_t thread_tid; /* thread ID */
	long thread_count;    /* connections handled */
	char socket_buffer[SOCKSIZE];  /* commands that return many rows need a buffer */
	int socket_size;
	int fd;               /* socket fd for this thread */
} Thread;
            /* 0=waiting 1=serving client */
Thread	*tptr = NULL;		/* array of Thread structures; malloc */

int buffered_send(Thread *t,const char*);
int buffered_send_flush(Thread *t,const char*);
int buffered_lock_send_flush(Thread *t,const char*);
int buffered_flush(Thread *t);
int hdb_dump(Thread *t, HDB *hdbp, int level, int start, char *list);
int hdb_dump_flat(Thread *t, HDB *hdbp, int start, char *list);
int hdb_print(Thread *t, HDB *hdbp, char *list, int frmt);
int help(Thread *t, char *cmd);
int commands(Thread *t);

int listenfd;
int nthreads=10;
socklen_t addrlen;
pthread_mutex_t	mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

//TODO.. needs -D_XOPEN_SOURCE=500
//pthread_rwlock_t rwlock; 


int debug=0;
//int connfd=0;
int listenfd=0;
void usage();

#define BACKLOG 10

int help(Thread *t, char *cmd){

	char buf[MAXLENGTHX2];
	int i = 0;

	while(hdb_cmds[i].name){
		if(strcmp(cmd, "")){
			if(!strcasecmp(hdb_cmds[i].name, cmd)){
				snprintf(buf, sizeof(buf), "%s(%i) %s - %s\n#OK\n", 
						hdb_cmds[i].name, 
						hdb_cmds[i].func,
						hdb_cmds[i].param,
						hdb_cmds[i].info);
				if(buffered_send_flush(t,buf)==-1){
					return 1;	
				}
				return 0;
			}
		}
		else {
			snprintf(buf, sizeof(buf), 
					"%s %s\n", hdb_cmds[i].name, hdb_cmds[i].param);
			if(buffered_send_flush(t,buf)==-1){
				return 1;
			}
		}
		i++;
	}
	if(buffered_send_flush(t,"#OK\n")==-1){
		return 1;	
	}
	return 0;
}

int commands(Thread *t){
	int i = 0;
	char buf[MAXLENGTHX2];

	while(hdb_cmds[i].name){
		snprintf(buf, sizeof(buf), "%s ", hdb_cmds[i].name);
		if(buffered_send(t,buf)==-1){
			if(debug){
				fprintf(stderr, "commands() - sending commands failed.. exiting\n");
			}
			return 1;
		}
		i++;
	}
	if(buffered_send_flush(t,"\n")==-1){
		if(debug){
			fprintf(stderr, "commands() - socket error\n");
			
		}
		return 1;
	}	
	return 0;
}

int find_command(char *cmd){
	int i = 0;
	int hit = 0;
	int match = 0;
	while(hdb_cmds[i].name){
		if (strlen(cmd) == strlen(hdb_cmds[i].name) && 
				!strcasecmp(cmd, hdb_cmds[i].name))
		{
			hit=1;
			match=i;
			break;
		}
		else if(!strncasecmp(hdb_cmds[i].name, cmd, strlen(cmd))){
			hit++;
			match=i;
		}
		//extra functions that should not show in help
		//debug stuff
		/*
		else if(!strcasecmp(cmd, "status")){
			return HDB_STATUS;
		}
		else 
		*/
		else if(atoi(cmd) == HDB_PORT){ 
			return HDB_PORT;
		}

		i++;
	}
	if(hit==1){
		return hdb_cmds[match].func;		
	}
	return HDB_NONE;
}

void sig_chld(int s) {
	if(debug){
		fprintf(stderr, "%p sig_child() - got signal %i\n", (void*)pthread_self(),s);
	}
}

// not used any more
void kill_threads(){
	int i;
	for(i=0;i<nthreads;i++){
		if(debug){
			fprintf(stderr, "%p kill_threads() - killing thread no %i\n", (void*)pthread_self(), i);
		}
		pthread_kill(tptr[i].thread_tid, SIGUSR1);
	}
}

void sig_handler(int s){
	HDB hdbp;

	if(debug){
		fprintf(stderr, "%p sig_handler() - got signal %i\n", (void*)pthread_self(),s);
	}
	if(s == SIGPIPE){
		//TODO.. what now
		; //close(connfd);
	}
	//SIGINT  - sync and kill all
	//SIGTERM - sync and kill all
	//SIGHUP  - sync
	else if(s == SIGTERM || s == SIGHUP || s == SIGINT){
		write_lock(&mut);
		if(debug){
			fprintf(stderr, "%p sig_handler() - syncing data\n", (void*)pthread_self());
		}
		if(hhdb_sync(&hdbp) && debug){
			fprintf(stderr, "%p sig_handler() - syncing failed\n", (void*)pthread_self());
		}
		unlock(&mut);
		if(s == SIGTERM || s == SIGINT){
			if(lflag){
				hdb_close_log();
			}
			exit(0);
		}
/*
		//LINUXTHREADS for older systems where every threads has it's own PID
		if(s == SIGTERM || s == SIGINT){
			pthread_exit(NULL);
			if(lflag){
				hdb_close_log();
			}
		}
*/
	}
}

void sigchld_handler(int s) {
	while(wait(NULL) > 0){
		if(debug){
			fprintf(stderr, "%p sigchild_handler() - got signal %i\n", (void*)pthread_self(), s);
		}
	}
}

void usage(){
	fprintf(stderr, "hdb daemon %s", APP_VERSION);
	fprintf(stderr, 
			"Usage: nhdbd [switches]\n"
			"   -d          Debug mode. No daemon and debug output\n"
			"   -f          Foreground. No daemon\n"
			"   -t number   Number of connection threads (default 10)\n"
			"   -c number   Cache size (must be less than ulimit -n, default 128)\n"
			"   -l facility Log requests to facility (syslog,filename or console)\n"
			"   -h          Help.\n");
	exit(1);
}


int x_get_raw(Thread *t, HDB *hdbp, char *list, char *key, int *type){

	int ret=0;
	char *value=NULL;
	char output[MAXMESSAGE];
	char val_encoded[MAXMESSAGE*3+1];

	read_lock(&mut);
	if(debug){
		fprintf(stderr, "%p x_get_raw() - root=%s list=%s key=%s type=%i\n",
			(void*)pthread_self(), hdbp->root, list, key, *type);
	}

	if((value = hhdb_get_raw(hdbp, list, key, type))!=NULL){
		if(!urlencode(val_encoded, value)){
			snprintf(output, sizeof(output), "#ERR %i\n",hdbp->dberrno);
			ret=buffered_send_flush(t,output);
		}
		else {
			snprintf(output, sizeof(output), "%i %s\n#OK\n", *type, val_encoded);
			ret=buffered_send_flush(t,output);
		}
		free(value);
	}
	else {
		snprintf(output, sizeof(output), "#ERR %i\n",hdbp->dberrno);
		ret=buffered_send_flush(t,output);
	}
	unlock(&mut);
	return ret;
}

int x_dump_flat_regex(Thread *t, HDB *hdbp, char *list, char *key, char *value){

	int ret = 0;
	FILE *file=NULL;
	int fd = -1;

	read_lock();
#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	hhdb_sync(hdbp);
#endif
       if(debug){
                fprintf(stderr, "%p x_dump_flat_regex() - list=%s key=%s value=%s\n",
                (void*)pthread_self(), list, key, value);
        }


	fd = dup(t->fd);

	//file = fdopen(t->fd,"w+");
	file = fdopen(fd,"w+");
	if(file==NULL){
		printf("fdopen failed %i\n", errno);
		perror("x_dump_flat_free");
		assert(0);
		unlock();
		return 1;
	}
	//hampa..testing
	/*
	ret=buffered_send_flush(t,"#OK\n");
	close(fd);
	fclose(file);
	unlock();
	return ret;
	*/
	
	strcpy(t->socket_buffer, "");
	t->socket_size=0;
	hdbp->file = file;
	if(hhdb_dump_regex(hdbp, list, key, value)){
		fflush(file);
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		fflush(file);
		ret=buffered_send_flush(t,"#OK\n");
	}
	// hampa testing
	close(fd);
	fclose(file);

	unlock();
	return ret;
}

int x_dump_flat_glob(Thread *t, HDB *hdbp, char *list, char *key, char *value){

	int ret = 0;
	FILE *file=NULL;
	int fd = -1;

	read_lock();
#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	hhdb_sync(hdbp);
#endif
	if(debug){
		fprintf(stderr, "%p x_dump_flat_glob() - list=%s\n", (void*)pthread_self(), list);
	}

	fd = dup(t->fd);

	//file = fdopen(t->fd,"w+");
	file = fdopen(fd,"w+");
	if(file==NULL){
		printf("fdopen failed %i\n", errno);
		perror("x_dump_flat_glob_free");
		unlock();	
		return 1;
	}
	
	strcpy(t->socket_buffer, "");
	t->socket_size=0;
	hdbp->file = file;
	if(hhdb_dump_glob(hdbp, list, key, value)){
		fflush(file);
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		fflush(file);
		ret=buffered_send_flush(t,"#OK\n");
	}
	// hampa testing
	close(fd);
	fclose(file);

	unlock();
	return ret;
}

int x_dump_flat(Thread *t, HDB *hdbp, char *list){

	int ret = 0;

	read_lock();
#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	hhdb_sync(hdbp);
#endif
	if(debug){
		fprintf(stderr, "%p x_dump_flat() - list=%s\n", (void*)pthread_self(), list);
	}

	strcpy(t->socket_buffer, "");
	t->socket_size=0;
	if(hdb_dump_flat(t, hdbp, 1, list)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_dump(Thread *t, HDB *hdbp, int level, char *list){

	int ret = 0;

	read_lock();
#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	hhdb_sync(hdbp);
#endif
	if(debug){
		fprintf(stderr, "%p x_dump() - list=%s\n", (void*)pthread_self(), list);
	}

	strcpy(t->socket_buffer, "");
	t->socket_size=0;
	if(hdb_dump(t, hdbp, level, 1, list)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	if(debug){
		fprintf(stderr, "%p x_dump() - done =%s\n", (void*)pthread_self(), list);
	}
	unlock();
	return ret;
}

int x_exist(Thread *t, HDB *hdbp, char* list){

	int ret = 0;

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_exist() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	if(hhdb_exist(hdbp, list)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_key_exist(Thread *t, HDB *hdbp, char* list, char *key){

	int ret = 0;

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_exist() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	if(hhdb_key_exist(hdbp, list, key)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_sync(Thread *t, HDB *hdbp){

	int ret = 0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_sync()\n", (void*)pthread_self());
	}

	if(hhdb_sync(hdbp)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_mv(Thread *t, HDB *hdbp, char *src, char *target){
		
	int ret=0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_mv() - root=%s src=%s target=%s\n", (void*)pthread_self(), hdbp->root, src,target);
	}

	if(hhdb_mv(hdbp, src, target)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();

	return ret;
}

int x_del_val(Thread *t, HDB *hdbp, char *list, char *key){

	int ret = 0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_del_val() - root=%s list=%s key=%s\n", (void*)pthread_self(), hdbp->root, list, key);
	}

	if(hhdb_del_val(hdbp, list, key)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_get_nval(Thread *t, HDB *hdbp, char *list, char *key){
	int ret = 0;
	int hdbret = 0;
	char value[HDB_VALUE_MAX];
	char val_encoded[HDB_VALUE_MAX*3+1];
	char output[MAXMESSAGE];

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_get_nval() - root=%s list=%s key=%s\n", 
			(void*)pthread_self(), hdbp->root,list, key);
	}

	if((hdbret=hhdb_get_nval(hdbp, list, key, HDB_VALUE_MAX, value))!=0){
		assert(hdbret!=0);
		snprintf(output, MAXMESSAGE, "#ERR %i\n", hdbret);
		ret=buffered_send_flush(t,output);
	}
	else {
		if(!urlencode(val_encoded, value)){
			snprintf(output, MAXMESSAGE, "#ERR %i\n", hdbret);
			ret=buffered_send_flush(t,output);
		}
		else {
			printf("val_encoded %s value %s\n", val_encoded, value);

			snprintf(output, MAXMESSAGE, "%s\n#OK\n", val_encoded);
			ret=buffered_send_flush(t,output);
		}
	}
	unlock();
	return ret;
}

int x_update_val(Thread *t, HDB *hdbp, char *list, char *key, char *value){

	int ret = 0;

	write_lock(&mut);
	if(debug){
		fprintf(stderr, "%p x_update_val() - root=%s list=%s key=%s\n", (void*)pthread_self, hdbp->root, list, key);
	}

	if(!hhdb_key_exist(hdbp, list,key) && !hhdb_set_val(hdbp, list, key, value)){
		ret=buffered_send_flush(t,"#OK\n");
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();
	return ret;
}


int x_set_val(Thread *t, HDB *hdbp, char *list, char *key, char *value){

	int ret = 0;
	char plain[HDB_VALUE_MAX];

	write_lock(&mut);
	if(debug){
		fprintf(stderr, "%p x_set_val() - root=%s list=%s key=%s\n", (void*)pthread_self, hdbp->root, list, key);
	}
	//unencode any data that we got
	urldecode(plain, value);
	if(hhdb_set_val(hdbp,list,key,plain)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_set_raw(Thread *t, HDB *hdbp, char *list, char *key, char *value, int type){

	int ret = 0;
	int errcode = 0;
	char plain[HDB_VALUE_MAX];
	
	strcpy(plain,"");

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_set_raw() - root=%s list=%s key=%s strlen(value)=%i value=%s type=%i\n",
		(void*)pthread_self(), hdbp->root, list, key, strlen(value), value, type);
	}

	urldecode(plain, value);
	if((errcode=hhdb_set_raw(hdbp, list, key, plain, type))){
		ret=buffered_send_flush(t,"#ERR\n");
		if(debug){
			fprintf(stderr, "%p x_set_raw() - error %i\n", (void*)pthread_self(), errcode);
		}
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	if(debug){
		fprintf(stderr, "%p x_set_raw() ret %i errcode %i\n", (void*)pthread_self(), ret, errcode);	
	}
	unlock();
	return ret;
}

int get_num_connected(){
	int i = 0;
	int connected = 0;
	for(i = 0; i<nthreads; i++){
		if(tptr[i].state){
			connected++;
		}
	}
	return connected;
}

int x_status(Thread *t, int id){

	int ret=0;
	int i = 0;
	char buf[512];
	time_t now = time(NULL);
	time_t contime = 0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_status()\n", (void*)pthread_self());
	}
	for(i=0;i<nthreads;i++){
		if(tptr[i].thread_count != 0){

			contime  = now - tptr[i].contime;

			snprintf(buf, sizeof(buf), 
			"%sthread: %d con: %ld contime %ldm:%lds hdbroot: %s state: %s(%i)\n", 
			i==id ? "*" : "",
			i, 
			tptr[i].thread_count,
			contime/60, contime%60,
			tptr[i].hdbroot,
			tptr[i].state ? "serving" : "waiting",
			tptr[i].state);
			
			if(buffered_send(t,buf)==-1){
				unlock();
				return 1;
			}
		}
	}
	snprintf(buf, sizeof(buf), "cache type: %i\nlists in hash: %i\nhash size: %i\n#OK\n", 
		hdb_get_configi(HDB_CONFIG_CACHE_TYPE),
		hdb_get_configi(HDB_CONFIG_HASH_SIZE),
		hdb_get_configi(HDB_CONFIG_HASH_MAX_SIZE));
	ret=buffered_send_flush(t, buf);
	unlock();
	return ret;	
}

int x_set_config(Thread *t, HDB *hdbp, int parameter, int value){ 
	int ret = 0;

	if(debug){
		fprintf(stderr, "%p x_set_config() - parameter=%i value=%i\n", (void*)pthread_self(), parameter, value);
	}

	write_lock();
	if(!hhdb_set_configi(hdbp, parameter, value)){
		ret=buffered_send_flush(t, "#OK\n");
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();

	return ret;
}

int x_get_config(Thread *t, HDB *hdbp, int parameter){ 
	int ret = 0;
	int value = 0;
	char output[HDB_VALUE_MAX];

	if(debug){
		fprintf(stderr, "%p x_get_config() - parameter=%i\n", (void*)pthread_self(), parameter);
	}

	read_lock();
	if((value = hhdb_get_configi(hdbp, parameter)) != -1){
		snprintf(output,sizeof(output), "%i\n#OK\n", value);	
		ret=buffered_send_flush(t, output);
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();

	return ret;
}

int x_delete_list(Thread *t, HDB *hdbp, char *list){
	int ret=0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_delete_list() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}
	if(hhdb_delete_list(hdbp, list)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_wipe(Thread *t, HDB *hdbp){
	int ret=0;
	
	write_lock();
	if(debug){
		fprintf(stderr, "%p x_wipe() - root=%s\n", (void*)pthread_self(), hdbp->root);
	}
	if(hhdb_wipe(hdbp)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_add(Thread *t, HDB *hdbp, char *list, char *key, int amount){

	int ret = 0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_add() - root=%s list=%s key=%s amount=%i\n", 
		(void*)pthread_self(), hdbp->root, list, key, amount);
	}
	if(hhdb_add(hdbp,list,key,amount)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_get_cur(Thread *t, HDB *hdbp, char *list, int cursor){
        char output[MAXMESSAGE];
        char value[HDB_VALUE_MAX];
        char key[HDB_KEY_MAX];
        int ret = 0;
	int herr = 0;

        if(debug){
                fprintf(stderr, "%p x_get_cur() - list=%s cursor=%i\n", (void*)pthread_self(), list, cursor);
        }

	write_lock();
       	if((herr=hhdb_get_cur(hdbp, list, cursor, key, value))){
         	ret=buffered_send_flush(t, "#ERR 4\n");
	}
        else {
		snprintf(output, sizeof(output), "%s %s\n#OK\n", key, value);
		ret=buffered_send_flush(t, output);
	}
	unlock();
	if(debug){
		fprintf(stderr, "%p x_get_cur() returned errcode=%i key=%s value%s\n", (void*)pthread_self(), herr, key, value);
	}
        return ret;
}

int x_get_sublist_cur(Thread *t, HDB *hdbp, char *list, int cursor){
	int ret=0;
	char value[HDB_VALUE_MAX];
	char output[MAXMESSAGE];

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_get_sublist_cur_full() - root=%s list=%s cursor=%i\n", 
		(void*)pthread_self(), hdbp->root, list, cursor);
	}

	if(hhdb_get_sublist_cur(hdbp, list, cursor, value)==0){
		snprintf(output, sizeof(output), "%s\n#OK\n", value);
		ret=buffered_send_flush(t,output);
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();

	return ret;
}

int x_get_sublist_cur_full(Thread *t, HDB *hdbp, char *list, int cursor){
	int ret=0;
	char value[HDB_VALUE_MAX];
	char output[MAXMESSAGE];

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_get_sublist_cur_full() - root=%s list=%s cursor=%i\n", 
		(void*)pthread_self(), hdbp->root, list, cursor);
	}

	if(hhdb_get_sublist_cur_full(hdbp, list, cursor, value)==0){
		snprintf(output, sizeof(output), "%s\n#OK\n", value);
		ret=buffered_send_flush(t,output);
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();

	return ret;
}


int x_get_sublist_full(Thread *t, HDB *hdbp, char *list, int index){

	int ret=0;
	char value[HDB_VALUE_MAX];
	char output[MAXMESSAGE];

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_get_sublist_full() - root=%s list=%s index=%i\n",
		(void*)pthread_self(), hdbp->root, list, index);
	}
	if((ret=hhdb_get_sublist_full(hdbp, list, index, value)) == 0){
		snprintf(output, sizeof(output), "%s\n#OK\n", value);
	}
	else {
		strcpy(output, "#ERR\n");
	}
	ret=buffered_send_flush(t,output);
	unlock();
	return ret;
}

int x_get_rec(Thread *t, HDB *hdbp,char *list, int index){
	int ret=0;
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	char output[MAXMESSAGE];

	
	write_lock();
	if(debug){
		fprintf(stderr, "%p x_get_rec() - root=%s list=%s index=%i\n", (void*)pthread_self(), hdbp->root, list, index);
	}

	if(!hhdb_get_rec(hdbp, list, index, key, value)){
		snprintf(output,sizeof(output),"%s %s\n#OK\n",key,value);
		ret=buffered_send_flush(t,output);
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();
	return ret;
}
int x_list_stat(Thread *t, HDB *hdbp, char *list){
	int ret=0;
	char output[MAXMESSAGE];
	HDBS stat;

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_list_stat() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	if((ret = hhdb_list_stat(hdbp, list, &stat))){
		snprintf(output,sizeof(output),"#ERR %i\n", ret);
	}
	else {
		snprintf(output,sizeof(output),"%li %li\n#OK\n",stat.mtime, stat.atime);
	}

	ret=buffered_send_flush(t,output);
	unlock();
	return ret;
}

int x_print_stat(Thread *t, HDB *hdbp, char *list, int format){
	int ret=0;
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	HDBS stat;
	HDBC *hdbc=NULL;
	FILE *file=NULL;
	int fd = -1;
	
	if(debug){
		fprintf(stderr, "%p x_list_stat() - list=%s format=%i\n", (void*)pthread_self(), list, format);
	}
	read_lock();	

#ifndef WITH_BDB_LOCKING
	hhdb_sync(hdbp);
#endif
	fd = dup(t->fd);
	file = fdopen(fd,"w+");
	if(file==NULL){
		perror("x_print_stat");
		unlock();
		return 1;
	}

	strcpy(t->socket_buffer, "");
	t->socket_size=0;
	hdbp->file = file;

	if((hdbc=hhdb_copen(hdbp, hdbc, list))==NULL){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		while(!hdb_cget(hdbc, key, value)){
			//TODO.. add some check that value is what we want
			if(!hdb_stat(list,key,&stat)){
				hdb_print_output(hdbp->file, format, stat.mtime, stat.atime, list, key, value);
			}
		}
		hdb_cclose(hdbc);
	}
	fflush(file);
	ret=buffered_send_flush(t,"#OK\n");
	close(fd);
	fclose(file);
	unlock();
	return ret;
}

int x_stat(Thread *t, HDB *hdbp, char *list, char *key){
	int ret=0;
	char output[MAXMESSAGE];
	HDBS stat;

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_stat() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	if((ret = hhdb_stat(hdbp, list, key, &stat))){
		snprintf(output,sizeof(output),"#ERR %i\n", ret);
	}
	else {
		snprintf(output,sizeof(output),"%li %li\n#OK\n",stat.mtime, stat.atime);
	}

	ret=buffered_send_flush(t,output);
	unlock();
	return ret;
}

int x_get_size(Thread *t, HDB *hdbp, char *list){
	int ret=0;
	int size= 0;
	char output[MAXMESSAGE];

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_get_size() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}
	size=hhdb_get_size(hdbp, list);
	snprintf(output,sizeof(output),"%i\n#OK\n",size);
	ret=buffered_send_flush(t,output);
	unlock();
	return ret;
}

int x_create_list(Thread *t, HDB *hdbp, char *list){
	int ret = 0;

	write_lock();
	if(debug){
		fprintf(stderr, "%p x_create_list() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}
	if(hhdb_create_list(hdbp,list)){
		ret=buffered_send_flush(t,"#ERR\n");
	}
	else {
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_get_sublist(Thread *t, HDB *hdbp, char *list, int index){

	int ret = 0;
	char value[HDB_VALUE_MAX];
	char output[MAXMESSAGE];

	read_lock(&mut);
	if(debug){
		fprintf(stderr, "%p x_get_sublist() - root=%s list=%s index=%i\n", (void*)pthread_self(), hdbp->root, list, index);
	}

	if((ret=hhdb_get_sublist(hdbp, list, index, value)) == 0){
		snprintf(output, sizeof(output), "%s\n#OK\n", value);
		ret=buffered_send_flush(t,output);
	}
	else {
		ret=buffered_send_flush(t,"#ERR\n");
	}
	unlock();
	return ret;
}

int x_sublist_cur_full(Thread *t, HDB *hdbp, char *list){

	int ret=0;
	int i=0;
	int numlists=0;
	char **plist=NULL;	
	char output[MAXMESSAGE];

	write_lock(&mut);
	if(debug){
		fprintf(stderr, "%p x_sublist_cur_full() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);	
	}

	if((numlists = hhdb_scan_sublist_full(hdbp,list,&plist))!=-1){
		for(i=0;i<numlists;i++){
			snprintf(output, sizeof(output), "%s\n", plist[i]);
			if((ret=buffered_send(t,output))==-1){
				break;
			}
		}
		hdb_scan_sublist_close(plist);
	}

	if(ret==0){
		ret=buffered_send_flush(t,"#OK\n");
	}

	unlock();
	return ret;
}

int x_print_sublist(Thread *t, HDB *hdbp, char *list){
	int ret=0;
	int i=0;
	int numlists=0;
	char **plist=NULL;
	char output[MAXMESSAGE];

	read_lock();
	if(debug){
		fprintf(stderr, "%p x_print_sublist() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	if((numlists = hhdb_scan_sublist(hdbp,list,&plist))!=-1){
		for(i=0;i<numlists;i++){
			snprintf(output, sizeof(output), "%s\n", plist[i]);
			if((ret=buffered_send(t,output))==-1){
				break;
			}
		}
		hdb_scan_sublist_close(plist);
	}
	if(ret==0){
		ret=buffered_send_flush(t,"#OK\n");
	}
	unlock();
	return ret;
}

int x_print_list(Thread *t, HDB *hdbp, char *list){
	int ret=0;
	char sublist[HDB_PATH_MAX];
	char output[MAXMESSAGE];
	HDBC *hdbc=NULL;

	read_lock();
#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	hhdb_sync(hdbp);
#endif
	if(debug){
		fprintf(stderr, "%p x_print_list() root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}
	//TODO.. use hdb_print?
	if((hdbc = hhdb_sublist_copen(hdbp,hdbc,list)) != NULL){
		while(!hdb_sublist_cget_full(hdbc, sublist, sizeof(sublist))){
			snprintf(output, sizeof(output), "%s\n", sublist);
			if(buffered_send(t,output)==-1){
				break;
			}
		}
		hdb_sublist_cclose(hdbc);	
	}

	ret=buffered_send_flush(t,"#OK\n");
	unlock();
	return ret;
}

int x_print(Thread *t, HDB *hdbp, char *list, int output){

	int ret = 0;
	char buf[MAXMESSAGE];

	if(debug){
		fprintf(stderr, "%p x_print() - root=%s list=%s\n", (void*)pthread_self(), hdbp->root, list);
	}

	//write_lock();
	if((ret=hdb_print(t, hdbp, list, output))){
		snprintf(buf, sizeof(buf), "#ERR %i\n", ret);
		ret=buffered_send_flush(t,buf);
	}
	else {	
		ret=buffered_send_flush(t,"#OK\n");
	}
	if(debug){
		fprintf(stderr, "%p x_print() done ret %i\n", 
			(void*)pthread_self(), ret);
	}
	//unlock();
	return ret;
}

int main(int argc , char **argv)
{
	struct sockaddr_in my_addr;
	int yes=1;
	int i=0;
	int fd_lockfile=0;
	int port = HDB_PORT;
	int hash_max_size = 0;
	FILE *pidfile = NULL;

	//initalize with hash
	//this can be turned of with -c 0
	hdb_set_configi(HDB_CONFIG_CACHE_TYPE,HDB_CACHE_HASH);

	for(i = 0; i< argc; i++){
		if(!strcmp(argv[i], "-d")){
			dflag++;
			debug++;
		}
		else if(!strcmp(argv[i], "-f")){
			fflag++;	
		}
		else if (!strcmp(argv[i], "-l")){
			if(++i<argc){
				if(!strcmp(argv[i], "syslog")){
					if(hdb_open_log(HDB_LOG_SYSLOG, NULL)){
						fprintf(stderr, "FAIL - failed to open syslog\n");
						exit(1);
					}
					lflag=HDB_LOG_SYSLOG;
				}
				else if (!strcmp(argv[i], "console")){
					lflag=HDB_LOG_CONSOLE;
					hdb_open_log(HDB_LOG_CONSOLE, NULL);
				}
				else {
					if(hdb_open_log(HDB_LOG_FILE, argv[i])){
						fprintf(stderr, 
						"FAIL - failed to open %s for logging\n", 
						argv[i]);
					}
					lflag=HDB_LOG_FILE;
				}
			}
			else {
				usage();
			}
		}
		else if(!strcmp(argv[i], "-t")){
			if(++i<argc){
				nthreads=atoi(argv[i]);		
			}
			else {
				usage();
			}
		}	
		else if(!strcmp(argv[i], "-c")){
			if(++i<argc){
				hash_max_size=atoi(argv[i]);
				hdb_set_configi(HDB_CONFIG_HASH_MAX_SIZE,hash_max_size);
				if(hash_max_size==0){
					hdb_set_configi(HDB_CONFIG_CACHE_TYPE,HDB_CACHE_NONE);
				}
			}
			else {
				usage();
			}
		}
		else if(!strcmp(argv[i], "-h")){
			usage();
		}
	}

	/*
	if(pthread_rwlock_init(&rwlock, NULL)){
		fprintf(stderr, "Failed to initialize rwlock\n");
		exit(1);
	}
	*/

	if(!dflag && !fflag){
		daemon(0,0);
	}

	signal(SIGINT, sig_handler);

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	my_addr.sin_family = AF_INET;         // host byte order
	my_addr.sin_port = htons(port);     // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if (bind(listenfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(listenfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	// Write out the pid file 
	if((pidfile = fopen("/var/run/hdbd.pid", "wb")) == NULL){
		perror("Couldn't create pid file /var/run/hdbd.pid");
	}
	else {
		fprintf(pidfile, "%ld\n", (long) getpid());
		fclose(pidfile);
	}

	//create a lock file to tell clients that server is running
	//printf("LOCKING %s\n", HDB_LOCKFILE);
	if((fd_lockfile=lock(HDB_LOCKFILE))==-1){
		fprintf(stderr, "unable get lock on %s\n", HDB_LOCKFILE);
		exit(1);
	}

	tptr = malloc(nthreads * sizeof(Thread));

#ifndef NOTHREADS 
	//create our threads
	for (i = 0; i < nthreads; i++) {
		pthread_create(&tptr[i].thread_tid, NULL, &thread_main, (void *) i);
	}

	//only main thread returns here but signal will be sent to all threads
	signal(SIGPIPE, sig_handler);
	signal(SIGCHLD, sig_handler);

	//SIGTERM will sync threads and exit
	signal(SIGTERM, sig_handler);

	//SIGHUP will sync threads
	signal(SIGHUP, sig_handler);

	//used internally only
	signal(SIGUSR1, sig_handler);

	while(1){
		pause(); /* everything done by threads */
	}
#else
	thread_main(0);	
#endif
	if(tptr)
		free(tptr);
	return 0;
}

void * thread_main(void *arg){
	int thread_id=(int)arg;
	int connfd;
	void web_child(int);
	socklen_t clilen;
	struct sockaddr_in their_addr;
	int sin_size;
	int amount = 0;
	//int cursor = 0;
	char list[HDB_PATH_MAX];
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	char buf[MAXLENGTHX2];
	int i = 0;
	int tid=0;
	int size = 0;
	int type = 0;
	int ret=0;
	int index = 0;
	char byte=0;
	int bytes_read=0;
	char request[1024];
	HDB hdbp;
	char *stripped = NULL;
	char output[HDB_VALUE_MAX];
	char input[OPTS][HDB_VALUE_MAX];
	int cmd = HDB_NONE;
	int escape_error=0;
	Thread *t=NULL;

	memset(&hdbp, 0, sizeof(HDB)); //Not needed but helps when debugging, 

	if(dflag){
		fprintf(stderr, "%p thread_main() - hdbroot=%s thread_id=%d\n", (void*)pthread_self(), hdbp.root, thread_id);
	}

	for(;;){
		wait_for_new_client:
		
		assert(hdbp.dblock == NULL);

		if(hhdb_set_root(&hdbp, HDBROOT)){
			fprintf(stderr, "%p thread_main() - Unable to set default root. Panic\n", (void*)pthread_self());
			exit(1);
		}

		clilen = addrlen;
		tptr[thread_id].state=0;
		pthread_mutex_lock(&mlock);
		if(debug){
			fprintf(stderr, "%p thread_main() - waiting for new client\n", (void*)pthread_self());
		}
		tptr[thread_id].state=0;
		sin_size = sizeof(struct sockaddr_in);
		if ((connfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			pthread_mutex_unlock(&mlock);
			continue;
		}
		if(debug){
			fprintf(stderr, "%p thread_main() - got connection from %s\n",(void*)pthread_self(),
			inet_ntoa(their_addr.sin_addr));
		}
		tid=(int)arg;
		tptr[tid].thread_count++;
		tptr[tid].state=1;
		tptr[tid].fd=connfd;
		tptr[tid].contime=time(NULL);
		t = &tptr[tid];
		strcpy(hdbp.root, HDBROOT);
		strncpy(tptr[tid].hdbroot,hdbp.root, MAXLENGTH);
		
		// count no of threads
		if(get_num_connected() == nthreads){
			openlog("hdbd", LOG_PID, LOG_DAEMON);
			syslog(LOG_CRIT, "Reached max number of connection threads (%i)", nthreads);
			closelog();
		}

		pthread_mutex_unlock(&mlock);

		//clear buffer from last client
		read_lock();
		//strcpy(socket_buffer, "");
		assert(t->socket_size == 0);

		snprintf(buf, sizeof(buf), "#WELCOME to HDBD version %s, I understand: ", APP_VERSION);
		if(buffered_send_flush(t, buf)==-1){
			unlock();
			goto wait_for_new_client;
		}
		if(commands(t)){
			unlock();
			goto wait_for_new_client;
		}
		unlock();
		while(1){
			byte=bytes_read=0;
			while(byte != '\n'){
				if((size = recv(connfd, &byte, 1, 0)) == -1){
					if(debug){
						fprintf(stderr, "%p thread_main()  - got closed connection when receiving\n",
						(void*)pthread_self());
					}
					hhdb_close(&hdbp);	
					close(connfd);
					goto wait_for_new_client;
				}
				else if(size == 0){
					if(debug){
						fprintf(stderr, "%p thread_main() - client exited without #BYE\n", (void*)pthread_self());
					}
					hhdb_close(&hdbp);	
					close(connfd);
					goto wait_for_new_client;
				}
				if(byte == '\r')
					continue; //this ok?
				if(bytes_read<1024){
					request[bytes_read++] = byte;
				}
			}
			//printf("%c", byte);
			if (bytes_read >= 1024){
				if(debug){
					fprintf(stderr, "%p thread_main() - big request sending #ERR\n", (void*)pthread_self());
				}
				write_lock(&mut);
				if(buffered_send_flush(t,"#ERR\n")==-1){
					unlock();
					goto wait_for_new_client;
				}
				unlock();
			}

			request[bytes_read-1] = '\0';
			if(debug){
				fprintf(stderr, "%p thread_main() - got request %i #%s#\n", (void*)pthread_self(), bytes_read, request);
			}
			if(lflag){
				hdb_print_log("%p %s\n", 
				(void*)pthread_self(),
				request);
			}
			stripped = strip(request);
			if(!strcmp(stripped, "")){
				continue;
			}
			if(!strcasecmp(stripped, "QUIT")){
				if(debug){
					fprintf(stderr, "%p thread_main() - got QUIT closing client %i\n",
					(void*)pthread_self(), thread_id);
				}
				
				hhdb_close(&hdbp);

				if(buffered_lock_send_flush(t,"#BYE\n")==-1){
					//should be closed already
					//strcpy(socket_buffer,"");
				}
				else {
					//strcpy(socket_buffer,"");
#ifdef NOTHREADS
					goto wait_for_new_client;
#else 
					close(connfd);
#endif
				}	
				write_lock(&mut);
				tptr[thread_id].state=0;
				unlock();
				goto wait_for_new_client;
			}
			i=0;
			ret=0;
			strcpy(list, "");
			strcpy(value, "");
			strcpy(output, "");
			strcpy(key, "");
		
			escape_error = unescape(stripped, input);
			/*
			printf("###GOT %p list=%s key=%s value(strlen%i)=%s extra=%s escape_error=%i\n",
				(void*)pthread_self(),
				input[1], input[2], strlen(input[3]), input[3], input[4],
				escape_error);
			*/
			if(escape_error || input[0] == NULL || !strcmp(input[0], "")){
				if(buffered_lock_send_flush(t,"#ERR\n")==-1){
					goto wait_for_new_client;
				}
				else {
					continue;
				}
			}
			//you can use the numerical procedure name
			if(!isalpha(*input[0])){
				cmd = atoi(input[0]);
			}
			else if((cmd = find_command(input[0])) == HDB_NONE){
				snprintf(buf, sizeof(buf), "#ERR 101 I dont understand that command\n");
				if(buffered_lock_send_flush(t,buf)==-1){
					goto wait_for_new_client;
				}
				continue;
			}
			ret=0;
			switch(cmd){
				case HDB_GET_VAL:
					ret = x_get_nval(t,&hdbp,input[1],input[2]);
					break;
				case HDB_SET_VAL:
					ret = x_set_val(t,&hdbp,input[1],input[2],input[3]);
					break;
				case HDB_UPDATE_VAL:
					ret = x_update_val(t,&hdbp,input[1],input[2],input[3]);
					break;
				case HDB_DEL_VAL:
					ret = x_del_val(t, &hdbp, input[1], input[2]);
					break;
				case HDB_EXIST:
					if(strcmp(input[2], "")){
						ret = x_key_exist(t,&hdbp,input[1], input[2]);
					}
					else {
						ret = x_exist(t,&hdbp,input[1]);
					}
					break;
				case HDB_MOVE:
					ret=x_mv(t,&hdbp, input[1], input[2]);
					break;
				case HDB_SYNC:
					ret=x_sync(t, &hdbp);
					break;
				case HDB_SET_EXEC:
					ret=x_set_raw(t,&hdbp,input[1],input[2],input[3],HDB_TYPE_EXEC);	
					break;
				case HDB_SET_LINK:
					ret=x_set_raw(t,&hdbp,input[1],input[2],input[3],HDB_TYPE_LINK);	
					break;
				case HDB_SET_FILE:
					ret=x_set_raw(t,&hdbp,input[1],input[2],input[3],HDB_TYPE_FILE);	
					break;
				case HDB_SET_RAW:
					type = atoi(input[4]);
					ret=x_set_raw(t,&hdbp,input[1],input[2],input[3],type);
					break;
				case HDB_GET_RAW:
					ret=x_get_raw(t,&hdbp,input[1], input[2], &type);
					break;
				case HDB_PRINT_FULL:
					ret=x_print(t,&hdbp,input[1], 0);
					break;
				case HDB_PRINT:
					ret=x_print(t,&hdbp,input[1], 1);
					break;
				case HDB_PRINT_LIST:
					//ret=x_print_list(t,hdbp,input[0]);
					ret=x_print_sublist(t,&hdbp,"");
					break;
				case HDB_PRINT_LISTR:
					ret=buffered_lock_send_flush(t,"#ERR 99 - Not implemented\n");
					break;
				case HDB_DUMP_FLAT:
					ret=x_dump_flat(t,&hdbp,input[1]);	
					break;
				case HDB_DUMP_FLAT_REGEX:
					ret=x_dump_flat_regex(t,&hdbp,input[1],input[2],input[3]);
					break;
				case HDB_DUMP_FLAT_GLOB:
					ret=x_dump_flat_glob(t,&hdbp,input[1],input[2],input[3]);
					break;
				case HDB_DELETE_LIST:
					ret=x_delete_list(t,&hdbp,input[1]);
					break;
				case HDB_DUMP:
					if(!strcmp(input[1], "")){
						ret=x_dump(t, &hdbp, -1, input[1]);	
					}
					else {
						ret=x_dump(t, &hdbp, 0, input[1]);	
					}
					break;
				case HDB_CONFIG:
					if(!strcmp(input[2], "")){
						ret=x_get_config(t, &hdbp, atoi(input[1]));
					}
					else {
						ret=x_set_config(t, &hdbp, atoi(input[1]), atoi(input[2]));
					}
					break;			
				case HDB_LOG:
					i=0;
					if(!strcmp(input[1], "")){
						switch(hdb_get_configi(HDB_CONFIG_LOG_FACILITY)){
							case HDB_LOG_CONSOLE:
								ret=buffered_lock_send_flush(t,
										"CONSOLE\n#OK\n");
								break;
							case HDB_LOG_SYSLOG:
								ret=buffered_lock_send_flush(t,
										"SYSLOG\n#OK\n");
								break;
							case HDB_LOG_FILE:
								ret=buffered_lock_send_flush(t,
										"FILE\n#OK\n");
								break;
							default:
								ret=buffered_lock_send_flush(t,
										"NONE\n#OK\n");
								break;
						}
					}
					else {
						if(!strcasecmp(input[1], "console")){ 
							lflag=HDB_LOG_CONSOLE;
							i=hdb_open_log(HDB_LOG_CONSOLE, NULL);
						}
						else if(!strcasecmp(input[1], "file")){
							lflag=HDB_LOG_FILE;
							i=hdb_open_log(HDB_LOG_FILE, NULL);
						}
						else if(!strcasecmp(input[1], "syslog")){
							lflag=HDB_LOG_SYSLOG;
							i=hdb_open_log(HDB_LOG_SYSLOG, NULL);
						}
						else if(!strcasecmp(input[1], "none")){
							lflag=0;
							hdb_close_log();
						}
						else {
							i=1;
						}

						if(i){
							lflag=0;
							ret=buffered_send_flush(t,"#ERR\n");
						}
						else {
							ret=buffered_send_flush(t,"#OK\n");

						}
					}
					break;
				case HDB_ROOT:
					if(!strcmp(input[1], "")){
						if(debug){
							fprintf(stderr, "%p get_root()\n", (void*)pthread_self());
						}
						snprintf(output,sizeof(output), "%s\n#OK\n", hdbp.root);	
						ret=buffered_lock_send_flush(t,output);
					}
					else {
						write_lock();
						if(debug){
							fprintf(stderr, "%p set_root() - root=%s\n", 
							(void*)pthread_self(), input[1]);
						}
						if(!hhdb_set_root(&hdbp, input[1])){
							ret=buffered_send_flush(t,"#OK\n");
						}
						else {
							ret=buffered_send_flush(t,"#ERR\n");
						}
						unlock();
					}
					break;
				case HDB_VERSION:
					snprintf(output, sizeof(output), "%s\n#OK\n", APP_VERSION);
					ret=buffered_lock_send_flush(t,output);
					break;
				case HDB_WIPE:
					ret=x_wipe(t,&hdbp);
					break;
				case HDB_DEBUG:
					//debug^=1;
					ret=buffered_lock_send_flush(t, "#ERR\n");
					break;
				case HDB_ADD:
					amount = atoi(input[3]);
					ret=x_add(t,&hdbp,input[1],input[2],amount);
					break;
				case HDB_PRINT_SUBLIST:
					//this is used alot in SPA
					ret=x_print_sublist(t,&hdbp,input[1]);
					break;
				case HDB_PRINT_SUBLIST_FULL:
					ret=x_sublist_cur_full(t,&hdbp,input[1]);
					break;
				case HDB_GET_SUBLIST:
					index=atoi(input[2]);
					ret=x_get_sublist(t,&hdbp,input[1],index);
					break;
				case HDB_GET_SUBLIST_FULL:
					index=atoi(input[2]);
					ret=x_get_sublist_full(t,&hdbp,input[1],index);
					break;
				case HDB_GET_SUBLIST_CUR_FULL:
					ret=x_get_sublist_cur_full(t,&hdbp,input[1],atoi(input[2]));
					break;
				case HDB_GET_SUBLIST_CUR:
					ret=x_get_sublist_cur(t,&hdbp,input[1],atoi(input[2]));
					break;
				case HDB_GET_CUR:
					ret=x_get_cur(t,&hdbp,input[1],atoi(input[2]));
					break;
				case HDB_CREATE_LIST:
					ret=x_create_list(t,&hdbp,input[1]);
					break;
				case HDB_GET_SIZE:
					ret=x_get_size(t,&hdbp,input[1]);
					break;
				case HDB_PRINT_STAT:
					ret=x_print_stat(t, &hdbp, input[1], atoi(input[2]));
					break;
				case HDB_STAT:
					if(!strcmp(input[2], "")){
						ret=x_list_stat(t,&hdbp, input[1]);
					}
					else {
						ret=x_stat(t,&hdbp, input[1], input[2]);
					}
					break;
				case HDB_GET_REC:
					index = atoi(input[2]);
					ret=x_get_rec(t,&hdbp,input[1],index);
					break;
				case HDB_HELP:
					read_lock();
					ret=help(t,input[1]);
					unlock();
					break;
				case HDB_STATUS:
					ret=x_status(t, (int)arg);
					break;
				case HDB_PORT:
					ret=buffered_lock_send_flush(t,
		"Would you like to play a game of chess? I play very well.\n#OK\n");
					//hdb_print_hash_cache(); //todo remove
					//I use this for debugging
					assert(0);
					exit(0);
					break;
				default: 
					ret=buffered_lock_send_flush(t, "#ERR\n");
					break;
			}
			
			if(ret){
				if(debug){
					fprintf(stderr, "%p socket error.. goto waiting for new client\n",(void*)pthread_self());
				}
				
				goto wait_for_new_client;
			}
		}
		read_lock();
		send(connfd, "#BYE\n", 5, 0);
		unlock();
	}
	if(debug){
		fprintf(stderr, "%p closing connfd\n", (void*)pthread_self());
	}
	close(t->fd);//must close this.. otherwise we get hanged
	return 0;
}

int buffered_lock_send_flush(Thread *t, const char *data){
	int ret=0;

	write_lock(&mut);
	ret=buffered_send_flush(t,data);
	unlock(&mut);

	return ret;
}

int buffered_send_flush(Thread *t, const char *data){
	if(buffered_send(t,data)){
		return -1;
	}
	if(buffered_flush(t)){
		return -1;
	}
	return 0;
}

int buffered_flush(Thread *t){
	int ret=0;
	if(t->socket_size){
/*
		if(debug){
			fprintf(stderr, "%p buffered_flush() socket_size %i strlen %i\n---\n%s---", 
			(void*)pthread_self(), t->socket_size, strlen(t->socket_buffer), t->socket_buffer);
		}
*/
		assert(t->socket_size == strlen(t->socket_buffer));
		if((ret=send(t->fd,t->socket_buffer,t->socket_size,0))==-1){
			if(debug){
				fprintf(stderr, "%p buffered_flush() - send socket error %i\n", (void*)pthread_self(), ret);
			}
			close(t->fd);
			//clear the socket buffer for next client
			strcpy(t->socket_buffer,"");
			t->socket_size=0;
			return -1;
		}
		strcpy(t->socket_buffer,"");
		t->socket_size=0;
	}
	else {
		if(debug){
			fprintf(stderr, "%p socket size is 0 socket_buffer = %s\n", (void*)pthread_self(), t->socket_buffer);
		}
	}
	return 0;
}

int buffered_send(Thread *t, const char *data){
	int len=strlen(data);

	//if(dflag){
		//fprintf(stderr, "%p buffered_send() socket_size = %i len = %i data %s socket_buffer len %i\n", 
		//(void*)pthread_self(), t->socket_size, len, data, strlen(t->socket_buffer));
	//}
	if(t->socket_size+len>=SOCKSIZE){
		if(dflag){
			fprintf(stderr, "%p buffered_send() - data bigger than socksize\n", (void*)pthread_self());
		}
	
		if(buffered_flush(t)){
			if(debug){
				fprintf(stderr, "%p buffered_send() - socket error\n", (void*)pthread_self());
			}
			strcpy(t->socket_buffer, "");
#if DEBUG
			memset(t->socket_buffer, 0, SOCKSIZE);
#endif
			t->socket_size=0;
			return -1;
		}
		strncpy(t->socket_buffer,data,SOCKSIZE);
		t->socket_size=0;
	}
	else {
		strcat(t->socket_buffer,data);
	}
	t->socket_size+=len;
	//TODO.. remove
#if DEBUG
	if(t->socket_size != strlen(t->socket_buffer)){
		fprintf(stderr, "%p buffer '%s'\n", (void*)pthread_self(), t->socket_buffer);
		fprintf(stderr, "%p strlen buffer %i\n", (void*)pthread_self(), strlen(t->socket_buffer));
		fprintf(stderr, "%p buffer %i\n", (void*)pthread_self(), t->socket_size);
		fflush(stderr);
		assert(0);
	}
#endif
	return 0;
}

char *strip(char *s){

	int i;

	i = strlen(s) - 1;

	while(s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r'){
		s[i--] = '\0';
	}

	while(*s == ' ' || *s == '\t'){
		s++;
	}
	return s;
}

int hdb_print(Thread *t,  HDB *hdbp, char *list, int type){
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	HDBC *hdbc=NULL;
	char buf[HDB_VALUE_MAX];
	int ret=0;
	int gotoutput=0;

	while(*(list+strlen(list)-1) == '/'){
		list[strlen(list)-1] = '\0';
	}

#ifndef WITH_BDB_LOCKING
	//if we don't share the enviroment between the hash-cash and copen.. we need to sync
	write_lock();
	hhdb_sync(hdbp);
#else
	read_lock();
#endif


	if((hdbc=hhdb_copen(hdbp, hdbc, list))==NULL){
		assert(hdbp->dberrno);
		unlock();
		printf("got error %i\n", hdbp->dberrno);
		return hdbp->dberrno;
	}

	while(!(ret=hdb_cget(hdbc, key, value))){
		// we need to return HDB_KEY_NOT_FOUND if the list is empty
		// this should match the behaviour for hdb-light (hdb -p emptylist)
		gotoutput=1;

		if(type){
			if(!strcmp(value, "")){
				snprintf(buf, sizeof(buf), "%s\n", key);
			}
			else {
				snprintf(buf, sizeof(buf), "%s %s\n", key, value);	
			}
		}
		else {
			if(!strcmp(value, "")){
				snprintf(buf, sizeof(buf), "%s/%s\n", list, key);	
			}
			else {
				snprintf(buf, sizeof(buf), "%s/%s %s\n", list, key, value);	
			}
		}
		if(buffered_send(t, buf)==-1){
			hdb_cclose(hdbc);
			unlock();
			return -1;
		}
	} 
	hdb_cclose(hdbc);
	unlock();
	return gotoutput ? 0 : ret;
}

int hdb_dump(Thread *t, HDB *hdbp, int level, int start, char *list){
	char sublist[HDB_VALUE_MAX];
	char key[HDB_KEY_MAX];
	char buf[HDB_VALUE_MAX];
	char value[HDB_VALUE_MAX];
	char tabs[HDB_VALUE_MAX];
	int cursor=HDB_NEXT;
	HDBC *hdbc=NULL;

	assert(list);

	if(list == NULL){
		return 0;
	}
	if(level > 0){
		memset(tabs, ' ', sizeof(tabs));
		tabs[level] = '\0';
	}
	else {
		tabs[0] = '\0';
	}
	if(list && strcmp(basename(list), ".")){
		snprintf(buf, sizeof(buf), "%s<%s>\n", tabs, basename(list));
		if(buffered_send(t,buf)==-1){
			return 1;
		}
   	} 
	if((hdbc=hhdb_copen(hdbp, hdbc, list))!=NULL){
		while(!(hdb_cget(hdbc, key, value))){
			snprintf(buf, sizeof(buf), "  %s %s %s\n", tabs , key, value);
			if(buffered_send(t,buf)==-1){
				hdb_cclose(hdbc);
				return 1;
			}
		}
		hdb_cclose(hdbc);
	}
	hdbc=NULL;

	//if previous attempts crashed make sure dirlist cursor is closed
	//if(start){
	//	cursor=HDB_FIRST;
	//}

	if((hdbc = hhdb_sublist_copen(hdbp, hdbc, list)) != NULL){
		while(!hdb_sublist_cget_full(hdbc, sublist, sizeof(sublist))){
			cursor=HDB_NEXT;
			if(hdb_dump(t, hdbp, ++level, 0, sublist)){
				hdb_sublist_cclose(hdbc);	
				return 1;
			}
			level--;
		}
		hdb_sublist_cclose(hdbc);	
	}

   	if(list && strcmp(basename(list), ".")){
		snprintf(buf, sizeof(buf), "%s</%s>\n", tabs, basename(list));
		if(buffered_send(t, buf)==-1){
			return 1;
		}
   	} 
	return 0;
}

int hdb_dump_flat(Thread *t, HDB *hdbp, int start, char *list){

	char buf[MAXLENGTHX2];
	char sublist[HDB_PATH_MAX];
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	//int cursor=HDB_NEXT;
	HDBC *hdbc=NULL;

	if(list == NULL){
		return 0;
	}

	/*
    	if(*list == '/'){
        	list++;
    	}
	*/

	if((hdbc=hhdb_copen(hdbp, hdbc, list))!=NULL){
		while(!(hdb_cget(hdbc, key, value))){
			snprintf(buf, sizeof(buf), "%s/%s=%s\n", list, key, value);
			if(buffered_send(t,buf)==-1){
				hdb_cclose(hdbc);
				return 1;
			}
		}
		hdb_cclose(hdbc);
	}

	//if previous attempts crashed (ctrl-c etc) make sure dirlist cursor is closed
	//if(start){
	//	cursor=HDB_FIRST;
	//}

	if((hdbc = hhdb_sublist_copen(hdbp,hdbc,list)) != NULL){
		while(!hdb_sublist_cget_full(hdbc, sublist, sizeof(sublist))){
			if(hdb_dump_flat(t, hdbp, 0, sublist)){
				hdb_sublist_cclose(hdbc);	
				return 1;
			}
		}
		hdb_sublist_cclose(hdbc);	
	}

	return 0;
}

int unescape(char *in, char out[OPTS][HDB_VALUE_MAX]){
	
	int inside=0;
	char delim=' ';
	int i = 0;
	int x = 0;
	char *p=in;

	do {
		if(!inside){
			if (*p == '"' || *p == '\''){
				delim=*p;
				inside++;
				p++;
			}
			else if (*p != ' '){
				delim=' ';
				inside++;
			}
		}
		if(inside){
			if(*p==delim){
				inside=0;
				out[i][x]='\0';
				if(++i>=OPTS)
					return 1;
				x=0;
			}
			else {
				if(*p=='\\'){
					p++;
				}
				out[i][x++]=*p;
				if(x>HDB_VALUE_MAX)
					return 1;
			}
		}	
	}
	while (*(p++));
	
	while(++i<OPTS){
		out[i][0]='\0';
	}

	return 0;			
}

/******************************************************
We create a lock file so the client apps can know if
the server is running or not. 
*******************************************************/
int lock(char *file){
    struct flock lock;
	int fd;

	if(file == NULL){
		return -1;
	}

	//create 644 lockfile 
	if((fd = open(file, O_RDWR|O_CREAT,  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1) {
		fprintf(stderr, "open failed %s\n", strerror(errno));
		return -1;
	}
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if(!fcntl(fd, F_SETLK, &lock)) {
            return fd;
    }
	return -1;
}

int df_unlock(int fd)
{

	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if((fcntl(fd, F_SETLK, &lock)) == -1) {
		return 1;
	}
	return 0;
}

void write_lock(){
	//pthread_rwlock_rwlock(&rwlock);
#ifndef WITH_BDB_LOCKING
	pthread_mutex_lock(&mut);
#endif
}

void read_lock(){
	//pthread_rwlock_rdlock(&rwlock);
#ifndef WITH_BDB_LOCKING
	pthread_mutex_lock(&mut);
#endif

}
void unlock(){
	//pthread_rwlock_unlock(&rwlock);
#ifndef WITH_BDB_LOCKING
	pthread_mutex_unlock(&mut);
#endif
}

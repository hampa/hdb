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
#include <unistd.h>
#include <sys/types.h>
#if defined (DB3) || defined(DB4)
#define DB3_DB4 1
#include <db.h>
#else
#include <db_185.h>
#endif
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hdb.h"
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <libgen.h>
#include <regex.h>
#include <limits.h>
#include <glob.h>
#include <time.h>

extern int errno;

#ifdef DEBUG
	#ifdef HDBD
	extern  void * pthread_self(void);
	#define DBG(fmt...) if (1) fprintf(stderr, "%p ", (void*)pthread_self()); fprintf(stderr,fmt)
	#else
	#define DBG(fmt...) if (1) fprintf(stderr,fmt)
	#endif
#else
#define DBG(fmt...)
#endif

/****************************************************************
  Trigger routines
 ****************************************************************/
int trigger(int type, const char *list, const char *key, const char *old,
		const char *new);

int use_trigger = 0;
int dberrno=0;


/*****************************************************************
  The hive we store in BDB
 ******************************************************************/
#define META_SIZE 64
struct hive_t {
	char *value;
	int size;
	int *meta;
};

void unpack_hive(char *buffer, struct hive_t *hive);
enum {
	META_RAW,
	META_PROTO,
	META_MTIME,
	META_ATIME
};

/****************************************************************
  BDB open and close routines
 ****************************************************************/
int close_rootlist(const char *root, const char *list);
int open_rootlist_db(const char *rootlist);
DB *open_db2(const char *rootlist, int create);
int add_dbp(const char *rootlist, DB* dbp);
HDBT *get_hdbt(const char *root, const char *list);
int close_hdbt(HDBT *hdbt); //close and free hdbt
#ifdef DB3_DB4
int get_dbc(HDBC *hdbc, char *list, char *value);
int dbp_close(DB *dbp);
DBC * get_dbcp(HDBT *hdbt);
DBC * create_dbcp(DB *dbp);
int close_dbc(DBC *dbcp);
#endif
int dbp_get(DB *dbp, DBT *key, DBT *data);
int dbp_put(DB *dbp, DBT *key, DBT *data);
int dbp_del(DB *dbp, DBT *key);
int dbp_sync(DB *dbp);
DB *dbp_open(const char *rootlist, int create);
DB *dbp_open_create(const char *rootlist);

/****************************************************************
  DBP (database pointer) caching routines
 ****************************************************************/
#ifdef HDBD
int hdb_cache_type = HDB_CACHE_HASH;
#else
//int hdb_cache_type = HDB_CACHE_NONE;
int hdb_cache_type = HDB_CACHE_HASH;
#endif

#ifdef DB3_DB4
int hdb_lock_type = HDB_LOCK_BDB;
#else
int hdb_lock_type = HDB_LOCK_NONE;
#endif

int dbps_size = 128;
int dbps_insert = 0;
int dbps_inc = 0;
int hdbt_hash_size = 0;
int hdbt_hash_is_full = 0;
int hdbt_hash_max_size = 128;	//this will also be the number of open files
HDBT *hdbt_hash = NULL;
HDBT *hdbt_last = NULL;
int hdbt_last_id=0;
int initialize_hdbt();

int add_hdbt_to_cache(const char *rootlist, HDBT *hdbt, int inode_hashkey);
int add_hdbt_to_hash(const char *rootlist, HDBT *hdbt, int inode_hashkey);
int add_hdbt_to_list(int id, HDBT *hdbt);
HDBT *get_hdbt_from_hash(int inode_hashkey);
HDBT *get_hdbt_from_list(int inode_id);
int hdb_print_hash_cache();
int get_hash_key(const char *root, const char *list);
int clear_hdbt_hash();
int del_hdbt_from_hash(int hashkey);

struct hash_hive {
	HDBT *hdbt;
	int i;		//not used today
};

struct dbps {
	char *rootlist;
	HDBT *dbp;
};

/*****************************************************************
  Directory cache to help speed up nested for each
  Not thread safe operations
 *****************************************************************/
struct dirlist {
	DIR *dp;
	int key;
	struct dirlist *next;
	struct dirlist *prev;
	struct dirlist *head;
	struct dirlist *tail;
};

struct linkedlist_t {
	struct dirlist *head;
	struct dirlist *tail;
};

//struct dirlist * create_dirlist_cache( struct dirlist *dl);
struct linkedlist_t *create_dirlist_head( struct linkedlist_t *head);
int close_dirlist_cache(struct linkedlist_t *ll, int key); //close dirlist matching key
int clear_dirlist_cache(struct linkedlist_t *ll); //clear all elements
struct dirlist *add_dirlist_cache(struct linkedlist_t *ll, const char *rootlist, int key);
//struct linkedlist_t dirlist_head;

/****************************************************************
  Helper routines
 ****************************************************************/
int dp_get_next(DIR *dp, char *list);
static inline int root_cmp(char *oldroot, const char *newroot);
ino_t get_inode(const char *rootlist);
char *get_exec(const char *cmd);	//execute cmd
char *get_file(const char *file);
char *get_link(const char *link);
int set_dbname(HDB *hdbp, const char *list, int size, char *fullpath);
int set_rootlist(const char *root, const char *list, int size, char *rootlist);

#ifdef WITH_BDB_LOCKING 
#define WRITE_LOCK if(db_write_lock(hdbp)) return HDB_LOCK_ERROR;
#define WRITE_LOCK_DEFAULT if(db_write_lock(&hdb_default)) return HDB_LOCK_ERROR;
#define WRITE_LOCK_NULL if(db_write_lock(hdbp)) return NULL;
#define READ_LOCK if(db_read_lock(hdbp)) return HDB_LOCK_ERROR;
#define READ_LOCK_NULL if(db_read_lock(hdbp)) return NULL;
#define UN_LOCK db_unlock(hdbp);
#define UN_LOCK_DEFAULT db_unlock(&hdb_default);
#else
#define WRITE_LOCK
#define WRITE_LOCK_NULL
#define WRITE_LOCK_DEFAULT
#define READ_LOCK 
#define READ_LOCK_NULL 
#define UN_LOCK
#define UN_LOCK_DEFAULT 
#endif

#ifdef DB3_DB4
DB_ENV *hdb_dbenv=NULL; //if we don't use dbenv, we still need null to pass to functions
#endif

#ifdef WITH_BDB_ENVIROMENT 
DB_ENV *env_setup(char *home, char *data_dir, FILE *errfp, char *progname, int join);
int env_lock_fd=0;
int env_lock(char *file);
int env_open(int join);
void env_cleanup();
int check_env();
int env_unlock();
#endif

#ifdef WITH_BDB_LOCKING
int db_write_lock(HDB *hdbp);
int db_read_lock(HDB *hdbp);
int db_unlock(HDB *hdbp);
int db_lock(HDB *hdbp, int type);
#endif

int _hdb_lock(int fd);
int _hdb_lstat(HDB *hdbp, const char *list, int type);
int _hdbcur_to_dbcur(int cursor);
int _hdb_scan_sublist(HDB *hdbp, const char *parent_list, char ***listp, int fullpath, int sort);
void _hdb_unlock(int fd);
int _sync_restart(); //non locking sync version.. used internally
int _sync_close(); //non locking sync version.. used internally
int _hhdb_create_list(HDB *hdbp, const char *list);
char *_get(HDB *hdbp, const char *list, const char *key, int *type, HDBS *stat);
int _set(HDB *hdbp, const char *list, const char *key, const char *val, int type);
int _set_int(HDB *hdbp, const char *list, const char *key, int val);
int _get_int(HDB *hdbp, const char *list, const char *key);

/*
* REGEX handling 
*/
struct hdb_regexp_t {
	regex_t list;
	regex_t key;
	regex_t value;
	regmatch_t match_info;
};
typedef struct hdb_regexp_t hdb_regex;
int _dump_regex(HDB *hdbp, char *list, hdb_regex *re);
void _regex_free(hdb_regex *re);
int _print_list_regex(HDB *hdbp, int fmt, char *list, hdb_regex *re);

static int filterdir(const struct dirent *d){
	if(!strcmp(d->d_name, "." ) || !strcmp(d->d_name, "..") || d->d_type!=DT_DIR){
		return 0;
	}
	return 1;
}

//not used anymore...we use alphasort
/*
static int ino_compare(const void *a, const void *b){
	return ((*(const struct dirent **) a)->d_ino > (*(const struct dirent **) b)->d_ino);
}
*/

/**********************************************************
  Globals
 **********************************************************/
char username[255];
FILE *logfile;
int hdb_log_facility=0;
int lockfd;			//one lock at the time


/**********************************************************
  API Calls
 **********************************************************/

/**********************************************************
  Fetch version (network, light)
 **********************************************************/
int hdb_get_access(){
	return HDB_ACCESS_FILE;
}

/**********************************************************
 * Logfile routines 
 **********************************************************/
int hdb_open_log(int facility, char *name){

	char buf[4096];

	if(hdb_log_facility){
		hdb_close_log();
	}

	hdb_log_facility=facility;
	switch(facility){
		//TODO.. add HDB_LOGG_TTY
		case HDB_LOG_CONSOLE:
			break;
		case HDB_LOG_SYSLOG:
			openlog("hdb", LOG_PID,LOG_DAEMON);
			break;
		case HDB_LOG_FILE:
			if(name){
				strncpy(buf, name, sizeof(buf));
			}
			else {
				strcpy(buf, "/var/log/hdbd.log");		
			}
			//TODO.. should we add "a" append?
			if((logfile = fopen(buf, "w+"))==NULL){
				return HDB_OPEN_ERROR;
			}
			break;
		case HDB_LOG_HDB:
			break;
		default:
			hdb_log_facility=HDB_NONE;
			return 1;
	}
	return 0;
}

int hdb_print_log(const char *format, ...){

	va_list ap;

	switch(hdb_log_facility){
		case HDB_LOG_CONSOLE:
			va_start(ap, format);
			vprintf(format, ap);	
			break;
		case HDB_LOG_FILE:
			va_start(ap, format);
			vfprintf(logfile, format, ap);
			break;
		case HDB_LOG_SYSLOG:
			va_start(ap, format);
			vsyslog(LOG_INFO, format, ap);
			break;
		default:
			return HDB_ERROR;
	}

	return 0;
}

int hdb_close_log(){

	switch(hdb_log_facility){
		case HDB_LOG_FILE:
			fclose(logfile);
			break;
		case HDB_LOG_SYSLOG:
			closelog();
			break;
		default:
			hdb_log_facility=0;
			return 1;
	}

	hdb_log_facility=0;
	return 0 ;
}

/**********************************************************
 * Used in functions that return numbers or pointers. 
 **********************************************************/
int hdb_get_error()
{
	return dberrno;
}

int hhdb_get_error(HDB *hdbp){
	return hdbp->dberrno;
}

/**********************************************************
  return values for the selected parameter or -1 if not found
 **********************************************************/
int hdb_get_configi(int parameter){
	return hhdb_get_configi(&hdb_default, parameter);	
}

int hhdb_get_configi(HDB *hdbp, int parameter){
	switch (parameter) {
		case HDB_CONFIG_HASH_MAX_SIZE:
			return hdbt_hash_max_size;
		case HDB_CONFIG_HASH_SIZE:
			return hdbt_hash_size;
		case HDB_CONFIG_CACHE_TYPE:
			return hdb_cache_type;
		case HDB_CONFIG_LOG_FACILITY:
			return hdb_log_facility;
		case HDB_CONFIG_STAT:
			return hdbp->update_stat;
	}
	dberrno = HDB_INVALID_INPUT;
	return -1;
}

/**********************************************************
  set values for the selected parameter
 **********************************************************/
int hdb_set_configi(int parameter, int value){
	return hhdb_set_configi(&hdb_default, parameter, value);
}

int hhdb_set_configi(HDB *hdbp, int parameter, int value)
{
	if(parameter == HDB_CONFIG_HASH_MAX_SIZE) {
		hdbt_hash_max_size = value;
		if(hdbt_hash_max_size > hdbt_hash_size) {
			hdbt_hash_is_full = 0;
		}
		else {
			hdbt_hash_is_full = 1;
		}
	}
	else if(parameter == HDB_CONFIG_CACHE_TYPE) {
		hdb_cache_type = value;
	}
	else if(parameter == HDB_CONFIG_LOCK){
		hdb_lock_type = value;
	}
	else if (parameter == HDB_CONFIG_STAT){
		hdbp->update_stat = value;
	}
	else {
		return HDB_INVALID_INPUT;
	}
	return 0;
}


/**********************************************************
  Return our current root. must never fail.
 **********************************************************/
char *hdb_get_root(){
	return hhdb_get_root(&hdb_default);
}

char *hhdb_get_root(HDB *hdbp)
{
	if(!hdbp->root_is_ok) {
		hhdb_set_root(hdbp, getenv("HDBROOT"));
	}	
	assert(hdbp->root != NULL);
	return hdbp->root;
}

/**********************************************************
  To save username in logging.
  Currently not used.
 **********************************************************/
int hdb_set_user(char *user)
{
	snprintf(username, sizeof(username), user);
	return 0;
}

/**********************************************************
  Verify that the root is ok. Can be used by clients
 **********************************************************/
int hdb_verify_root(const char *root){
	if(root &&
	   strstr(root, "..") == NULL &&
	   strstr(root, ";") == NULL &&
	   *root != '~' &&
	   *root != '.' &&
	   strcmp(root, "") && 
	  (strstr(root, "hdb") != NULL)) 
	{
		return 0;
	}
	return 1;
}

/**********************************************************
  hdb_set_root - Set the current root directory for hdb.
  - default is HDBROOT=/var/db/hdb
  - Must never create fail to create a root.
  - If the input is invalid or we fail to set the requested 
  root use the default root and return an error.
 **********************************************************/
int hdb_set_root(const char *root)
{
	return hhdb_set_root(&hdb_default, root);
}

int hhdb_set_root(HDB *hdbp, const char *root)
{
	mode_t mode;
	char cmd[HDB_PATH_MAX];
	struct stat lstatbuf;
	int ret = 0;

	DBG("hdb_set_root - trying to set root to %s\n", root);

	//0. Open enviroment
	#if WITH_BDB_ENVIROMENT
	//open our enviroment if its not opened
	check_env();
	#endif

	//always print this to stdout by default
	hdbp->file = stdout;

	//always update timestamps
	hdbp->update_stat=HDB_ENABLE;

	//1. Check for valid input.
	if(!root) {
		//use default root if non provided and we dont have a root
		if(!hdbp->root_is_ok) {
			DBG("hdb_set_root - using default root\n");
			strncpy(hdbp->root, HDBROOT, sizeof(hdbp->root));
			hdbp->root_is_ok = 1;
		}
		return 0;
	}

	//2. Check if we have a new root.
	//root should start and end with a '/'
	//but.. this is not always the case
	if(!root_cmp(hdbp->root, root)) {
		hdbp->root_is_ok = 1;
		DBG("hdb_set_root - same root as before. return 0\n");
		return 0;
	}

	//3. check for invalid root
	if(hdb_verify_root(root)){
		DBG("hdb_set_root - root contains invalid characters. Using default\n");
		strncpy(hdbp->root, HDBROOT, sizeof(hdbp->root));
		ret = HDB_INVALID_INPUT;
	}
	else {
		strncpy(hdbp->root, root, sizeof(hdbp->root));
	}

	if(hdbp->root[strlen(hdbp->root) - 1] != '/') {
		strcat(hdbp->root, "/");
	}

	//4. Create the directory root structure if needed
	if(lstat(hdbp->root, &lstatbuf) == -1) {
		mode = umask(0);
#ifdef DEBUG
		snprintf(cmd, sizeof(cmd), "mkdir -m 777 -p %s", hdbp->root);
		DBG("hdb_set_root - running system cmd: %s\n", cmd);
#else
		snprintf(cmd, sizeof(cmd), "mkdir -m 777 -p %s > 2&>1 > /dev/null", hdbp->root);
#endif
		DBG("hdb_set_root - creating new root %s\n", hdbp->root);
		if(system(cmd)) {
			umask(mode);
			DBG("hdb_set_root - Unable to create hdbroot %s\n", hdbp->root);
			return HDB_INVALID_INPUT;
		}
		umask(mode);
	}

	DBG("hdb_set_root - done new root is %s\n", hdbp->root);
	hdbp->root_is_ok = 1;
	return ret;
}

/**********************************************************
  hdb_check_lock - return 0 (file is locked)
  - return !0 file is not locked or on other error
  This is used for global lockfiles.. hence no HDBROOT
 **********************************************************/
int hdb_check_lock(const char *file){
	
	int fd;
	int ret=0;

	if(file == NULL){
		return HDB_INVALID_INPUT;
	}

	if((fd = open(file, O_RDONLY)) == -1){
		DBG("hdb_lock - unable to open file %s\n", file);
		return HDB_OPEN_ERROR;
	}

	//lockfd return 0 and hdb_ckeck_lock return 1 
	if((lockf(fd, F_TEST, 0)==0)){
		ret=1;
	}
	close(fd);
	return ret;
}


/**********************************************************
  hdb_lock - Wait max timeout seconds and lock list
 **********************************************************/
int hdb_lock(const char *list, int timeout)
{
	int t = 0;
	char *l = NULL;
	char buf[HDB_PATH_MAX];
	int ret;

	if(!hdb_default.root_is_ok && hhdb_set_root(&hdb_default, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	//1. Make sure we have the list
	if((ret = hhdb_create_list(&hdb_default, list))) {
		return ret;
	}

	l = strdup(list);
	snprintf(buf, sizeof(buf), "%s%s/%s.db", hdb_default.root, list, basename(l));
	free(l);

	DBG("hdb_lock - locking file %s (list=%s)\n", buf, list);

	if((lockfd = open(buf, O_RDWR)) == -1) {
		DBG("hdb_lock - unable to open file %s\n", buf);
		return HDB_OPEN_ERROR;
	}
	if(timeout == 0) {
		if(!_hdb_lock(lockfd)) {
			return 0;
		}
	}
	else {
		if(t < 0) {
			t = 300;	//WAIT FOREVER == 5 minutes
		}
		t = time(NULL) + timeout;
		while(t > time(NULL)) {
			if(!flock(lockfd, LOCK_EX)) {
				return 0;
			}
		}
	}

	DBG("hdb_lock - Failed to lock file %s timeout=%i\n", buf, timeout);

	return HDB_ERROR;
}

/**********************************************************
  hdb_unlock - unlock lockfd.
 **********************************************************/
int hdb_unlock(const char *list)
{

	//TODO.. add support for list locking
	_hdb_unlock(lockfd);
	if(lockfd != -1) {
		close(lockfd);
	}

	return 0;
}

/**********************************************************
  hdb_incr - atomic++ operation. 
 **********************************************************/
int hdb_incr(const char *list, const char *key){
	return hhdb_incr(&hdb_default, list, key);
}

int hhdb_incr(HDB *hdbp, const char *list, const char *key){
	return hhdb_add(hdbp, list, key, 1);
}

/**********************************************************
  hdb_sync - sync data to disk by closing all database file pointers.
 **********************************************************/
int hdb_sync(){
	if(!hdb_default.root_is_ok && hhdb_set_root(&hdb_default, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}
	return hhdb_sync(&hdb_default);
}

int hhdb_sync(HDB *hdbp){
	int ret=0;
	WRITE_LOCK
	ret=_sync_restart();
	DBG("calling db_unlock()\n"); //testing
	UN_LOCK
	return ret;
}

int _sync_restart(){
	_sync_close();
	initialize_hdbt();
	return 0;
}
//internal sync procedure 
int _sync_close()
{

	DBT key, data;
	HDBT *hdbt;
#if DB3_DB4
	DBC *dbcp;
#endif
	int ret = 0;
	int i = 0;
	DB *dbp = NULL;
	DB *hp = NULL;
	struct hash_hive *h = NULL;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	DBG("hdb_sync\n");

	//go through the database pointer hash and close all open databases.
	//NOTE.. closing dbp twice will coredump in bdb185.
	//NOTE.. dbp can be found in several caches. 
	if(hdb_cache_type == HDB_CACHE_HASH && hdbt_hash && hdbt_hash->dbp) {
		DBG("hdb_sync - sync and close hash hdbt_hash %p\n", hdbt_hash); 
		hp=hdbt_hash->dbp;
#ifdef DB3_DB4
		if((ret = hp->cursor(hp, NULL, &dbcp, 0)) != 0) {
			DBG("hdb_sync - cursor error %s\n", db_strerror(ret));
			assert(0);
			return HDB_CURSOR_ERROR;
		}
		ret = dbcp->c_get(dbcp, &key, &data, DB_FIRST);
#else
		ret = (*hp->seq) (hp, &key, &data, R_FIRST);
#endif
		while(ret == 0) {
			if(!data.data) {
				assert(0);
				continue;
			}
			h = data.data;
			if(!h->hdbt) {
				assert(0);
				continue;
			}
			/*
			   DBG("hdb_sync - dbp_sync %i %s %p hdbt_last %p\n", i,
			   list, h->dbp, hdbt_last);
			 */
			i++;
			dbp = h->hdbt->dbp;
			hdbt = h->hdbt;

			//so we null the other one element cache to prevent core dump
			if(hdbt_last && dbp == hdbt_last->dbp) {
				DBG("hdb_sync - found hdbt_last in hash ergo no need to close it\n");
				hdbt_last = NULL;
			}

			//TODO.. do we need to sync first?
			/*
			   if(dbp->sync(dbp, 0)) {
			   DBG("hdb_sync - failed to sync %p\n", dbp);
			   assert(0);
			   }
			 */
			if(close_hdbt(hdbt)){
				DBG("failed to close hdbt %p\n", hdbt);
			}
			hdbt=NULL;
#if defined (DB3_DB4)
			ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
#else
			ret = (*hp->seq) (hp, &key, &data, R_NEXT);
#endif
		}
		DBG("_sync() - closing hdbt_hash %p\n", (void*)hdbt_hash);
		close_hdbt(hdbt_hash);
		hdbt_hash=NULL;
		dbps_insert = 0;
		hdbt_hash_size = 0;
		hdbt_hash_is_full = 0;

	}
	//if we didnt find our hdbt_last in the hash.. close it also
	//or we dont use hash
	if(hdbt_last) {
		DBG("hdb_sync - closing hdbt_last %p\n", hdbt_last);
		/*
		   if((ret=hdbt_last->sync(hdbt_last, 0))) {
		   DBG("hdb_sync - failed to sync hdbt_last %s\n", db_strerror(ret));
		   assert(0);
		   }
		 */

		if(close_hdbt(hdbt_last)){
			DBG("hdb_sync - failed to close hdbt %p\n", hdbt_last);
			assert(0);
		}
		hdbt_last=NULL;
		hdbt_last_id=0;
	}
	DBG("_sync() - done hdbt_hash %p\n", (void*)hdbt_hash);
	return 0;
}

/**********************************************************
  hdb_mv - move (rename) hdb three  
 **********************************************************/
int hdb_mv(const char *list, const char *dest){
	return hhdb_mv(&hdb_default, list, dest);
}

int hhdb_mv(HDB *hdbp, const char *list, const char *dest)
{
	char src_rootlist[HDB_PATH_MAX];
	char dst_rootlist[HDB_PATH_MAX];
	char cmd[HDB_PATH_MAX];
	char *l = NULL;

	if(!list || !dest) {
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	WRITE_LOCK
	set_rootlist(hdbp->root, list, sizeof(src_rootlist), src_rootlist);
	set_rootlist(hdbp->root, dest, sizeof(dst_rootlist), dst_rootlist);

	DBG("hdb_mv - src %s dst %s\n", src_rootlist, dst_rootlist);
	if(_hhdb_create_list(hdbp, dest)) {
		DBG("hdb_mv - failed to create %s\n", dst_rootlist);	
		UN_LOCK
		return HDB_ERROR;
	}

	//TODO.. add hdb_sync_tree(const char *list)
	_sync_restart();

	l = strdup(src_rootlist);
	snprintf(cmd, sizeof(cmd), "mv %s %s/%s", 
	src_rootlist, dst_rootlist, basename(l));
	free(l);
	
	DBG("hdb_mv running cmd %s\n", cmd);
	if(system(cmd)) {
		DBG("hdb_mv - system command failed\n");
		UN_LOCK
		return HDB_SHELL_ERROR;
	}
	UN_LOCK
	if(use_trigger) {
		trigger(HDB_TRIG_MOVE, list, dest, NULL, NULL);
	}
	return 0;
}

/**********************************************************
  hdb_get_nval - copy not more than size characters to val.
 **********************************************************/
int hdb_get_nval(const char *list, const char *key, int size, char *val)
{
	return hhdb_get_nval(&hdb_default, list, key, size, val);
}

int hhdb_get_nval(HDB *hdbp, const char *list, const char *key, int size, char *val)
{
	char *value = NULL;

	if((value = hhdb_get_val(hdbp, list, key)) == NULL) {
		strcpy(val, "");
		return HDB_ERROR;
	}

	strncpy(val, value, size);
	free(value);
	return 0;
}

/**********************************************************
  hdb_get_pval - returns static char. so no freeing.
  constrained by HDB_VALUE_MAX
 **********************************************************/
char *hdb_get_pval(const char *list, const char *key)
{
	static char buf[HDB_VALUE_MAX];
	hdb_get_nval(list, key, HDB_VALUE_MAX, buf);
	return buf;
}

/**********************************************************
  hdb_get_val - returns malloced buffer. Needs to be freed. 
  If the key is a meta key. Return the underlying value.
 **********************************************************/
char *hdb_get_val(const char *list, const char *key)
{
	return hhdb_get_val(&hdb_default, list, key);
}

char *hhdb_get_val(HDB *hdbp, const char *list, const char *key)
{

	int type = HDB_TYPE_STR;
	char *val = NULL;
	char *meta_val = NULL;
	val = hhdb_get_raw(hdbp, list, key, &type);

	if(type == HDB_TYPE_EXEC) {
		meta_val = get_exec(val);
		free(val);
		return meta_val;
	}
	else if(type == HDB_TYPE_LINK) {
		meta_val = get_link(val);
		free(val);
		return meta_val;
	}
	else if(type == HDB_TYPE_FILE) {
		meta_val = get_file(val);
		free(val);
		return meta_val;
	}

	return val;
}

int hdb_list_stat(const char *list, HDBS *stat){
	return hhdb_list_stat(&hdb_default, list, stat);
}

int hhdb_list_stat(HDB *hdbp, const char *list, HDBS *stat){
	char rootlist[HDB_PATH_MAX];
	struct stat buf;

	assert(stat);
	
	stat->atime=stat->mtime=-1;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);
	if((lstat(rootlist, &buf) == -1)) {
		DBG("hhdb_exist - list %s does NOT exist\n", rootlist);
		return HDB_ERROR;
	}
		
	stat->atime = buf.st_atime;
	stat->mtime = buf.st_mtime;

	return 0;
}

int hdb_stat(const char *list, const char *key, HDBS *stat){
	return hhdb_stat(&hdb_default, list, key, stat);
}

int hhdb_stat(HDB *hdbp, const char *list, const char *key, HDBS *stat){
	int type=0;

	assert(stat);

	stat->mtime=stat->atime=-1;

	READ_LOCK
	if(_get(hdbp, list, key, &type, stat)){
		UN_LOCK
		return 0;
	}
	UN_LOCK
	return 1;
}

/**********************************************************
  hdb_get_raw - returns malloced buffer that needs to be freed.
  Setting the type (HDB_TYPE_STR, HDB_TYPE_LINK etc) int *type
 **********************************************************/
char *hdb_get_raw(const char *list, const char *key, int *type)
{
	return hhdb_get_raw(&hdb_default, list, key, type);
}

char *hhdb_get_raw(HDB *hdbp, const char *list, const char *key, int *type)
{
	char *result=NULL;
	WRITE_LOCK_NULL
	result=_get(hdbp, list, key, type, NULL);
	UN_LOCK
	return result;
}

char *_get(HDB *hdbp, const char *list, const char *key, int *type, HDBS *stat){
	DBT k, data;
	int ret;
	char *result = NULL;
	HDBT *hdbt = NULL;
	DB *dbp = NULL;
	struct hive_t hive;

	DBG("hdb_get_val - list %s key %s\n", list, key);
	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		hdbp->dberrno = dberrno = HDB_ROOT_ERROR;
		return NULL;
	}


	if((hdbt = get_hdbt(hdbp->root, list)) == NULL) {
		return NULL;
	}
	assert(hdbt->dbp);
	dbp = hdbt->dbp;

	memset(&k, 0, sizeof(k));
	memset(&data, 0, sizeof(data));
	memset(&hive, 0, sizeof(struct hive_t));

	k.data = (void *) key;
	k.size = strlen(key);
#ifdef HDBD
	data.flags = DB_DBT_MALLOC;
#endif 

	ret = dbp_get(dbp,&k,&data);
	if(ret==0){
		if(data.size > 0) {

			unpack_hive(data.data, &hive);

			*type = hive.meta[META_RAW];
			if(stat){
				stat->mtime = hive.meta[META_MTIME];
				stat->atime = hive.meta[META_ATIME];
			}
			//used internally
			hdbp->mtime = hive.meta[META_MTIME];
			hdbp->atime = hive.meta[META_ATIME];

			//the hives are of variable size
			if((result = malloc(hive.size + 1)) == NULL) {
				//TODO.. anyone uses dberrno
				hdbp->dberrno = dberrno = HDB_MALLOC_ERROR;
				return NULL;
			}
			strncpy(result, hive.value, hive.size);
			result[hive.size] = '\0';
			DBG("hdb_get_raw - copying result size=%i result='%s'\n", hive.size, result);
		}
		else {
			DBG("hdb_get_raw - HIVE_ERROR\n");

			hdbp->dberrno = dberrno = HDB_HIVE_ERROR;
			return NULL;
		}
	}
	else {
#ifdef DB3_DB4
		DBG("hdb_get_raw - DBP_ERROR %s\n", db_strerror(ret));
#else
		DBG("hdb_get_raw - DBP_ERROR %i\n", ret);
#endif
		hdbp->dberrno = dberrno = HDB_DBP_ERROR;
		return NULL;
	}

	//update mtime.. this is another write
	if(!stat && hdbp->update_stat == HDB_ENABLE){
		hive.meta[META_MTIME] = time(NULL);
		if((ret = dbp_put(dbp, &k, &data))){
			hdbp->dberrno = dberrno = HDB_DBP_ERROR;
			result=NULL;
		}
	}

	return result;
}

/**********************************************************
  hdb_get_bool - Returns 1 on "True", "Yes", "On", !0 values.
 **********************************************************/
int hdb_get_bool(const char *list, const char *key)
{
	char bufval[5];

	if(hdb_get_nval(list, key, 5, bufval) == 0) {
		if(!strcasecmp(bufval, "true") ||
		   !strcasecmp(bufval, "yes") ||
		   !strcasecmp(bufval, "on") || 
		   atoi(bufval)) {
			return 1;
		}
	}

	return 0;
}

/**********************************************************
  hdb_get_int - Does not use dberrno, return 0 if key not found
 **********************************************************/
int hdb_get_int(const char *list, const char *key){
	return hhdb_get_int(&hdb_default, list, key);
}

int hhdb_get_int(HDB *hdbp, const char *list, const char *key)
{
	int ret=0;
	READ_LOCK
	ret =_get_int(hdbp, list, key);
	UN_LOCK
	return ret;
}

int _get_int(HDB *hdbp, const char *list, const char *key)
{
	char *value = NULL;
	int ivalue = 0;
	int type=0;

	if((value = _get(hdbp, list, key, &type, NULL)) != NULL) {
		ivalue = atoi(value);
		free(value);
	}

	return ivalue;
}

/**********************************************************
  hdb_set_int
 **********************************************************/
int hdb_set_int(const char *list, const char *key, int val){
	return hhdb_set_int(&hdb_default, list, key, val);
}

int hhdb_set_int(HDB *hdbp, const char *list, const char *key, int val){
	int ret=0;
	READ_LOCK
	ret=_set_int(hdbp,list,key,val);
	UN_LOCK
	return ret;
}
int _set_int(HDB *hdbp, const char *list, const char *key, int val)
{
	char value[255];
	snprintf(value, sizeof(value), "%i", val);
	return _set(hdbp, list, key, value, 0);
}

/**********************************************************
  hdb_get_long
 **********************************************************/
long long hdb_get_long(const char *list, const char *key)
{
	char *value = NULL;
	long long ivalue = 0;

	if((value = hdb_get_val(list, key)) != NULL) {
		ivalue = atoll(value);
		free(value);
	}

	return ivalue;
}

/**********************************************************
  hdb_set_long
 **********************************************************/
int hdb_set_long(const char *list, const char *key, long long val)
{
	char value[255];
	snprintf(value, sizeof(value), "%lld", val);
	return hdb_set_val(list, key, value);
}

/**********************************************************
  hdb_add - Atomic add operation
  Use negative values to subtract.
 **********************************************************/
int hdb_add(const char *list, const char *key, int value){
	return hhdb_add(&hdb_default, list, key, value);
}

int hhdb_add(HDB *hdbp, const char *list, const char *key, int value)
{
	int i;
#if !defined(HDBD) && !defined(WITH_BDB_LOCKING)
	int fd = 0;
	char dbname[HDB_PATH_MAX];
#endif
	int result = 0;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		DBG("hdb_add - set_hdb_root failed\n");
		return HDB_ROOT_ERROR;
	}


	//we have to make sure that the list exist
	//if it is the first request dbname is NULL so locking will fail
	if(hhdb_create_list(hdbp, list)) {
		DBG("hdb_add - unable to create list %s\n", list);
		return HDB_ERROR;
	}

	//HDBD controls looking so we dont need that.
#if defined(HDBD) || defined(WITH_BDB_LOCKING)
	WRITE_LOCK
	i = _get_int(hdbp, list, key) + value;
	result = _set_int(hdbp, list, key, i);
	UN_LOCK
#else
	if(set_dbname(hdbp, list, sizeof(dbname), dbname)){
		return HDB_ERROR;
	}

	DBG("hdb_add - locking dbname %s\n", dbname);

	if((fd = open(dbname, O_RDWR)) == -1) {
		DBG("hdb_add - unable to open file %s\n", dbname);
	}

	if(_hdb_lock(fd)) {
		DBG("hdb_add - unable to get lock\n");
		return HDB_ERROR;
	}
	_sync_restart();
	i = hhdb_get_int(hdbp, list, key) + value;
	result = hhdb_set_int(hdbp, list, key, i);
	_sync_restart();
	_hdb_unlock(fd);
	if(fd != -1) {
		close(fd);
	}
#endif
	return result;
}

/**********************************************************
  hdb_set_file - make key a file. Will later return cat file | head -1
 **********************************************************/
int hdb_set_file(const char *list, const char *key, const char *file)
{
	return hdb_set_raw(list, key, file, HDB_TYPE_FILE);
}

/**********************************************************
  hdb_set_exec - will return sh -x key | head -1
 **********************************************************/
int hdb_set_exec(const char *list, const char *key, const char *cmd)
{
	return hdb_set_raw(list, key, cmd, HDB_TYPE_EXEC);
}

/**********************************************************
  hdb_set_link - A link is a key that refers to another key
 **********************************************************/
int hdb_set_link(const char *list, const char *key, const char *link)
{
	return hdb_set_raw(list, key, link, HDB_TYPE_LINK);
}

/**********************************************************
  hdb_set_val
 **********************************************************/
int hhdb_set_val(HDB *hdbp, const char *list, const char *key, const char *val){
	return hhdb_set_raw(hdbp, list, key, val, HDB_TYPE_STR);
}

int hdb_set_val(const char *list, const char *key, const char *val)
{
	return hdb_set_raw(list, key, val, HDB_TYPE_STR);
}

int hdb_set_raw(const char *list, const char *key, const char *val, int type)
{
	return hhdb_set_raw(&hdb_default, list, key, val, type);
}

int hhdb_set_raw(HDB *hdbp, const char *list, const char *key, const char *val, int type){
	int ret=0;
	WRITE_LOCK
	ret=_set(hdbp,list,key,val,type);
	UN_LOCK
	return ret;
}

int _set(HDB *hdbp, const char *list, const char *key, const char *val, int type)
{
	DBT k, data;
	char oldvalue[HDB_VALUE_MAX];
	int bufflen = 0;
	char *databuff = NULL;
	int ret = 0;
	DB *dbp = NULL;
	HDBT *hdbt = NULL;
	struct hive_t h;
	int meta[META_SIZE];

	DBG("hhdb_set_raw - root=%s list=%s key=%s value=%s\n", hdbp->root, list,
			key, val);

	if(!list) {
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	//hdbt must not be closed by other thread 
	if((hdbt = get_hdbt(hdbp->root, list)) == NULL) {
		//Auto create the list
		if(_hhdb_create_list(hdbp, list)) {
			DBG("hdb_set_raw - ERR hdb_create_list failed\n");
			return HDB_ERROR;
		}
		//use the newly created list
		if((hdbt = get_hdbt(hdbp->root, list)) == NULL) {
			DBG("hdb_set_raw - ERR could not find newly created list\n");
			return HDB_DBP_ERROR;
		}
	}
	assert(hdbt->dbp);
	dbp = hdbt->dbp;

	//find old value.. we need to send that in the event
	if(use_trigger) {
		//TODO.. catch errors needed here? Do we care?
		//TODO.. any locking problems here????
		hdb_get_nval(list, key, sizeof(oldvalue), oldvalue);
	}

	/* Zero out the DBTs before using them. */
	memset(&k, 0, sizeof(k));
	memset(&data, 0, sizeof(data));

	k.data = (void *) key;
	k.size = strlen(key);
#ifdef HDBD
	data.flags = DB_DBT_MALLOC;
#endif

	/* Create our hive with data */
	h.value = (char *)val;
	h.size = strlen(val);

	memset(&meta, 0, sizeof(meta));	
	meta[META_RAW] = type;
	meta[META_PROTO] = HDB_PROTO;
	if(hdbp->update_stat == HDB_ENABLE){
		meta[META_MTIME] = meta[META_ATIME] = time(NULL);
	}

	/* Some of the structures data is on the stack, and some
	 * is on the heap. To store this structure we need to marshall
         * it -- pack it all into a single location in memory
         */
	
	/* create the buffer */
	data.size = sizeof(meta) + sizeof(int) + h.size + 1;
	databuff = malloc(data.size);
	memset(databuff, 0, data.size);

	/* copy everything to the buffer */
	memcpy(databuff, &(h.size), sizeof(int));
	bufflen = sizeof(int);
	memcpy(databuff + bufflen, meta, sizeof(meta));
	bufflen += sizeof(meta);
	memcpy(databuff + bufflen, h.value, h.size + 1);
	bufflen += h.size + 1;

	/* store it */
	data.data = databuff;

	DBG("hdb_set_raw - saving value=%s type=%i size=%i\n", h.value,
			meta[META_RAW], h.size);

	ret = dbp_put(dbp, &k, &data);
	free(databuff);

	if(ret){
		return HDB_BDB_ERROR;
	}

	//TODO.. valgrind complains about this. 
	//TODO.. this is slow. causes alot of disc writes
#if !defined(DB3_DB4)
	if(hdb_cache_type == HDB_CACHE_NONE) {
		//dbp->sync(dbp, 0);
		dbp_sync(dbp);
	}
#endif

	if(use_trigger) {
		trigger(HDB_TRIG_WRITE, list, key, oldvalue, val);
	}

	return 0;
}

/**********************************************************
  hdb_wipe - Delete the whole database. rm -rf
 **********************************************************/
int hdb_wipe(){
	return hhdb_wipe(&hdb_default);
}

int hhdb_wipe(HDB *hdbp)
{
	char db[HDB_PATH_MAX];
	int ret=0;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}
	
	WRITE_LOCK

	//close and sync all filepointers
	if(_sync_restart()) {
		assert(0);
		ret=HDB_SYNC_ERROR;
		goto cleanup;
	}

	//TODO.. make test to make sure root is a hdb root
	//TODO.. add check for .db files?
	snprintf(db, sizeof(db), "rm -rf %s*", hdbp->root);
	DBG("hdb_wipe - system(%s)\n", db);
	if(system(db)) {
		assert(0);
		ret=HDB_SHELL_ERROR;
		goto cleanup;
	}

	if(use_trigger) {
		//TODO.. how to explain that the whole database is wiped
		trigger(HDB_TRIG_DELETE, "/", NULL, NULL, NULL);
	}
cleanup:
	UN_LOCK
	return 0;
}

/**********************************************************
  hdb_del_val - deletes key and value in list
**********************************************************/
int hdb_del_val(const char *list, const char *key){
	return hhdb_del_val(&hdb_default, list, key);
}

int hhdb_del_val(HDB *hdbp, const char *list, const char *key)
{
	DBT k;
	int ret;
	DB *dbp;
	HDBT *hdbt;
	char oldvalue[HDB_VALUE_MAX];

	DBG("hdb_del_val - list=%s key=%s\n", list, key);

	if(!list) {
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	WRITE_LOCK
	if((hdbt = get_hdbt(hdbp->root, list)) == NULL) {
		DBG("hdb_del_val - failed to get dbp\n");
		UN_LOCK
		return HDB_DBP_ERROR;
	}
	assert(hdbt->dbp);
	dbp = hdbt->dbp;

	//send old value with trigger
	if(use_trigger) {
		hdb_get_nval(list, key, sizeof(oldvalue), oldvalue);
	}

	memset(&k, 0, sizeof(k));
	k.data = (void *) key;
	k.size = strlen(key);
	
	ret = dbp_del(dbp, &k);
	if(ret==0){
		DBG("hdb_del_val - %s key was deleted\n", (char *) k.data);
	}
	else {
		DBG("hdb_del_val - BDB_ERROR\n");
		UN_LOCK
		return HDB_BDB_ERROR;
	}

	if(use_trigger) {
		trigger(HDB_TRIG_DELETE, list, key, oldvalue, NULL);
	}

#ifndef DB3_DB4
	dbp_sync(dbp);	
#endif
	UN_LOCK
	return 0;
}


/**********************************************************
  hdb_get_sublist_full - return the recno (1 = first sublist)
  number sublist with full path.
 **********************************************************/
int hdb_get_sublist_full(const char *parent_list, int recno, char *list){
	return hhdb_get_sublist_full(&hdb_default, parent_list, recno, list);
}

int hhdb_get_sublist_full(HDB *hdbp, const char *parent_list, int recno, char *list)
{
	int i = 0;
	char buf[HDB_PATH_MAX];
	strcpy(buf, list);
	i = hhdb_get_sublist(hdbp, parent_list, recno, buf);
	if(i == 0) {
		snprintf(list, sizeof(buf), "%s/%s", parent_list, buf);
	}
	return i;
}

/**********************************************************
  hdb_get_sublist - return the recno (1 = first sublist)
  Linear search which is slow in big sublists.
 **********************************************************/
int hdb_get_sublist(const char *parent_list, int recno, char *list){
	return hhdb_get_sublist(&hdb_default, parent_list, recno, list);
}

int hhdb_get_sublist(HDB *hdbp, const char *parent_list, int recno, char *list)
{
	char plbuf[HDB_PATH_MAX];
	struct dirent **namelist;
	int numlists = 0;
	int result = 1;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	if(recno<=0){
		return HDB_INVALID_INPUT;
	}

	strcpy(list, "");

	snprintf(plbuf, sizeof(plbuf), "%s%s", hdbp->root, parent_list);

	if((numlists = scandir(plbuf, &namelist, filterdir, alphasort)) == -1){
		DBG("hdb_get_sublist - can't open parent_list %s fullpath %s\n", parent_list, plbuf);
		return HDB_OPEN_ERROR;
	}

	if(recno <= numlists){ 
		strncpy(list, namelist[recno-1]->d_name, HDB_LIST_MAX);
		result=0;	
	}

	while(numlists--){
		free(namelist[numlists]);
	}
	free(namelist);

	return result;

}

/********************************************************** 
  hdb_get_sublist_cur_full - Get cursor to the sublist with full path.
 **********************************************************/
int hdb_get_sublist_cur_full(const char *parent_list, int cursor, char *list){
	return hhdb_get_sublist_cur_full(&hdb_default, parent_list, cursor, list);
}

int hhdb_get_sublist_cur_full(HDB *hdbp, const char *parent_list, int cursor, char *list)
{
	int i = 0;
	char buf[HDB_PATH_MAX];

	DBG("hhdb_get_sublist_cur_full parent_list=%s cursor=%i\n", parent_list, cursor);

	if(!list || !parent_list) {
		DBG("hdb_get_sublist_cur_full - parent_list %s, list %s\n",
				parent_list, list);
		return HDB_INVALID_INPUT;
	}
	strncpy(buf, list, sizeof(buf));

	i = hhdb_get_sublist_cur(hdbp, parent_list, cursor, buf);
	if(i == 0) {
		if(!strcmp(parent_list, "")){
			snprintf(list, HDB_PATH_MAX, "%s", buf);
		}
		else {
			snprintf(list, HDB_PATH_MAX, "%s/%s", parent_list, buf);
		}
	}

	return i;
}

#ifdef DB3_DB4
int get_dbc(HDBC *hdbc, char *key, char *value){
	int ret;
	struct hive_t hive;
	char *p;
	int type;
	DBT k, data;
	DBC *dbcp = NULL; 
	assert(hdbc && hdbc->dbcp);

	dbcp = (DBC *)hdbc->dbcp;

	assert(value && key);
	strcpy(value, "");
	strcpy(key, "");
	memset(&k, 0, sizeof(k));
	memset(&data, 0, sizeof(data));

	//READ_LOCK
	ret = dbcp->c_get(dbcp, &k, &data, DB_NEXT);
	//UN_LOCK
	if(ret==0){
		if(data.size > 0) {
			unpack_hive(data.data, &hive);
			type = hive.meta[META_RAW];
			hdbc->mtime = hive.meta[META_MTIME];
			hdbc->atime = hive.meta[META_ATIME];

			if(type == HDB_TYPE_STR || type == HDB_TYPE_INT) {
				strncpy(value, hive.value, hive.size);
				value[hive.size] = '\0';
			}
			else if(type == HDB_TYPE_LINK) {
				if((p = get_link(hive.value))) {
					strncpy(value, p, HDB_VALUE_MAX);
				}
			}
			else if(type == HDB_TYPE_EXEC) {
				if((p = get_exec(hive.value))) {
					strncpy(value, p, HDB_VALUE_MAX);
				}
			}
			else if(type == HDB_TYPE_FILE) {
				if((p = get_file(hive.value))) {
					strncpy(value, p, HDB_VALUE_MAX);
				}
			}
		}
		if(k.size > 0) {
			strncpy(key, (char *) k.data, HDB_KEY_MAX);
			assert(k.size < HDB_KEY_MAX);
			key[k.size] = '\0';	//avoid trailing trash
		}
		ret = 0;
	}
	else {
		ret = HDB_KEY_NOT_FOUND;
	}
	return ret;
}
#endif

int hdb_cget(HDBC *hdbc, char *list, char *value){
	DBG("hdb_cget() - hdbc=%p\n", hdbc);
#ifdef DB3_DB4
	assert(hdbc);
	return get_dbc(hdbc, list, value); 
#else
	return 1;
#endif
}

int hdb_cclose(HDBC *hdbc){
	DBG("hdb_cclose() - hdbc=%p\n", hdbc);
#ifdef DB3_DB4
	int ret=0;
	assert(hdbc && hdbc->dbcp);
	//WRITE_LOCK
	ret = close_dbc(hdbc->dbcp);	
	dbp_close(hdbc->dbp);
	//UN_LOCK
	hdbc->dbcp=NULL;
	hdbc->dbp=NULL;
	free(hdbc);
	return ret;
#else
	return 1;
#endif
}
HDBC *hdb_copen(HDBC *hdbc, char *list){
	return hhdb_copen(&hdb_default, hdbc, list);
}

HDBC *hhdb_copen(HDB *hdbp, HDBC *hdbc, char *list){
#ifdef DB3_DB4
	char rootlist[HDB_PATH_MAX];
	//HDBT *hdbt=NULL;
	DBG("hhdb_copen() - hdbc=%p list=%s\n", hdbc, list);

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp,getenv("HDBROOT"))) {
		hdbp->dberrno = dberrno = HDB_ROOT_ERROR;
		return NULL;
	}
	if(hhdb_exist(hdbp,list)){
		hdbp->dberrno = dberrno = HDB_LIST_NOT_FOUND;
		return NULL;
	}
	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);
	hdbc = malloc (sizeof(HDBC));
	memset(hdbc, 0, sizeof(HDBC));
	WRITE_LOCK_NULL
	//dir could be there (hdb-exist but .db file is gone)
	hdbc->dbp = open_db2(rootlist, 0); //if its not there.. dont open

	if(hdbc->dbp == NULL){
		UN_LOCK
		hdbp->dberrno = dberrno = HDB_ERROR;
		free(hdbc);
		return NULL;
	}

	hdbc->dbcp = create_dbcp(hdbc->dbp);
	if(hdbc->dbcp==NULL){
		dbp_close(hdbc->dbp);
		hdbc->dbp=NULL;
		UN_LOCK
		hdbp->dberrno = dberrno = HDB_ERROR;
		free(hdbc);
		return NULL;
		
	}
	UN_LOCK
	return hdbc;
#else
	return NULL;
#endif
}

HDBC *hdb_sublist_copen(HDBC *hdbc, char *list){
	return hhdb_sublist_copen(&hdb_default, hdbc, list);
}

HDBC *hhdb_sublist_copen(HDB *hdbp, HDBC *hdbc, char *list){

	char rootlist[HDB_PATH_MAX];

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		hdbp->dberrno = dberrno = HDB_ROOT_ERROR;
		return NULL;
	}

	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);

	hdbc = malloc(sizeof(HDBC));

	if((hdbc->dp = opendir(rootlist)) == NULL) {
		DBG("hdb_sublist_copen - cant open %s return NULL\n", rootlist);
		hdbp->dberrno = dberrno = HDB_OPEN_ERROR;
		free(hdbc);
		return NULL;
	}

	hdbc->parent_list = malloc(strlen(list)+1);
	strcpy(hdbc->parent_list, list);

	return hdbc;
}


int hdb_sublist_cget(HDBC *hdbc, char *list, int size){
	assert(hdbc);
	assert(hdbc->dp);

	return dp_get_next(hdbc->dp, list);
}

int hdb_sublist_cget_full(HDBC *hdbc, char *list, int size){
	int ret;
	char buf[HDB_LIST_MAX];

	ret = hdb_sublist_cget(hdbc, buf, sizeof(buf));	
	if(ret == 0){
		if(!strcmp(hdbc->parent_list, "") || !strcmp(hdbc->parent_list, ".")){
			snprintf(list, size, "%s", buf);
		}
		else {
			snprintf(list, size, "%s/%s", hdbc->parent_list, buf);
		}
	}

	return ret;
}

int hdb_sublist_cclose(HDBC *hdbc){
	assert(hdbc && hdbc->dp && hdbc->parent_list);

	closedir(hdbc->dp);
	free(hdbc->parent_list);
	free(hdbc);
	
	return 0;
}

int hdb_scan_sublist_full(const char *parent_list, char ***listp){
	return _hdb_scan_sublist(&hdb_default, parent_list, listp, 1, 0);
}

int hdb_scan_sublist(const char *parent_list, char ***listp){
	return  _hdb_scan_sublist(&hdb_default, parent_list, listp, 0, 0);
}

int hhdb_scan_sublist_full(HDB *hdbp, const char *parent_list, char ***listp){
	return _hdb_scan_sublist(hdbp, parent_list, listp, 1, 0);
}

int hhdb_scan_sublist(HDB *hdbp, const char *parent_list, char ***listp){
	return  _hdb_scan_sublist(hdbp, parent_list, listp, 0, 0);
}

int _hdb_scan_sublist(HDB *hdbp, const char *parent_list, char ***listp, int fullpath, int sort){
	char rootlist[HDB_PATH_MAX];
	int numlists=0;
	int i = 0;
	struct dirent **namelist;
	char **list;
	int listlen = 0;

	DBG("hdb_scan_sublist - %s\n", parent_list);
	assert(parent_list);
	
	if(fullpath && !strcmp(parent_list, "")){
		fullpath=0;
	}

	//show we keep this here???
	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}
		
	set_rootlist(hdbp->root, parent_list, sizeof(rootlist), rootlist);

	if((numlists = scandir(rootlist, &namelist, filterdir, alphasort)) == -1){
		return -1;
	}

	//we alloc numlists+1 since we want to make the last element NULL for while loops etc 
	list=malloc(sizeof(char*)*(numlists+1));
		
	for(i=0;i<numlists;i++){
		if(fullpath)
		{
			//add extra byte for '/'
			listlen = strlen(namelist[i]->d_name) + strlen(parent_list) + 2;
			list[i] = malloc(listlen);
			snprintf(list[i], listlen, "%s/%s", parent_list, namelist[i]->d_name);
		}
		else {
			listlen = strlen(namelist[i]->d_name) + 1;
			list[i] = malloc(listlen);
			strncpy(list[i], namelist[i]->d_name, listlen);
		}
		free(namelist[i]);
	}
	free(namelist);
	
	//TODO.. make elements + 1 NULL
	list[i]=NULL;
	
	*listp=list;

	return numlists;
}

int hdb_scan_sublist_close(char **list){
	int i;

	assert(list);

	for(i=0;list[i];i++){
		free(list[i]);
	}	
	
	free(list);
	
	return 0;
}

/********************************************************** 
db_get_next - Get next sublist from dir pointer and store in list
 **********************************************************/
int dp_get_next(DIR *dp, char *list){
	struct dirent *dirp;

	if(!list || !dp){
		DBG("dp_get_next - invalid input\n");
		return HDB_INVALID_INPUT;
	}


	while((dirp = readdir(dp)) != NULL) {
		if(!strcmp(dirp->d_name, "..") || !strcmp(dirp->d_name, ".")) {
			continue;
		}
		if(dirp->d_type == DT_DIR){
			strncpy(list, dirp->d_name, HDB_LIST_MAX);
			return 0;
		}
	}

	DBG("db_get_next - failed to find more lists\n");
	strcpy(list,"");	
	return HDB_ERROR;
}

/********************************************************** 
  hdb_get_sublist_cur - get cursor to sublist.
  only support for DB_FIRST, DB_NEXT, DB_CLOSE
 **********************************************************/
int hdb_get_sublist_cur(const char *parent_list, int cursor, char *list){
	return hhdb_get_sublist_cur(&hdb_default, parent_list, cursor, list);
}

int hhdb_get_sublist_cur(HDB *hdbp, const char *parent_list, int cursor, char *list)
{
	char rootlist[HDB_PATH_MAX];
	struct dirlist *dl=NULL;
	struct linkedlist_t *head;
	int inode=0;

	DBG("hdb_get_sublist_cur - parent_list %s cursor %i\n", parent_list, cursor);
	if(!parent_list) {
		DBG("hdb_get_sublist_cur - parent_list is NULL returning\n");
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	set_rootlist(hdbp->root, parent_list, sizeof(rootlist), rootlist);

	if((inode = get_inode(rootlist)) == -1) {
		DBG("hdb_get_sublist_cur() - no such directory %s\n", rootlist);
		return HDB_LIST_NOT_FOUND;
	}

	head=(struct linkedlist_t*) hdbp->dirlist_head;

	//DB_FIRST -> close cache
	if(cursor == HDB_FIRST) {
		close_dirlist_cache(head, inode);
	}

	// this is freed in hdb_close
	if(hdbp->dirlist_head==NULL){
		head=create_dirlist_head(head);
		hdbp->dirlist_head = (void*)head;
	}

	assert(head && hdbp->dirlist_head);	

	//try to fetch directory pointer from cache
	if((dl = add_dirlist_cache(head, rootlist, inode)) == NULL) {
		DBG("hdb_get_sublist_cur - add_dirlist_cache failed root=%s parent_list%s\n", hdb_default.root, parent_list);
		return HDB_ERROR;
	}

	assert(dl->dp!=NULL);
	if(!dp_get_next(dl->dp, list)){
		return 0;
	}	 
	//no more elements in this directory.. remove it from the cache
	DBG("hdb_get_sublist_cur - no more elements in this directory.. returning 1\n");
	close_dirlist_cache(head, inode);
	return 1;
}

/********************************************************** 
  hdb_get_rec - recno 1 is first key
  Slow linear search with hdb185.
 **********************************************************/
int hdb_get_rec(const char *list, int recno, char *key, char *val){
	return hhdb_get_rec(&hdb_default, list, recno, key, val);
}

int hhdb_get_rec(HDB *hdbp, const char *list, int recno, char *key, char *val)
{
	DBT k, data;
	struct hive_t hive;
#if defined(DB3_DB4)
	DBC *dbcp;
#else
	static int r = 0;
#endif
	DB *dbp;
	HDBT *hdbt;
	int ret = 0;

	strcpy(key, "");
	strcpy(val, "");

	DBG("hdb_get_rec - list=%s recno=%i\n", list, recno);
	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	//Check for input errors
	if(recno < 1) {
		DBG("hdb_get_rec - ERR invalid recno %i\n", recno);
		return HDB_INVALID_INPUT;
	}
	if(!key || !val) {
		DBG("hdb_get_rec - ERR key or val is NULL\n");
		return HDB_INVALID_INPUT;
	}

	READ_LOCK
	if((hdbt = get_hdbt(hdbp->root, list)) == NULL) {
		DBG("hdb_get_rec - ERR unable to get dbp\n");
		UN_LOCK
		return HDB_DBP_ERROR;
	}
	assert(hdbt->dbp);
	dbp = hdbt->dbp;

#ifdef DB3_DB4
	//this cursor is only valid for this call
	//so no need to save it in hdbt
	if((dbcp = create_dbcp(hdbt->dbp)) == NULL){
		UN_LOCK
		return HDB_CURSOR_ERROR;
	}	
	assert(dbcp != hdbt->dbcp);
#endif

	memset(&k, 0, sizeof(k));
	memset(&data, 0, sizeof(data));
	k.data = &recno;

#if defined(DB3_DB4)
	ret = dbcp->c_get(dbcp, &k, &data, DB_SET_RECNO);
#else
	ret = (*dbp->seq) (dbp, &k, &data, R_FIRST);
	r = HDB_KEY_NOT_FOUND;
	while(ret == 0 && r != recno) {
		ret = (*dbp->seq) (dbp, &k, &data, R_NEXT);
		r++;
	}
	if(r==recno){
		ret = 0;
	}
#endif
	if(ret == 0) {
		if(data.size > 0) {
			unpack_hive(data.data, &hive);
			strncpy(val, hive.value, hive.size);
			val[hive.size] = '\0';	//avoid trailing trash
		}

		if(k.size > 0) {
			strncpy(key, (char *) k.data, data.size);
			key[k.size] = '\0';	//avoid trailing trash
		}
		DBG("hdb_get_rec - Found data for key\n");
		ret = 0;
	}
	else {
		DBG("hdb_get_rec - Unable to find key\n");
		strcpy(val, "");
		strcpy(key, "");
		ret = HDB_KEY_NOT_FOUND;
	}

#ifdef DB3_DB4
	if(ret != DB_NOTFOUND) {;
	}
	/* Close the cursor. */
	if((dbcp->c_close(dbcp)) != 0) {
		assert(0);
		UN_LOCK
		return HDB_BDB_ERROR;
	}
	dbcp=NULL;
#endif

	UN_LOCK
	return ret;
}

/********************************************************** 
  hdb_get_cur
 **********************************************************/

int hdb_get_cur(const char *list, int cursor, char *key, char *val){
	return hhdb_get_cur(&hdb_default, list, cursor, key, val);
}

int hhdb_get_cur(HDB *hdbp, const char *list, int cursor, char *key, char *val)
{
	DBT k, data;
	struct hive_t hive;
	char *p = NULL;
	int type = HDB_TYPE_STR;
	DB *dbp;
#ifdef DB3_DB4
	DBC *dbcp;
#endif
	HDBT *hdbt;
	int ret = 0;
	int dbcursor = _hdbcur_to_dbcur(cursor);

	strcpy(key, "");
	strcpy(val, "");

	DBG("hdb_get_cur() list=%s cursor=%i\n", list, cursor);

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	//an empty list is not valid
	if(list && !strcmp(list, "")) {
		//DBG("hdb_get_cur() HDB_INVALID_INPUT\n");
		//return HDB_INVALID_INPUT;
		hdbt = get_hdbt(hdbp->root, ".");
	}
	else {
		hdbt = get_hdbt(hdbp->root, list);
	}

	if(hdbt == NULL) {
		DBG("hdb_get_cur() failed to get_hdbt\n");
		//we should have an error from get_hdbt
		assert(dberrno);

		//did we get an error from get_hdbt return that
		return dberrno ? dberrno : HDB_DBP_ERROR;
	}
	assert(hdbt->dbp);
	dbp = hdbt->dbp;

#ifdef DB3_DB4
	if((dbcp = get_dbcp(hdbt)) == NULL){
		DBG("hdb_get_cur() cursor error\n");
		return HDB_CURSOR_ERROR;
	}	
	assert(dbcp == hdbt->dbcp);
#endif

	memset(&k, 0, sizeof(k));
	memset(&data, 0, sizeof(data));

#ifdef HDBD
	data.flags = DB_DBT_MALLOC; 
#endif

#ifdef DB3_DB4 
	ret = dbcp->c_get(dbcp, &k, &data, dbcursor);
#else
	ret = (*dbp->seq) (dbp, &k, &data, dbcursor);
#endif
	if(ret==0){
		if(data.size > 0) {
			unpack_hive(data.data, &hive);
			type = hive.meta[META_RAW];
			if(type == HDB_TYPE_STR || type == HDB_TYPE_INT) {
				strncpy(val, hive.value, hive.size);
				val[hive.size] = '\0';
			}
			else if(type == HDB_TYPE_LINK) {
				if((p = get_link(hive.value))) {
					strncpy(val, p, HDB_VALUE_MAX);
				}
			}
			else if(type == HDB_TYPE_EXEC) {
				if((p = get_exec(hive.value))) {
					strncpy(val, p, HDB_VALUE_MAX);
				}
			}
			else if(type == HDB_TYPE_FILE) {
				if((p = get_file(hive.value))) {
					strncpy(val, p, HDB_VALUE_MAX);
				}
			}
		}

		if(k.size > 0) {
			strncpy(key, (char *) k.data, HDB_KEY_MAX);
			key[k.size] = '\0';	//avoid trailing trash
		}
		DBG("%s", "Found data for key\n");
		ret = 0;
	}
	else {
		DBG("%s", "Unable to find key\n");
		strcpy(val, "");
		strcpy(key, "");
		ret = HDB_KEY_NOT_FOUND;
	}

#if defined(DB3_DB4)
	if(ret != DB_NOTFOUND) {
		;
	}
	/*
	   assert(dbcp != hdbt->dbcp);
	   dbcp->c_close(dbcp);
	   dbcp=NULL;
	 */
#endif
	return ret;
}

/********************************************************** 
  hdb_key_exist - returns 0 if key exist
 **********************************************************/

int hdb_key_exist(const char *list, const char *key){
	return hhdb_key_exist(&hdb_default, list, key);
}

int hhdb_key_exist(HDB *hdbp, const char *list, const char *key){
	char buf[1]; /* no need fo big buffer */
	return hhdb_get_nval(hdbp, list, key, sizeof(buf), buf);
}

/********************************************************** 
  hdb_exist - returns 0 if list exist
 **********************************************************/
int hdb_exist(const char *list)
{
	return hhdb_exist(&hdb_default, list);
}

int hhdb_exist(HDB *hdbp, const char *list){
	char rootlist[HDB_PATH_MAX];
	struct stat buf;

	if(!hdbp || !list){
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}
	
	//TODO.. should this look for the actual .db file instead?
	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);

	if((lstat(rootlist, &buf) == -1)) {
		DBG("hhdb_exist - list %s does NOT exist\n", rootlist);
		return 1;
	}

	DBG("hhdb_exist - list %s exist\n", rootlist);
	return 0;
}

/********************************************************** 
  hdb_get_size - return no of datbases in list (== nr of subdirectories)
  returns 0 on error
 **********************************************************/
int hdb_get_size(const char *list){
	return hhdb_get_size(&hdb_default, list);
}

int hhdb_get_size(HDB *hdbp, const char *list){

	struct stat buf;
	char rootlist[HDB_PATH_MAX];

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		DBG("hdb_get_size - set_hdb_root failed\n");
		//error.. return 0
		return 0;
	}

	if(!list){
		hdbp->dberrno = dberrno = HDB_INVALID_INPUT;
		return 0;
	}

	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);

	//TODO.. does not work on BSD Unix
	if(!stat(rootlist, &buf) && buf.st_mode & S_IFDIR) {
		return buf.st_nlink - 2;
	}

	//even though error.. return 0
	return 0;
}

/********************************************************** 
  hdb_create_list - return 0 on success or if the list already exist
 **********************************************************/
int hdb_create_list(const char *list){
	return hhdb_create_list(&hdb_default, list);
}

int hhdb_create_list(HDB *hdbp, const char *list){
	int ret=0;
	WRITE_LOCK
	ret=_hhdb_create_list(hdbp, list);
	UN_LOCK
	return ret;
}
int _hhdb_create_list(HDB *hdbp, const char *list){
	char *bname, *basec, *dirc, *dname;
	char parentdir[HDB_PATH_MAX];
	char dbname[HDB_PATH_MAX];
	char rootlist[HDB_PATH_MAX];
	mode_t mode;
	struct stat buf;
	int ret = 0;

	DBG("hdb_create_list %s\n", list);

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		DBG("_hhdb_create_list - root error\n");
		return HDB_ROOT_ERROR;
	}

	//assert(strcmp(list, "."));

	if(!list) {
		return HDB_INVALID_INPUT;
	}

	//we dont like this
	if(*list == ' ' || *list == '.' || *list == '~' || *list == 0) {
		return HDB_INVALID_INPUT;
	}

	//TODO.. add same cheks as in hdb_set_root (strstr(root,"hdb")) etc
	//paranoid check
	if(strstr(list, "..") || strstr(list, ";")) {
		return HDB_INVALID_INPUT;
	}

	/*
	if(root_is_ok && hdb_set_root(getenv("HDBROOT"))) {
		DBG("hdb_create_list - root error\n");
		return HDB_ROOT_ERROR;
	}
	*/

	//List exist.. no need to create it then.
	if(!hhdb_exist(hdbp, list)) {
		return 0;
	}
	//Absolut path test.. we require a "hdb" in the dir
	//We could be recursing though. Have this test after hdb_exist test
	if(*list=='/' && !strstr(list, "hdb")) {
		return HDB_INVALID_INPUT;
	}


	/*
	   From the user specified list (list/sublist) we want:
	   rootlist=hdbroot/list/sublist
	   dbname=hdbroot/list/sublist.db
	   parentdir=hdbroot/list

	   since this info is used alot. we save to globals.
	 */

	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);

	basec = strdup(list);
	dirc = strdup(list);
	bname = basename(basec);
	dname = dirname(dirc);

	if(!strcmp(dname, ".") && 0){
		snprintf(parentdir, HDB_PATH_MAX, "%s", hdbp->root);
		snprintf(dbname, HDB_PATH_MAX, "%s/%s.db", hdbp->root, basename(hdbp->root));
	}
	else {
		snprintf(parentdir, HDB_PATH_MAX, "%s%s", hdbp->root, dname);
		snprintf(dbname, HDB_PATH_MAX, "%s/%s.db", rootlist, bname);
	}

	//recursivly create the hdb structure if needed
	if(strcmp(dname, ".") || strcmp(dname, "/")) {
		if((lstat(parentdir, &buf) == -1)) {
			DBG("hdb_create_list - no parent dir %s found.. recursing\n", parentdir);
			if(_hhdb_create_list(hdbp, dname) != 0) {
				if(dirc) {
					free(dirc);
				}
				if(basec) {
					free(basec);
				}
				DBG("hdb_create_list - recursive create failed\n");
				return HDB_ERROR;
			}
		}
	}

	if((lstat(rootlist, &buf) == -1)) {
		mode = umask(0);
		DBG("hdb_create_list - creating rootlist %s\n", rootlist);
		if((mkdir(rootlist, 0777) == -1)) {
			umask(mode);
			DBG("hdb_create_list - mkdir failed %s dname=%s parentdir=%s rootlist=%s\n", 
			strerror(errno), dname, parentdir, rootlist);
			free(dirc);
			free(basec);
			perror("hdb_create_list");
			assert(0);
			return HDB_ERROR;
		}
		umask(mode);
	}
	else {
		DBG("hdb_create_list - lstat failed\n");
		assert(0);
		ret = HDB_ERROR;
	}


	ret = open_rootlist_db(rootlist);


	free(basec);
	free(dirc);

	if(use_trigger) {
		trigger(HDB_TRIG_CREATE, list, NULL, NULL, NULL);
	}

	return ret;
}

/********************************************************** 
  hdb_delete_list unlink list and rm -rf on sublists
 **********************************************************/
int 
hdb_delete_list(const char *list){
	return hhdb_delete_list(&hdb_default, list);
}

int
hhdb_delete_list(HDB *hdbp, const char *list)
{
	char rootlist[HDB_PATH_MAX];
	char db[HDB_PATH_MAX];

	//no nulls or empty lists
	if(!list || !strcmp(list, "")) {
		return HDB_INVALID_INPUT;
	}

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		DBG("hdb_delete_list - root error\n");
		return HDB_ROOT_ERROR;
	}

	WRITE_LOCK
	close_rootlist(hdbp->root, list);

	set_rootlist(hdbp->root, list, sizeof(rootlist), rootlist);

	//if list does not exist.. return OK
	if(hdb_exist(rootlist)) {
		UN_LOCK
		return 0;
	}

	//TODO.. remove this to add support for absolute path
	//TODO.. add some sequrity checks
	if(*list == '/'){
		UN_LOCK
		return HDB_SECURITY_ERROR;
	}

	//we have problems if we remove a whole tree structure
	//this should be change to a hdb_sync_tree(char *list);
	//TODO.. add hdb_sync_tree
	_sync_restart();

	sprintf(db, "%s/%s.db", rootlist, list);
	DBG("hdb_delete_list - unlinking %s\n", db);
	(void) unlink(db);

	sprintf(db, "rm -rf %s", rootlist);
	DBG("hdb_delete_list - running cmd %s\n", db);
	if(system(db)) {
		//TODO.. could be that we faild because sublist was gone
		//which is OK.
		UN_LOCK
		return HDB_SHELL_ERROR;
	}
	UN_LOCK
	if(use_trigger) {
		trigger(HDB_TRIG_DELETE, list, NULL, NULL, NULL);
	}

	return 0;
}

/********************************************************** 
  hdb_trig_disable
 **********************************************************/
int hdb_trig_disable(int type, const char *list, const char *key)
{
	use_trigger = 0;
	return 0;
}

/********************************************************** 
  hdb_trig_enable
 **********************************************************/
int hdb_trig_enable(int type, const char *list, const char *key)
{
	use_trigger = 1;
	return 0;
}

/**************************************************************
  hdb_syscall
  op = HDB_GET_VAL, HDB_GET_VAL
  params = number of input parameters
  input[0] = "list"
  input[1] = "key" etc
 **************************************************************/
//int hdb_syscall(int op, int params, char **input, char **result){
int hdb_syscall(int op, struct HDBOP *hop, char **result){

	int ret=1;

	switch(op){
	case HDB_GET_RAW:
		break;
	case HDB_SET_RAW:
		break;
	default:
		break;
	}
	return ret;
}

/**************************************************************
example:
hdb_cmd("GET list key value", result);
 **************************************************************/
int hdb_cmd(char *cmd, char **result){
	return 0;
}

/**************************************************************
  network hdb functions 
 **************************************************************/
int hdb_connect(const char *host, int port, char *user, char *pwd,  int crypt){
	assert(0);	
	return HDB_NOT_IMPLEMENTED;
}

int hdb_disconnect(){
	assert(0);
	return HDB_NOT_IMPLEMENTED;
}

char *hdb_net_get(const char *cmd){
	assert(0);
	dberrno=HDB_NOT_IMPLEMENTED;
	return NULL;
}

int hdb_net_set(const char *cmd){
	assert(0);
	return HDB_NOT_IMPLEMENTED;
}

int hdb_net_print(FILE *fd, const char *cmd){
	assert(0);
	return HDB_NOT_IMPLEMENTED;
}

int hdb_net_print_fmt(FILE *fd, const char *cmd, int bash_friendly){
	assert(0);
	return HDB_NOT_IMPLEMENTED;
}

/************************************************************
  trigger -
 ************************************************************/
int trigger(int type, const char *list, const char *key, const char *oldvalue,
		const char *newvalue)
{
	//TODO.. add support for syslogging. 
	//TODO.. add support for scripted triggers. 
	return 0;
}


/*********************************************************************
  initialize_hdbt - Initialize our database pointer caches
  Only need to initialize the hash so far.
 *********************************************************************/

int initialize_hdbt()
{
#ifdef DB3_DB4
	int ret = 0;
#endif
	DB *dbp=NULL;

	DBG("initialize_hdbt - hdbt_hash = %p\n", hdbt_hash);

	assert(hdbt_hash == NULL);

	if(hdb_cache_type == HDB_CACHE_HASH) {
		hdbt_hash = malloc(sizeof(HDBT));
		hdbt_hash->dbp=NULL;
#ifdef DB3_DB4
		hdbt_hash->dbcp=NULL;
		if((ret = db_create(&dbp, NULL, 0)) != 0) {
			DBG("initialize_hdbt - db_create error %s\n", db_strerror(ret));
			free(hdbt_hash);
			hdbt_hash = NULL;
			assert(0);
			return HDB_ERROR;
		}
#endif

#ifdef DB3
		if((ret = dbp->open(dbp, NULL, NULL, DB_HASH, DB_CREATE, 0666)) != 0) {
			DBG("initialize_hdbt - creating hash failed\n");
			free(hdbt_hash);
			hdbt_hash = NULL;
			assert(0);
			return HDB_ERROR;
		}

#elif defined(DB4)
		if((ret = dbp->open(dbp, NULL, NULL, NULL, DB_HASH, DB_CREATE, 0666)) != 0) {
			DBG("initialize_hdbt - creating hash failed\n");
			free(hdbt_hash);
			hdbt_hash = NULL;
			assert(0);
			return HDB_ERROR;
		}
#else
		if((dbp = dbopen(NULL, O_CREAT | O_RDWR, 0666, DB_HASH, NULL)) == NULL) {
			DBG("initialize_hdbt - creating hash failed\n");
			free(hdbt_hash);
			hdbt_hash = NULL;
			assert(0);
		}
#endif
	}
	else {
		return HDB_ERROR;
	}
	hdbt_hash->dbp=(void*)dbp;

	hdbt_hash_size = 0;

	return 0;
}

/*********************************************************************
  add_hdbt_to_list - add database pointer to 1 list cache 
 *********************************************************************/
int
add_hdbt_to_list(int id, HDBT *hdbt)
{
	static int list_only = 0;

	DBG("add_hdbt_to_list - %i %p\n", id, hdbt);

	if(!hdbt || id==0) {
		assert(0);
		return HDB_INVALID_INPUT;
	}
	//we only close when
	//1. we have a valied hdbt_last (otherwise -> sigsegv)
	//2. hdbt_hash_is_full (otherwise -> sigsegv when hash closes dbp)
	//3. the list is the only cache mecanism (otherwise -> sigsegv when hash closes dbp)
	if(hdbt_last && hdbt_hash_is_full && list_only) {
		DBG("add_hdbt_to_list - closing hdbt_last %p\n", hdbt_last);
		if(close_hdbt(hdbt_last)){
			DBG("add_hdbt_to_list - failed to close hdbt_last %p\n", hdbt_last);
		}	
		hdbt_last=NULL;
	}
	hdbt_last_id = id; 
	hdbt_last = hdbt;

	if(hdbt_hash_is_full) {
		list_only = 1;
	}
	else {
		list_only = 0;
	}
	DBG("add_hdbt_to_list - hdbt_last is now %p\n", hdbt_last);
	return 0;
}


/******************************************************************
  get_hdbt_from_list - return 1 list cache database pointer 
 ******************************************************************/
HDBT *
get_hdbt_from_list(int id)
{
	if(hdbt_last && hdbt_last_id == id){
		DBG("get_hdbt_from_list - OK hdbt_last = %p\n", hdbt_last);
		return hdbt_last;
	}
	DBG("get_hdbt_from_list - FAIL hdbt_last is %p\n", hdbt_last);
	return NULL;
}

/****************************************************************
  del_hdbt_from_hash - hashkey is inode of the list. 
 ****************************************************************/
int del_hdbt_from_hash(int hashkey)
{
	DBT key;
	DB *dbp;
	char hashbuf[32];

	if(!hdbt_hash || !hashkey) {
		return HDB_ERROR;
	}
	
	dbp = (DB *)hdbt_hash->dbp;

	memset(&key, 0, sizeof(key));

	snprintf(hashbuf, sizeof(hashbuf), "%i", hashkey);
	key.data = (void *) hashbuf;
	key.size = strlen(hashbuf);

#ifdef DB3_DB4
	if(dbp->del(dbp, NULL, &key, 0)) {
		DBG("del_hdbt_from_hash - failed to remove inode %s\n",
				hashbuf);
		return HDB_ERROR;
	}
#else
	if(dbp->del(dbp, &key, 0)) {
		DBG("del_hdbt_from_hash - failed to remove inode %s\n",
				hashbuf);
		return HDB_ERROR;
	}
#endif
	hdbt_hash_size--;
	return 0;

}
/**************************************************************
  get_hdbt_from_hash - 
 **************************************************************/
HDBT *
get_hdbt_from_hash(int hashkey)
{
	int ret = 0;
	struct hash_hive *h;

	char hashbuf[32];

	DBT key, data;

	if(!hdbt_hash) {
		initialize_hdbt();
		if(!hdbt_hash) {
			return NULL;
		}
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	snprintf(hashbuf, sizeof(hashbuf), "%i", hashkey);
	key.data = (void *) hashbuf;
	key.size = strlen(hashbuf);

/*
#ifdef DB3_DB4
	ret = hdbt_hash->dbp->get(hdbt_hash->dbp, NULL, &key, &data, 0);
#else
	ret = hdbt_hash->dbp->get(hdbt_hash->dbp, &key, &data, 0);
#endif
*/
	ret = dbp_get(hdbt_hash->dbp, &key, &data);
	if(ret==0){
		if(data.size > 0 && data.data) {
			h = data.data;
			if(h->hdbt) {
				DBG("get_hdbt_from_hash - found hdbt %p dbp %p hashkey %i\n", 
					h->hdbt, h->hdbt->dbp, hashkey);
				assert(h->hdbt->dbp);
				return h->hdbt;
			}
		}
		else {
			DBG("get_hdbt_from_hash -  ERROR key %i dbp is NULL\n", hashkey);
		}
	}
	DBG("get_hdbt_from_hash - could not find %i\n", hashkey);
	return NULL;
}

/************************************************************
  get_hdbt - fetch the database pointer and add it to caches.
  1. Look in caches.
  2. Can't find in cache look on disk. 
  3. Found on disk then add to cache.
 ************************************************************/
HDBT *
get_hdbt(const char *root, const char *list)
{
	DB *db = NULL;
	HDBT *dbp= NULL;
	char rootlist[HDB_PATH_MAX];
	ino_t hashkey;

	DBG("get_hdbt - root=%s list=%s\n", root, list);

	if(!list) {
		dberrno = HDB_INVALID_INPUT;
		return NULL;
	}

	set_rootlist(root, list, sizeof(rootlist), rootlist);

	if((hashkey = get_inode(rootlist)) == -1) {
		DBG("get_hdbt() - no such directory %s\n", rootlist);
		dberrno = HDB_LIST_NOT_FOUND;
		return NULL;
	}

	//cache the last element
	if((dbp = get_hdbt_from_list(hashkey)) != NULL) {
		return dbp;
	}

	//look for the dbp in our hash
	if(hdb_cache_type == HDB_CACHE_HASH) {
		if((dbp = get_hdbt_from_hash(hashkey)) != NULL) {
			return dbp;
		}
	}


	//try to open the database and return a new dbp 
	if((db = open_db2(rootlist, 1)) == NULL) {
		DBG("get_hdbt - open_db2 failed\n");
		dberrno = HDB_OPEN_ERROR;
		return NULL;
	}

	//TODO.. make sure this is freed
	dbp = malloc(sizeof(HDBT));	
	dbp->dbp = db;
#ifdef DB3_DB4
	dbp->dbcp = NULL;
#endif

	if(add_hdbt_to_cache(rootlist, dbp, hashkey)) {
		DBG("get_hdbt - failed to dbp to any of the available cache\n");
	}

	return dbp;
}
/**
* clear all lists and free nodes
**/
int
clear_dirlist_cache(struct linkedlist_t *dl)
{

	struct dirlist *p = NULL;
	struct dirlist *n = NULL;

	DBG("clear_dirlist_cache() \n");
	if(dl == NULL || dl->head == NULL) {
		DBG("close_dirlist_cache -  head is null.. FAIL\n");
		return HDB_INVALID_INPUT;	//closing what?
	}

	p = dl->head;
	while(p) {
		n = p->next;
		DBG("clear_dirlist_cache - closing key %i dp %p\n", p->key, (void*)p->dp);
		assert(p && p->dp);
		closedir(p->dp);
		p->dp = NULL;
		free(p);
		p = n;
	}
	return 0;
}

/**********************************************************
  close_dirlist_cache - 
 *********************************************************/
int
close_dirlist_cache(struct linkedlist_t *dl, int key)
{

	struct dirlist *p = NULL;
	struct dirlist *n = NULL;

	DBG("close_dirlist_cache - key =%i\n", key);
	if(dl == NULL || dl->head == NULL) {
		DBG("close_dirlist_cache -  head is null.. FAIL\n");
		return HDB_INVALID_INPUT;	//closing what?
	}

	p = dl->head;
	while(p) {
		n = p->next;
		if(p->key == key){
			DBG("close_dirlist_cache - found match.. closing %i\n", key);
			closedir(p->dp);
			p->dp = NULL;

			p->key = 0;
			if(p == dl->head) {
				//move head forward
				if(p->next){
					dl->head=p->next;
					//since we do backwards loop in add_dirlist_cache.. make sure head->prev is null
					dl->head->prev=NULL;
				}
				else {
					DBG("close_dirlist_cache - free head\n");
					assert(dl);
					assert(p);
					p->next = p->prev = NULL;
					dl->head = dl->tail = NULL;
				}
			}
			else if(p == dl->tail) {
				//last element.. no more head
				DBG("close_dirlist_cache - free tail\n");
				dl->tail = p->prev;
				p->next = p->prev = NULL;
				dl->tail->next = NULL;
			}
			else {
				p->prev->next = p->next;
				p->next->prev = p->prev;
			}
			free(p);
			p=NULL;
			return 0;
		}
		p = n;
	}
	DBG("close_dirlist_cache - no match found returning HDB_ERROR\n");
	return HDB_ERROR;
}

struct linkedlist_t *create_dirlist_head( struct linkedlist_t *head){
	head = malloc(sizeof(struct linkedlist_t));
	head->head = head->tail = NULL;
	DBG("create_dirlist_head size %i %p\n", sizeof(struct linkedlist_t), (void*) head);
	return head;
}

//TODO.. this is not used
struct dirlist * create_dirlist_cache( struct dirlist *dl){

	if(dl){
		return NULL;
	}
	dl = malloc(sizeof(struct dirlist));
	dl->key=0;
	dl->head=NULL;
	dl->prev=NULL;
	dl->next=NULL;
	dl->tail=NULL;
	return dl;
}

/*********************************************************
  add_dirlist_cache
 *********************************************************/
struct dirlist *
add_dirlist_cache(struct linkedlist_t *dl, const char *rootlist, int key)
{

	struct dirlist *p = NULL;
	//int i = 0;

	DBG("add_dirlist_cache - dl %p key %i find dirlist for %s\n", (void*)dl, key, rootlist);

	if(dl==NULL){
		return NULL;
	}

	//1) - Lock in cache and return.
	//     for speed up check back first  
	for(p = dl->tail; p; p = p->prev) {
		DBG("add_dirlist_cache - comparing %i %i head=%p tail=%p this=%p next=%p prev=%p\n", p->key, key, dl->head, dl->tail, p, p->next, p->prev) ;
		//i++; //TODO.. remove testing only
		if(p->key == key){
			DBG("add_dirlist_cache - found it\n");
			return p;
		}
		//assert(i<33);
	}

	//2) Could not find directory in linked list
	//   .. create new node
	if((p = malloc(sizeof(struct dirlist))) == NULL) {
		DBG("add_dirlist_cache - malloc failed\n");
		dberrno = HDB_MALLOC_ERROR;
		return NULL;
	}

	if((p->dp = opendir(rootlist)) == NULL) {
		DBG("add_dirlist_cache - cant open %s return NULL\n", rootlist);
		dberrno = HDB_OPEN_ERROR;
		free(p);
		return NULL;
	}
	p->key = key;

	//3) add the node to the end
	p->next = NULL;


	//4) Make the ll pointer point to this as head
	if(dl->head == NULL) {
		DBG("add_dirlist_cache - creating head\n");
		dl->tail = dl->head = p->head = p->tail = p;
		//we have no prev in the head
		p->prev = NULL;
	}
	else {
		//5) Just append the node
		DBG("add_dirlist_cache - creating tail\n");
		p->prev = dl->tail;
		p->head = dl->head;
		p->tail = p;
		//6) Update the previous node ->next to this tail
		p->prev->next = p;
		//7) Update the Linked List tail info
		dl->tail = p;
	}

	assert(dl->head->prev == NULL);

	//8) Return the newly created dirlist
	DBG("add_dirlist_cache - list %s key %i added to cache\n", rootlist, key);
	return p;
}


/*********************************************************
  add_hdbt_to_cache - Add our dbp to one or more caches 
 **********************************************************/
int add_hdbt_to_cache(const char *rootlist, HDBT * dbp, int hashkey)
{

	hdbt_hash_is_full = 0;

	if(add_hdbt_to_hash(rootlist, dbp, hashkey)) {
		DBG("add_hdbt_to_cache - failed to add to hash table\n");
		//or.. hash is defunct
		hdbt_hash_is_full = 1;
	}

	//add our dbp to the linear_list_cache
	if(add_hdbt_to_list(hashkey, dbp)) {
		DBG("add_hdbt_to_cache failed to add to linear dbp list\n");
	}

	//TODO.. add error return code
	return 0;
}

/********************************************************
  add_dbp
 ********************************************************/
int add_dbp(const char *rootlist, DB * dbp)
{
	HDBT *hdbt=NULL;
	int hashkey;

	if(!rootlist) {
		return HDB_INVALID_INPUT;
	}

	if((hashkey = get_inode(rootlist)) == -1) {
		return HDB_LIST_NOT_FOUND;
	}

	hdbt = malloc(sizeof(HDBT));
	hdbt->dbp = dbp;
#ifdef DB3_DB4
	hdbt->dbcp = NULL;
#endif
	DBG("add_dbp - rootlist=%s\n", rootlist);
	return add_hdbt_to_cache(rootlist, hdbt, hashkey);
}

/***********************************************************
  add_hdbt_to_hash
************************************************************/
int add_hdbt_to_hash(const char *rootlist, HDBT * hdbt, int hashkey)
{

	DBT key, data;
	DB *dbp=NULL;
	int ret = 0;
	char hashbuf[32];
	struct hash_hive h;

	DBG("add_hdbt_to_hash -  rootlist=%s HDBT=%p\n", rootlist, hdbt);

	if(hdb_cache_type != HDB_CACHE_HASH) {
		DBG("add_hdbt_to_hash - hdb_cache_type does not use hash.. return\n");
		return HDB_ERROR;
	}
	if(hdbt_hash_size >= hdbt_hash_max_size) {
		DBG("add_hdbt_to_hash - hash is full hash_size=%i max_size=%i\n", hdbt_hash_size, hdbt_hash_max_size);
		return HDB_ERROR;
	}

	if(!hdbt_hash) {
		initialize_hdbt ();
		if(!hdbt_hash) {
			assert(0);
			return HDB_ERROR;
		}
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	h.i = 999;		//only used for testing so far

	h.hdbt = hdbt;
	snprintf(hashbuf, sizeof(hashbuf), "%i", hashkey);
	key.data = (void *) hashbuf;
	key.size = strlen(hashbuf);

	data.data = (void *) &h;
	data.size = sizeof(struct hash_hive);

	dbp = (DB *)hdbt_hash->dbp;

	DBG("add_hdbt_to_hash - put data.data=%p hdbt=%p\n", data.data, hdbt);
#ifdef DB3_DB4
	ret = dbp->put(dbp, NULL, &key, &data, DB_NOOVERWRITE);
#else
	ret = dbp->put(dbp, &key, &data, R_NOOVERWRITE);
#endif
	if(ret){
		DBG("add_hdbt_to_hash - failed retval = %i errno %i\n", ret, errno);
		if(ret == 1) {
			DBG("add_hdbt_to_hash - element does already exist.. should not happen\n");
			assert(0);
		}
		else {
			DBG("add_hdbt_to_hash - error adding to hash dbp=%p rootlist=%s errno=%i ret=%i data=%p data.size=%i\n", hdbt_hash, rootlist, errno, ret, h.hdbt, data.size);
#ifdef DB3_DB4
			DBG("add_hdbt_to_hash - %s\n", db_strerror(ret));
#endif
			assert(0);
		}
		return HDB_ERROR;
	}

	hdbt_hash_size++;

	return 0;
}

/******************************************************************
  helper routing to set global variable dbname. 
 ******************************************************************/
//remove this... not thread safe
int set_dbname(HDB *hdbp, const char *list, int size, char *dbname)
{
	char *bname = NULL;
	char *basec = NULL;

	if(!list) {
		return HDB_INVALID_INPUT;
	}

	assert(hdbp);

	basec = strdup(list);
	bname = basename(basec);
	snprintf(dbname, size, "%s%s/%s.db", hdbp->root, list, bname);
	free(basec);

	return 0;
}


/*****************************************************
  close the databasepointer matching root and list.
 ******************************************************/
int close_rootlist(const char *root, const char *list)
{
	HDBT *hdbt;
	int hashkey;

	DBG("close_rootlist - root %s list %s\n", root, list);

	//TODO.. this will open it if it's already closed.
	if((hdbt = get_hdbt(root, list)) == NULL) {
		return HDB_ERROR;
	}

	assert(hdbt->dbp);

	//hdbt_last is also closed then.
	if(hdbt == hdbt_last) {
		hdbt_last = NULL;
		hdbt_last_id = 0;
	}

	if(close_hdbt(hdbt)){
		DBG("close_rootlist - failed to close hdbt %p\n", hdbt);
		assert(0);
		return HDB_ERROR;
	}
	hdbt=NULL;

	if(hdb_cache_type == HDB_CACHE_HASH) {
		if((hashkey = get_hash_key(root, list)) == -1) {
			return HDB_ERROR;
		}
		if(del_hdbt_from_hash(hashkey)) {
			return HDB_ERROR;
		}
	}
	return 0;
}

//old root must have format '/dir/'
//newroot can be in format '/dir' or '/dir/'
//TODO.. use inode to compare instead
static inline int root_cmp(char *oldroot, const char *newroot)
{
	while(1) {
		if(*oldroot != *newroot) {
			if(!*newroot && !*++oldroot) {
				return 0;
			}
			return -1;
		}
		if(!*oldroot) {
			return 0;
		}
		oldroot++;
		newroot++;
	}
	return 0;
}
/********************************************
open_rootlist_db - Open berkeley database and add it to caches.
*******************************************/
int 
open_rootlist_db(const char *rootlist){

	DB *dbp = NULL;

	if((dbp = open_db2(rootlist, 1)) == NULL) {
		return HDB_ERROR;
	}
	else {
		add_dbp(rootlist, dbp);
	}
	
	return 0;
}

/******************************************************
create_dbcp
******************************************************/
#ifdef DB3_DB4
DBC * 
create_dbcp(DB *dbp){
	int ret = 0;
	DBC *dbcp=NULL;
	assert(dbp);

	if((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		DBG("create_dbcp - error %s\n", db_strerror(ret));
		assert(0);
		return NULL;
	}
	return dbcp;
}

DBC * 
get_dbcp(HDBT *hdbt){

	DBC *dbcp=NULL;
	DB *dbp=NULL;
	int ret;

	assert(hdbt);
	assert(hdbt->dbp);

	if(hdbt->dbcp){
		return hdbt->dbcp;
	}

	dbp = (DB*)hdbt->dbp;

	DBG("get_dbcp - hdbt %p dbp %p dbcp %p\n", hdbt, hdbt->dbp, hdbt->dbcp);
	if((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		DBG("get_dbcp - cursor error %s\n", db_strerror(ret));
		assert(0);
		return NULL;
	}
	hdbt->dbcp = dbcp;
	return dbcp;
}


int close_dbc(DBC *dbcp){
	assert(dbcp);

	int ret=0;
	if((ret=dbcp->c_close(dbcp))){
		DBG("close_dbc - %s\n", db_strerror(ret));
		assert(0);
		ret=HDB_BDB_ERROR;
	}
	return ret;
}

#endif

//DB185, Sleepycat wrappers
int dbp_sync(DB *dbp){
	assert(dbp);
	return dbp->sync(dbp,0);
}

int dbp_del(DB *dbp, DBT *key){
	assert(dbp);
#if defined (DB3_DB4)
	return dbp->del(dbp, NULL, key, 0);
#else
	return dbp->del(dbp, key, 0);
#endif
}

int dbp_put(DB *dbp, DBT *key, DBT *data){
	assert(dbp);
#if defined (DB3_DB4)
	return dbp->put(dbp, NULL, key, data, 0);
#else
	return dbp->put(dbp, key, data, 0);
#endif
}

int dbp_get(DB *dbp, DBT *key, DBT *data){
	assert(dbp);
#if defined (DB3_DB4)
	return dbp->get(dbp, NULL, key, data, 0);
#else
	return dbp->get(dbp, key, data, 0);
#endif
} 

DB *dbp_open(const char *rootlist, int create){
	char *bname, *basec;
	basec = strdup(rootlist);
	bname = basename(basec);
	char dbname[HDB_PATH_MAX];
	DB *dbp = NULL;
	mode_t mode;
	int dbflag=0;
	//flags == DB_CREATE
#ifndef DB3_DB4
	BTREEINFO b;
#else
	int ret=0;
#endif

	int ret_val = 0;

	if(create){
#ifdef DB3_DB3
		dbflag = DB_CREATE;
#else
		dbflag = O_CREAT;
#endif
	}

#if defined (DB3_DB3) && defined (HDBD) && defined(WITH_BDB_LOCKING)
	dbflag |= DB_THREAD;
#endif

	DBG("dbp_open - Enter %s\n", rootlist);

	basec = strdup(rootlist);
	bname = basename(basec);

	memset(dbname, 0, sizeof(dbname));
	snprintf(dbname, sizeof(dbname), "%s/%s.db", rootlist, bname);

	mode=umask(0);
#ifdef DB3
	if((ret =
	    dbp->open(dbp, dbname, NULL, DB_BTREE, dbflag, 0666)) != 0) {
		umask(mode);
	}
#elif defined(DB4)
	if((ret =
	    dbp->open(dbp, NULL, dbname, NULL, DB_BTREE, dbflag, 0666)) != 0) {
		umask(mode);
	}
#else
	b.flags = 0;
	b.cachesize = 0;
	b.maxkeypage = 0;
	b.minkeypage = 0;
	b.psize = 0;
	b.compare = NULL;
	b.prefix = NULL;
	b.lorder = 0;
	mode = umask(0);
	if((dbp =
	    dbopen(dbname, dbflag |O_RDWR, 0666, DB_BTREE, &b)) == NULL) {
		DBG("dbp_open - unable to open %s\n", dbname);
		ret_val = 1;
	}
#endif
	umask(mode);
	free(basec);
	DBG("dbp_open - dbname %s dbp = %p\n", dbname, dbp);
	if(ret_val){
		return NULL;
	}
	return dbp;
}

int dbp_close(DB *dbp){
	int ret=0;
	assert(dbp);
#ifdef DB3_DB4
	DBG("dbp_close closing dbp %p\n", dbp);
	if((ret=dbp->close(dbp,0))){
		DBG("DBP_close - errno %i %s\n", ret, db_strerror(ret));
		assert(ret==2); //no such directory is ok.. externaly removed
		ret=HDB_BDB_ERROR;
	}
#else
	if((ret=dbp->close(dbp))){
		DBG("dbp_close - failed to close dbp %i %p\n", ret, dbp);
		ret=HDB_BDB_ERROR;
		assert(0);
	}
#endif
	DBG("dbp_close() - exiting ret = %i\n", ret);
	dbp=NULL;
	return ret;
}

/******************************************************
int close_hdbt
******************************************************/
int close_hdbt(HDBT *hdbt){
	
	int ret=0;	

	DBG("close_hdbt %p dbp %p dbpc %p\n", hdbt, hdbt->dbp, hdbt->dbcp);

	assert(hdbt);
	assert(hdbt->dbp);

#ifdef DB3_DB4
	//close cursor first
	//they are implicitly close later in when closing dbp
	//but this core-dumps -> catches bugs
	if(hdbt->dbcp){
		DBG("close_hdbt closing cursor %p\n", hdbt->dbcp);
		ret = close_dbc(hdbt->dbcp);
		//!!dbp->close frees the cursor
		hdbt->dbcp=NULL;
	}
	hdbt->dbcp=NULL;
#endif
/*
	DBG("close_hdbt closing dbp %p\n", hdbt->dbp);
	if((ret=hdbt->dbp->close(hdbt->dbp,0))){
		DBG("close_hdbt - errno %i %s\n", ret, db_strerror(ret));
		assert(ret==2); //no such directory is ok.. externaly removed
		ret=HDB_BDB_ERROR;
	}
#else
	if((ret=hdbt->dbp->close(hdbt->dbp))){
		DBG("close_hdbt - failed to close dbp %i %p\n", ret, hdbt->dbp);
		ret=HDB_BDB_ERROR;
		assert(0);
	}
#endif
*/
	
	ret = dbp_close(hdbt->dbp);
	hdbt->dbp=NULL;
	free(hdbt);
	return ret;
}
/******************************************************
open_db2 - open database and return database pointer
******************************************************/
DB *open_db2(const char *rootlist, int create)
{
#if defined(DB3_DB4)
	int dbret = 0;
#else
	BTREEINFO b;
#endif
	int ret_val = 0;
	int bdbopenflags=0;
	char *bname, *basec;
	char dbname[HDB_PATH_MAX];
	mode_t mode;
	DB *dbp = NULL;

	DBG("open_db2 - Enter %s\n", rootlist);

	basec = strdup(rootlist);
	bname = basename(basec);

	//TODO.. need this here?
	memset(dbname, 0, sizeof(dbname));

	//make /var/db/hdb/hdb.db instead of /var/db/hdb/..db
	snprintf(dbname, sizeof(dbname), "%s/%s.db", rootlist, bname);

	DBG("open_db2 - opening dbname %s\n", dbname);

#if WITH_BDB_ENVIROMENT
	//open our enviroment if its not opened
	check_env();
#endif

	if(create){
#ifdef DB3_DB4
		bdbopenflags=DB_CREATE;
#else
		bdbopenflags=O_CREAT;
#endif
	}
#if defined(HDBD) && defined(DB3_DB4) && defined(WITH_BDB_LOCKING)
	DBG("open_db2 with DB_THREAD\n");
	bdbopenflags|=DB_THREAD;
#endif

#ifdef DB3_DB4
	if((dbret = db_create(&dbp, hdb_dbenv, 0)) != 0) {
		DBG("open_db2 - db_create error %s\n", db_strerror(ret_val));
		free(basec);
		return NULL;
	}

	//we can store some private info here
	//dbp->app_private = 0x1;

	// fix me
	if((dbret = dbp->set_flags(dbp, DB_RECNUM)) != 0) {
		//dbp->err(dbp, ret, "set_flags: DB_RECNUM");
		free(basec);
		return NULL;
	}
#endif

	//valgrind complains about this????
	mode = umask(0);
#ifdef DB3
	if((dbret =
	    dbp->open(dbp, dbname, NULL, DB_BTREE, bdbopenflags, 0666)) != 0) {
		umask(mode);
		ret_val = dbret;
		dbp->err(dbp, dbret, "%s: open", dbname);
	}
#elif defined(DB4)
	if((dbret =
	    dbp->open(dbp, NULL, dbname, NULL, DB_BTREE, bdbopenflags, 0666)) != 0) {
		umask(mode);
		ret_val = dbret;
		dbp->err(dbp, dbret, "%s: open", dbname);
	}
#else
	b.flags = 0;
	b.cachesize = 0;
	b.maxkeypage = 0;
	b.minkeypage = 0;
	//TODO
	//b.psize = 1024; //this creates 3072 .db files with 1 key
	b.psize = 0;		//this create 8192 .db files with 1 key
	//B.PSIZE = 512;  //thi512s create 3072 .db files with 1 key
	b.compare = NULL;
	b.prefix = NULL;
	b.lorder = 0;
	mode = umask(0);
	if((dbp =
	    dbopen(dbname, bdbopenflags | O_RDWR, 0666, DB_BTREE, &b)) == NULL) {
		DBG("db2_open - unable to open %s\n", dbname);
		ret_val = 1;
	}
#endif

	umask(mode);
	free(basec);
	if(ret_val){
		dbp_close(dbp);
		dbp=NULL;
	}
	DBG("db2_open - dbname %s dbp = %p ret_val = %i\n", dbname, dbp, ret_val);
	return dbp;

}

/***************************************************************
clear_hdbt_hash - close all database pointers in hash.
!.. not used anymore
***************************************************************/
int clear_hdbt_hash()
{
	DBT key, data;
	DB *hp = NULL;
#ifdef DB3_DB4
	DBC *dbcp;
#endif
	int ret = 0;
	int i = 0;
	char list[HDB_LIST_MAX];
	int hashkey = 0;
	struct hash_hive *h = NULL;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	DBG("clear_hdbt_hash\n");

	if(hdb_cache_type == HDB_CACHE_HASH && hdbt_hash && hdbt_hash->dbp) {
		hp=hdbt_hash->dbp;
#ifdef DB3_DB4
		if((ret = hp->cursor(hp, NULL, &dbcp, 0)) != 0) {
			DBG("clear_hdbt_hash - cursor error %s\n", db_strerror(ret));
			return HDB_CURSOR_ERROR;
		}
#endif
		while(1) {
#ifdef DB3_DB4
			ret = dbcp->c_get(dbcp, &key, &data, DB_FIRST);
#else
			ret = (*hp->seq) (hp, &key, &data, R_FIRST);
#endif
			if(ret == 0) {
				strncpy(list, key.data, sizeof(list));
				list[key.size] = '\0';
				h = data.data;
				hashkey = atoi(list);
				DBG("clear_hdbt_hash %i key=%s dbp=%p\n", i, list, h->hdbt);
				if(del_hdbt_from_hash(hashkey)) {
					DBG("clear_hdbt_hash %i key=%s dbp=%p\n", i, list, h->hdbt);
					assert(0);
				}
				i++;
			}
			else {
#ifdef DB3_DB4
				ret = dbcp->c_get(dbcp, &key, &data, DB_FIRST);
#else
				ret = (*hp->seq) (hp, &key, &data, R_FIRST);
#endif
				if(ret == 0) {
					strncpy(list, key.data, sizeof(list));
					list[key.size] = '\0';
					assert(0);
				}
			}
		}
	}
#ifdef DB3_DB4
	dbcp->c_close(dbcp);
#endif
	return 0;
}

/*********************************************
hdb_print_hash_cache
TODO.. only used for debugging.
**********************************************/
int hdb_print_hash_cache()
{
	DBT key, data;
#ifdef DB3_DB4
	DBC *dbcp;
#endif
	DB *dbp=NULL;
	int ret = 0;
	int i = 0;
	char list[HDB_LIST_MAX];
	struct hash_hive *h = NULL;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	DBG("hdb_print_hash_cache()\n");
	if(hdb_cache_type == HDB_CACHE_HASH && hdbt_hash) {
		dbp = (DB *)hdbt_hash->dbp;
#ifdef DB3_DB4
		if((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
			//dbp->err(dbp, ret, "DB->cursor"); 
			DBG("%s", "hdb_print_cache - cursor error\n");
			return HDB_CURSOR_ERROR;
		}
		ret = dbcp->c_get(dbcp, &key, &data, DB_FIRST);
#else
		ret = (*dbp->seq) (dbp, &key, &data, R_FIRST);
#endif
		while(ret == 0) {
			strncpy(list, key.data, sizeof(list));
			list[key.size] = '\0';
			h = data.data;
			DBG("hash %i key=%s dbp=%p\n", i, list, h->hdbt);
			i++;
#ifdef DB3_DB4
			ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
#else
			ret = (*dbp->seq) (dbp, &key, &data, R_NEXT);
#endif
		}
	}
#ifdef DB3_DB4
	dbcp->c_close(dbcp);
#endif
	return 0;
}

/*****************************************************
_hdbcur_to_dbcur - translate cursor.
note! this are different in different db version so we translate
to internal format.
*****************************************************/
int _hdbcur_to_dbcur(int cursor){
	switch(cursor){
#ifdef DB3_DB4
		case HDB_FIRST: return DB_FIRST;
		case HDB_NEXT: return DB_NEXT;
		case HDB_PREV: return DB_PREV;
		case HDB_LAST: return DB_LAST;
#else
		case HDB_FIRST: return R_FIRST;
		case HDB_NEXT: return R_NEXT;
		case HDB_PREV: return R_PREV;
		case HDB_LAST: return R_LAST;
#endif
		default:
			assert(0);
			break;
	}
	return cursor;
	
}

#ifdef WITH_BDB_LOCKING
int db_unlock(HDB *hdbp){
	int ret=0;
	//DB_LOCK *dblock;

	DBG("db_unlock()\n");
	if(hdb_lock_type == HDB_LOCK_NONE){
		return 0;
	}

	if(hdb_dbenv==NULL){
		DBG("db_unlocked() - no env\n");
		return 1;
	}

	//dblock = (DB_LOCK*)&hdbp->dblock;
	ret = hdb_dbenv->lock_put(hdb_dbenv, hdbp->dblock); 
	switch(ret){
	case 0: 
		DBG("db_unlock() - pass\n");
		/*
		if ((ret = hdb_dbenv->lock_id(hdb_dbenv, hdb_locker)) != 0) {
                	DBG("db_unlock() - lock_id\n");
                	return 1;
        	}
		*/
		free(hdbp->dblock);
		hdbp->dblock=NULL;
		return 0;
	case DB_LOCK_NOTGRANTED:
		
		hdb_dbenv->err(hdb_dbenv, ret, NULL);
		assert(0);
		break;
	case DB_LOCK_DEADLOCK:
		hdb_dbenv->err(hdb_dbenv, ret, NULL);
		assert(0);
		break;
	default:
		hdb_dbenv->err(hdb_dbenv, ret, NULL);
		assert(0);
		break;
	}
	free(hdbp->dblock);
	DBG("db_unlock() - failed\n");
	assert(0);
	return 2;
}

int db_read_lock(HDB *hdbp){
	DBG("db_read_lock()\n");
	return db_lock(hdbp, DB_LOCK_READ);
}

int db_write_lock(HDB *hdbp){
	DBG("db_write_lock()\n");
	return db_lock(hdbp, DB_LOCK_WRITE);
}

int db_lock(HDB *hdbp, int type){
	int ret=0;
	//DB_LOCK dblock;
	char objbuf[255];
	DBT lock_dbt;

#ifndef WITH_BDB_LOCKING
	assert(0);
#endif

	//TODO.. some checking please	
	check_env();


	if(hdb_lock_type == HDB_LOCK_NONE){
		DBG("db_lock() - locks are disabled\n");
		return 0;
	}
	if(hdbp->lock_id == 0){
		if ((ret = hdb_dbenv->lock_id(hdb_dbenv, &hdbp->lock_id)) != 0) {
			DBG("db_lock() - Unable to get locker id\n");
			hdbp->lock_id=0;
			assert(0);
			return 1;
		}
	}

	//TODO... this is not needed to do every time
	sprintf(objbuf, "hdb.global");
	memset(&lock_dbt, 0, sizeof(lock_dbt));
	lock_dbt.data = objbuf;
	lock_dbt.size = (u_int32_t)strlen(objbuf);

	hdbp->dblock = malloc(sizeof(DB_LOCK));
	memset(hdbp->dblock, 0, sizeof(DB_LOCK));
	//TODO.. add timeout.. and check for deadlocked database
	ret = hdb_dbenv->lock_get(hdb_dbenv, hdbp->lock_id, 0, &lock_dbt, type, hdbp->dblock); 
	if(ret==0){
		DBG("db_lock() -  succeded\n");
		//hdbp->dblock = (void*) &dblock;	
		return 0;
	}
	hdbp->dblock=NULL;
	DBG("db_lock() - failed type %i ret %i\n", type, ret);
	hdb_dbenv->err(hdb_dbenv, ret, "db_lock - failed");
	assert(0);
	return 1;
}
#endif

/*****************************************************
lock - internal lock function.
*****************************************************/
int _hdb_lock(int fd)
{

	int i = 0;
	struct flock lock;

	lock.l_type = F_WRLCK;
	//lock.l_type = F_WDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	DBG("lock - trying 100 times to lock the file fd=%i\n", fd);
	//do max 100 retries for the lock
	for(i = 0; i < 100; i++) {
		if(!fcntl(fd, F_SETLK, &lock)) {
			DBG("lock succeeded\n");
			return 0;
		}
		usleep(10);
	}
	DBG("lock - failed to lock.  unlocking and returning\n");
	_hdb_unlock(fd);
	if(fcntl(fd, F_SETLK, &lock)) {
		return 1;
	}

	return 1;
}
/******************************************************************
unlock - internal unlock function
******************************************************************/
void _hdb_unlock(int fd)
{

	struct flock lock;

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if((fcntl(fd, F_SETLK, &lock)) == -1) {
		DBG("_hdb_unlock - fcntl failed\n");
	}

}

/***************************************************************
get_link - return the value that the link points to 
***************************************************************/
char *get_link(const char *listkey)
{

	char lk[HDB_PATH_MAX];
	char ll[HDB_PATH_MAX];
	char *list;
	char *key;
	char *p = NULL;

	//2 formats supported
	//key@list/sublist or list/sublist/key
	if((p = strstr(listkey, "@"))) {
		strncpy(lk, p + 1, sizeof(lk));
		strncpy(ll, listkey, sizeof(ll));
		ll[p - listkey] = '\0';
		list = ll;
		key = lk;
	}
	else {
		strcpy(lk, listkey);
		strcpy(ll, listkey);
		list = dirname(ll);
		key = basename(lk);
	}

	return hdb_get_val(list, key);
}

/**************************************************
get_file - return head -1 on file.
**************************************************/
char *get_file(const char *file)
{
	char buf[512];
	struct stat s;

	if(lstat(file, &s) == -1) {
		return NULL;
	}
	snprintf(buf, sizeof(buf), "head -1 %s 2>/dev/null", file);
	return get_exec(buf);
}
/**************************************************
get_exec - return cmd | head -1
**************************************************/
char *get_exec(const char *cmd)
{

	FILE *fp;
	char *result = NULL;
	int ret = 0;

	result = malloc(HDB_PATH_MAX);

	DBG("get_exec() runing cmd '%s'\n", cmd);
	if((fp = popen(cmd, "r")) == NULL) {
		DBG("unable to fopen %s\n", cmd);
		free(result);
		return NULL;
	}

	if(fgets(result, HDB_PATH_MAX, fp) != NULL) {
		//we only want one line without \n
		if(result[strlen(result) - 1] == '\n') {
			result[strlen(result) - 1] = '\0';
		}
	}
	if((ret = pclose(fp))) {
		DBG("get_exec returned %i\n", ret);
		strcpy(result, "");
	}
	return result;
}

/***************************************************
get_hash_key - return inode for root/list or -1 on error
****************************************************/
int get_hash_key(const char *root, const char *list)
{
	char rootlist[HDB_PATH_MAX];
	set_rootlist(root, list, sizeof(rootlist), rootlist); 
	return (int) get_inode(rootlist);
}

/****************************************************
get_inode - return inode for file or -1 on error
****************************************************/

//TODO.. make this return int
ino_t get_inode(const char *dir)
{
	struct stat buf;
	if((lstat(dir, &buf) == -1)) {
		return -1;
	}
	return buf.st_ino;
}

int hdb_dump_glob(char *list, char *key, char *value){
	return hhdb_dump_glob(&hdb_default, list, key, value);
}

int hhdb_dump_glob(HDB *hdbp, char *list, char *key, char *value){
	char rootlist[HDB_PATH_MAX];
	char path[HDB_PATH_MAX];
	char *errbuf;
	int errcode;
	int root_size;
	struct hdb_regexp_t re;
	char *p=NULL;
	int i=0;
	glob_t globbuf;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}

	if((errcode=regcomp(&re.key,key, REG_EXTENDED))){
		errbuf=malloc(255);
		regerror(errcode, &re.key, errbuf,  255);
		free(errbuf);
		return 1;
	}
	if((errcode=regcomp(&re.value,value, REG_EXTENDED))){
		errbuf=malloc(255);
		regerror(errcode, &re.value, errbuf, 255);
		free(errbuf);
		regfree(&re.key);
		return 1;
	}

	snprintf(rootlist, sizeof(rootlist), "%s%s", hdbp->root, list);
	root_size = strlen(hdbp->root);
	glob(rootlist, GLOB_ONLYDIR, NULL, &globbuf);
	
	DBG("hdb_dump_glob - rootlist=%s\n", rootlist);	
	DBG("hdb_dump_glob - matches %i\n", globbuf.gl_pathc);
	for(i=0;i<globbuf.gl_pathc;i++){
		//sanity check
		//remove rootlist again
		if(!realpath(globbuf.gl_pathv[i], path)){
			globfree(&globbuf);
			regfree(&re.value);
			regfree(&re.key);
			return 1;
		}
		DBG("hdb_dump_glob - path %s -> realpath %s\n", globbuf.gl_pathv[i], path);
		if(strlen(path) < root_size-1){
			DBG("globbing outside root strlen(%s) = %i root_size %i\n",
			path, strlen(path), root_size);
			globfree(&globbuf);
			regfree(&re.value);
			regfree(&re.key);
			return 1;
		}
		p = path;
		p += root_size;
		if(!strcmp(p, "")){
			strcpy(p, ".");
		}	
		
		_print_list_regex(hdbp, 0, p, &re);
	}
	globfree(&globbuf);
	regfree(&re.value);
	regfree(&re.key);
	return 0;
}


int hdb_dump_regex(char *list, char *key, char *value){
	return hhdb_dump_regex(&hdb_default, list, key, value);
}

int hhdb_dump_regex(HDB *hdbp, char *list, char *key, char *value){

	char *errbuf;
	int errcode=0;
	struct hdb_regexp_t re;
	int len=0;

	if(!hdbp->root_is_ok && hhdb_set_root(hdbp, getenv("HDBROOT"))) {
		return HDB_ROOT_ERROR;
	}
	
	DBG("hdb_dump_regex list=%s key=%s value=%s\n", list, key, value);
	if(list){
		len=strlen(list);
		if(list[len-1]=='/'){
			list[len-1]=0;
		}
	}
	if(list && !strcmp(list, "*")){
		list=NULL;
	}
	if(key && !strcmp(key, "*")){
		key=NULL;
	}
	if(value && !strcmp(value, "*")){
		value=NULL;
	}
	//NULL -> ".*"
	//""   -> ".*"
	if((errcode=regcomp(&re.list, (list && *list) ? list : ".*", REG_EXTENDED))){
		errbuf=malloc(255);
		regerror(errcode, &re.list, errbuf,  255);
		free(errbuf);
		return 1;
	}	
	if((errcode=regcomp(&re.key, (key && *key) ? key : ".*", REG_EXTENDED))){
		errbuf=malloc(255);
		regerror(errcode, &re.key, errbuf,  255);
		free(errbuf);
		regfree(&re.list);
		return 2;
	}
	if((regcomp(&re.value, (value && *value) ? value : ".*", REG_EXTENDED))){
		errbuf=malloc(255);
		regerror(errcode, &re.value, errbuf, 255);
		free(errbuf);
		regfree(&re.list);
		regfree(&re.key);
		return 3;
	}

	_dump_regex(hdbp, ".", &re); 
	_regex_free(&re);
	return 0;
}

void _regex_free(hdb_regex *re){

	assert(re); 

	regfree(&re->list);
	regfree(&re->key);
	regfree(&re->value);
}

int _dump_regex(HDB *hdbp, char *list, hdb_regex *re){
	char sublist[HDB_LIST_MAX];
	HDBC *hdbc=NULL;;

	if(list && *list == '/'){
		list++;
	}
	DBG("hdb_dump_flat startlist=%s\n", list);

	if(!regexec(&re->list, list, 1, &re->match_info, 0)){
		DBG("list regex match %s\n", list);
		_print_list_regex(hdbp, 0, list, re);
	}
	else {
		DBG(" list not match %s\n", list);
	}

	DBG("hdb_dump_flat calling hdb_get_sublist_cur_full %s\n", list);
	if((hdbc = hhdb_sublist_copen(hdbp,hdbc,list)) != NULL){
		while(!hdb_sublist_cget_full(hdbc, sublist, sizeof(sublist))){
			if(_dump_regex(hdbp, sublist, re)){
				hdb_sublist_cclose(hdbc);
				return 1;
			}
		}
		hdb_sublist_cclose(hdbc);
	}

	return 0;
}

int _print_list_regex(HDB *hdbp, int fmt, char *list, hdb_regex *re){

	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	HDBC *hdbc=NULL;
	
	DBG("_print_list_regex %s\n", list);

	if((hdbc=hhdb_copen(hdbp, hdbc, list))!=NULL){
		while(!(hdb_cget(hdbc, key, value))){
			if(!regexec(&re->key, key, 1, &re->match_info,0) &&
			   !regexec(&re->value, value, 1, &re->match_info,0)) 
			{
				//we found a hit
				hdb_print_output(hdbp->file, fmt, hdbp->atime, hdbp->mtime, list, key, value);
			}
			else {
				DBG(" key value regexp no match %s %s\n", key, value);
			}
			//snprintf(buf, sizeof(buf), "%s/%s=%s\n", list, key, value);
		}
		hdb_cclose(hdbc);
	}
	else {
		//   empty list. now what
		//1) Make sure we want list output
		//2) Make sure we don't have a key regexp (empty lists have no keys)
		//3) Make sure we dont't have a value regexp.
		//if(lflag && !kflag && !vflag){
		if((fmt & HDB_OUTPUT_LIST) && 
		   !(fmt & HDB_OUTPUT_KEY) && 
		   !(fmt & HDB_OUTPUT_VALUE))
		{
			hdb_print_output(hdbp->file,hdbp->atime, hdbp->mtime, fmt,list, "", "");
		}
	}
	return 0;
}

/*
int hdb_extract(const char *lkv, int size, char *list, char *key, char *value){
        char *l=NULL;
        char *k=NULL;
        char *eq=NULL;
        int klen=0;

	assert(list && key && value);
	strcpy(list,"");
	strcpy(key, "");
	strcpy(value, "");

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
*/

int hdb_get_abs_list(const char *root, const char *list, int size, char *abslist){
	return set_rootlist(root, list, size, abslist);
}

/*****************************************************
set_rootlist - return a root list take account abs path.
*****************************************************/
int set_rootlist(const char *root, const char *list, int size, char *rootlist){

	if(*list == '/'){
		snprintf(rootlist, size, "%s", list);
	}
	else if(!strcmp(list, ".")){
		snprintf(rootlist, size, "%s", root);
	}	
	else {
		snprintf(rootlist, size, "%s%s", root, list);
	}

	return 0;
}

//test functions
int test_dirlist_cache(){
	int i = 0;
	/*
	struct dirlist *head=NULL;
	struct dirlist *mitten1=NULL;
	struct dirlist *mitten2=NULL;
	struct dirlist *tail=NULL;
	*/
	struct dirlist *dl=NULL;
	struct linkedlist_t ll;
	
	ll.tail=NULL;
	ll.head=NULL;	

	//head = add_dirlist_cache(&ll, "/head");
	//mitten1 = add_dirlist_cache(&ll, "/var/db/hdb/mitten1");
	//mitten2 = add_dirlist_cache(&ll, "/var/db/hdb/mitten2");
	//tail = add_dirlist_cache(&ll, "/var/db/hdb/tail");

	//t = add_dirlist_cache(&ll, "/var/db/hdb/head");
	//t = add_dirlist_cache(head, "/var/db/hdb/mitten1");

	//close_dirlist_cache(&ll, "/var/db/hdb/head");
	//close_dirlist_cache(&ll, "/var/db/hdb/mitten2");
	//close_dirlist_cache(&ll, "/var/db/hdb/tail");
	
	
	for(dl=ll.head;dl;dl=dl->next){
		printf("%i -- %i\n", i, dl->key);
	}
	
	
	return 0;
}

int hdb_close(){
	int ret = 0;

	DBG("hdb_close()\n");
#ifdef WITH_BDB_ENVIROMENT 
	if(hdb_dbenv==NULL){
		return 0;
	}
#endif
	WRITE_LOCK_DEFAULT
	_sync_close();
#ifdef WITH_DBD_ENVIRONMENT
        if ((ret = hdb_dbenv->close(hdb_dbenv, 0)) != 0) {
                DBG("dbenv->close: %s\n", db_strerror(ret));
        }
	hdb_dbenv=NULL;
	//TODO.. remove input parameter
	env_unlock(env_lock_fd);
	env_lock_fd=0;
#endif
	UN_LOCK_DEFAULT
	//we dont need locks anymore
	hhdb_close(&hdb_default);

	//TODO.. make sure any dirlist cache is closed and freed
	//close_dirlist_cache(hdbp->dirlist_head, 0);
	return ret;
}

int hhdb_close(HDB *hdbp){
#ifdef WITH_BDB_LOCKING 
	int ret=0;
	if(hdb_dbenv==NULL){
		return 0;
	}
	if(hdbp->lock_id==0){
		DBG("hhdb_close - trying to close unlocked locked\n");
		return 1;
	}
	//assert(hdbp->lock_id);

	if(hdbp->lock_id){
		if((ret=hdb_dbenv->lock_id_free(hdb_dbenv, hdbp->lock_id))!=0){
			fprintf(stderr, "Unable to close lock\n");
		}
		hdbp->lock_id=0;
	}	
#endif
	// clear and free any open directories we might have
	clear_dirlist_cache((struct linkedlist_t *) hdbp->dirlist_head);
	if(hdbp->dirlist_head){
		free(hdbp->dirlist_head);
		hdbp->dirlist_head=NULL;
	}
	return 0;
}

#ifdef WITH_BDB_ENVIROMENT 

int env_close(){
	return hdb_close();
}

void env_cleanup(){
	//TODO.. does this work?
	system("rm /var/db/__* >/dev/null");
}

int env_open(int join){
	hdb_dbenv = env_setup("/var/db", "/var/db", stderr, "hdb", join); 
	if(hdb_dbenv==NULL){
		DBG("Unable to open db enviroment\n");
		return 1;
	}
	return 0;
}

DB_ENV * 
env_setup(char *home, char *data_dir, FILE *errfp, char *progname, int join){
	DB_ENV *dbenv;
	int ret;

	//Create an environment and initialize it for additional error reporting. 
	if ((ret = db_env_create(&dbenv, 0)) != 0) { 
		DBG("%s: %s\n", progname, db_strerror(ret)); 
		assert(0);
		return NULL; 
	} 
	dbenv->set_errfile(dbenv, errfp); 
	dbenv->set_errpfx(dbenv, progname);

	//Specify the shared memory buffer pool cachesize: 5MB.  
	//Databases are in a subdirectory of the environment home. 
	if ((ret = dbenv->set_cachesize(dbenv, 0, 5 * 1024 * 1024, 0)) != 0) { 
		dbenv->err(dbenv, ret, "set_cachesize"); 
		assert(0);
		goto err; 
	} 

	if ((ret = dbenv->set_data_dir(dbenv, data_dir)) != 0) { 
		dbenv->err(dbenv, ret, "set_data_dir: %s", data_dir); 
		assert(0);
		goto err; 
	}

	//If we want transactions... TODO
	//Open the environment with full transactional support. 
	//if ((ret = dbenv->open(dbenv, home, DB_CREATE | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN, 0)) != 0) { 

	//concurent storage enviroment
	//DB_CDB_ALLDB - lock the whole db.. we need this??
	//if ((ret = dbenv->open(dbenv, home, DB_CREATE | DB_INIT_MPOOL | DB_INIT_CDB | DB_CDB_ALLDB, 0)) != 0) { 
	if(join){
		ret = dbenv->open(dbenv, home, DB_JOINENV, 0);
	}
	else {
#if defined(HDBD) && defined(DB3_DB4) 
		//DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL
		//ret = dbenv->open(dbenv, home, DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_CDB | DB_THREAD, 0);
		ret = dbenv->open(dbenv, home, DB_CREATE | DB_INIT_MPOOL | DB_INIT_CDB | DB_THREAD, 0);
#else
		ret = dbenv->open(dbenv, home, DB_CREATE | DB_INIT_MPOOL | DB_INIT_CDB , 0);
#endif
	}
	if(ret!=0){
		dbenv->err(dbenv, ret, "environment open: %s ret=%i", home, ret); 
		assert(0);
		goto err;
	}
	return (dbenv);

	err: 
		assert(0);
		(void)dbenv->close(dbenv, 0); 
		return (NULL); 
}

/******************************************************
check_env
*******************************************************/
int check_env(){
    	struct flock lock;
	char *file = "/var/db/hdbenv";
	int fd;
	int ret=0;

	// we have an enviroment already
	if(env_lock_fd){
		assert(hdb_dbenv);
		return 0;
	}

	//create 644 lockfile 
	if((fd = open(file, O_RDWR|O_CREAT,  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1) {
		DBG("open failed %s\n", strerror(errno));
		fprintf(stderr, "open failed %s\n", strerror(errno));
		assert(0);
		//critical.. panic
		return 1;
	}
    	lock.l_whence = SEEK_SET;
    	lock.l_start = 0;
    	lock.l_len = 0;

	// Get a write lock on enviroment. Dont wait
	lock.l_type = F_WRLCK;
    	if((ret=fcntl(fd, F_SETLK, &lock))!=-1){
		// If we got a lock we are first in line so recreate enviroment
		// Create a new fresh enviroment 
		env_close();
		env_cleanup();
		env_open(0); 

		// Convert to read lock. Don't wait
		lock.l_type = F_RDLCK;
    		if((ret=fcntl(fd, F_SETLK, &lock))==-1){
			assert(0);
			return 1;
		}
		// Others can join the enviroment now
	}
	else {
		// Enviroment is owned by someone else. 
		// We must wait for any write lock to finish (some other process is creating the enviroment)
		lock.l_type = F_RDLCK;
    		if((ret=fcntl(fd, F_SETLKW, &lock))==-1){
			assert(0);
			return 1;
		}
		// Join existing enviroment
		env_open(1); 
	}
	env_lock_fd=fd;

	return 0;
}
int env_unlock()
{
	struct flock lock;

	if(env_lock_fd<=0){
		assert(0);
		return 1;
	}

	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if((fcntl(env_lock_fd, F_SETLK, &lock)) == -1) {
		return 1;
	}
	return 0;
}
#endif

int print_dirlist_cache(HDB *hdbp)
{

	struct linkedlist_t *dl = (struct linkedlist_t*) hdbp->dirlist_head;
	struct dirlist *p = NULL;

	for(p=dl->head; p; p=p->next){
		printf("%p key %i\n", (void*)p, p->key);
	}
	return 0;
}

void unpack_hive(char *buffer, struct hive_t *hive){
	assert(buffer);
	hive->size = *((int *)buffer);
	hive->meta = (int*)(buffer + sizeof(int));
	hive->value = buffer + sizeof(int) + (sizeof(int) * META_SIZE);
}

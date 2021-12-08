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

#ifndef __HDB_H
#define __HDB_H 

#include <stdio.h>
#include <dirent.h> //for DIR
#include <time.h>   //for time_t

#define HDBROOT 	"/var/db/hdb/"

// theese 3 should be removed
#define BASEDIR 	"/var/db/hdb/" 
#define MAXLENGTH	512 	/* maximum size of a key and value */
#define MAXLENGTHX2 	1024 	/* MAXLENGTH * 2 */

#define HDB_KEY_MAX		1024	/* max length of keys */
#define HDB_VALUE_MAX		4096	/* max length of value */
#define HDB_LIST_MAX		255	/* max length of lists. must be less than NAME_MAX in limits.h */
#define HDB_PATH_MAX		4096	/* max length of paths. must be less than PATH_MAX in limits.h */
#define MAXMESSAGE 		512 	/* maximum size of a hdbd message sent to a client */

//must be root to run this
//#define HDB_LOCKFILE "/var/lock/subsys/hdb"
#define HDB_LOCKFILE 	"/tmp/.hdb"
#define HDB_PORT 	12429
#define HDB_PROTO 	1 //bump up after each underlying db updates and udpates to hdb hives 
#define HDB_NET_TIMEOUTSEC 20

//DONT Set any of the HDBX structs values manually. 
//These functions are used to create thread save clients 

//HDB
struct __hdb; typedef struct __hdb HDB;
struct __hdb {
	char root[MAXLENGTH];
	FILE *file; //
	int dberrno;
	int root_is_ok;		//root has been verified
	time_t mtime; 		//updated after each call
	time_t atime; 		//accesstime
	int update_stat; 	//
	void *dblock;	 	//DB_LOCK dblock;
	void *dirlist_head; 	//		
	//u_int32_t lock_id; 	//struct __db_lock_u dblock;
	int lock_id; 	//struct __db_lock_u dblock;
};

//predefined HDB this are used internally when invoking hdb_ API functions as wrappers to hhdb_ API.
static HDB hdb_default;

//HDB CURSOR
struct __hdbc; typedef struct __hdbc HDBC;
struct __hdbc {
	DIR *dp;		
	char *parent_list;
	time_t mtime; 	//updated after each call
	time_t atime; 	//accesstime
	int update;
	void *dbp;	//DB struct	
	void *dbcp; 	//DBC struct (db3 or higher)
};

//HDB THANG 
struct __hdbt; typedef struct __hdbt HDBT;
struct __hdbt {
	void *dbp;	//DB struct	 
	void *dbcp; 	//DBC struct (db3 or higher)
};

//HDB STAT 
struct __hdbs; typedef struct __hdbs HDBS;
struct __hdbs {
	time_t atime;    /* time of last access */
	time_t mtime;    /* time of last modification */
};

//not used yet. still in development
struct HDBOP {
	char list[MAXLENGTH];
	char key[MAXLENGTH];
	char value[MAXLENGTH];
	int x;
	int y;
};

//creating and deleting lists
int hdb_create_list(const char *list);
int hdb_delete_list(const char *list);
int hhdb_create_list(HDB *hdb, const char *list);
int hhdb_delete_list(HDB *hdb, const char *list);

//fetching values
int hdb_list_stat(const char *list, HDBS *stat);
int hhdb_list_stat(HDB *hdbp, const char *list, HDBS *stat);
int hdb_stat(const char *list, const char *key, HDBS *stat);
int hhdb_stat(HDB *hdbp, const char *list, const char *key, HDBS *stat);

char *hdb_get_val(const char *list, const char *key); //returns a malloced string
char *hhdb_get_val(HDB *hdbp, const char *list, const char *key); //returns a malloced string
int hdb_get_nval(const char *list, const char *key, int size, char *val);
int hhdb_get_nval(HDB *hdbp, const char *list, const char *key, int size, char *val);
char *hdb_get_pval(const char *list, const char *key); //returns a static buffer
int hdb_get_abs_list(const char *root, const char *list, int, char *); 
int hdb_get_int(const char *list, const char *key);
int hhdb_get_int(HDB *hdbp, const char *list, const char *key);
long long hdb_get_long(const char *list, const char *key);
int hdb_get_bool(const char *list, const char *key);
int hdb_get_rec(const char *list, int record_nbr, char *key, char *val); //slow with db 1.85
int hhdb_get_rec(HDB *hdbp, const char *list, int record_nbr, char *key, char *val); //slow with db 1.85

//this will fetch the raw value from a list
//a link will be reported as the link and type will be of DB_TYPE_LINK
//a exec will return the string to be executed
char * hdb_get_raw(const char *src, const char *key, int *type);
char * hhdb_get_raw(HDB *hdb, const char *src, const char *key, int *type);

//fetching lists
//use thread safe versions instead!
int hdb_get_sublist_cur(const char *parent_list, int cursor, char *list);
int hhdb_get_sublist_cur(HDB *hdbp, const char *parent_list, int cursor, char *list);
int hdb_get_sublist_cur_full(const char *parent_list, int cursor, char *list);
int hhdb_get_sublist_cur_full(HDB *hdbp, const char *parent_list, int cursor, char *list);

int hdb_get_sublist(const char *parent_list, int list_nbr, char *list); //slow
int hhdb_get_sublist(HDB *hdbp, const char *parent_list, int list_nbr, char *list); //slow
int hdb_get_sublist_full(const char *parent_list, int list_nbr, char *list); //slow
int hhdb_get_sublist_full(HDB *hdbp, const char *parent_list, int list_nbr, char *list); //slow
int hhdb_get_size(HDB *hdbp, const char *list); // Returns the number of sub entries in the list
int hdb_get_size(const char *list); // Returns the number of sub entries in the list
int hhdb_get_cur(HDB *hdbp, const char *list, int cursor, char *key, char *value);
int hdb_get_cur(const char *list, int cursor, char *key, char *value);

//fetching values in list. thread safe
HDBC *hhdb_copen(HDB *hdbp, HDBC *hdbc, char *list);
HDBC *hdb_copen(HDBC *hdbc, char *list);
int hdb_cget(HDBC *hdbc, char *list, char *value);
//int hdb_cget_full(HDBT *hdbdc, char *list, char *value); 
int hdb_cclose(HDBC *hdbc);

//fetching lists - client responsible for cursors.
HDBC *hdb_sublist_copen(HDBC *hdbc, char *parent_list);
HDBC *hhdb_sublist_copen(HDB *hdbp, HDBC *hdbc, char *parent_list);
int hdb_sublist_cget(HDBC *hdbc, char *list, int size);
int hdb_sublist_cget_full(HDBC *hdbc, char *list, int size);
int hdb_sublist_cclose(HDBC *hdbc);

int hdb_scan_sublist(const char *parent_list, char ***lists);
int hdb_scan_sublist_full(const char *parent_list, char ***lists);
//thread safe versions
int hhdb_scan_sublist(HDB *hdbp, const char *parent_list, char ***lists);
int hhdb_scan_sublist_full(HDB *hdbp, const char *parent_list, char ***lists);
int hdb_scan_sublist_close(char **list);

//Setting values
int hdb_set_val(const char *list, const char *key, const char *val);
int hhdb_set_val(HDB *hdbp, const char *list, const char *key, const char *val);
int hdb_set_int(const char *list, const char *key, int val);
int hhdb_set_int(HDB *hdbp, const char *list, const char *key, int val);
int hdb_set_long(const char *list, const char *key, long long val);
int hdb_set_link(const char *list, const char *key, const char *link);
int hdb_set_exec(const char *list, const char *key, const char *cmd);
int hdb_set_file(const char *list, const char *key, const char *cmd);
//set the raw value.. should be one of the HDB_TYPE_XXX types
//this is only used internally, use wrappers (set_link, set_exec, set_val, set_int)
int hdb_set_raw(const char *list, const char *key, const char *val, int type);

//thread safe version
int hhdb_set_raw(HDB *hdb, const char *list, const char *key, const char *val, int type);

//deleting keys
int hdb_del_val(const char *list, const char *key);
int hhdb_del_val(HDB *hdbp, const char *list, const char *key);

//misc
int hdb_cut(const char *lkv, int size, char *list, char *key, char *value);
int hdb_get_error(); //not used
int hhdb_get_error(HDB *hbdp);
int hdb_sync(); //save all data to disc
int hhdb_sync(HDB *hdbp); //save all data to disc
int hdb_incr(const char *list, const char *key); //lock and add one to value
int hhdb_incr(HDB *hdbp, const char *list, const char *key); //lock and add one to value
int hdb_add(const char *list, const char *key, int num); //lock and num to value
int hhdb_add(HDB *hdbp, const char *list, const char *key, int num); //lock and num to value
int hdb_exist(const char *list); //returns 0 if the list exist
int hhdb_exist(HDB *hdb, const char *list);
int hdb_key_exist(const char *list, const char *key); //return 0 if the key exist
int hhdb_key_exist(HDB *hdbp, const char *list, const char *key); //return 0 if the key exist
int hdb_wipe(); //remove all lists from dbroot, this will wipe all lists
int hhdb_wipe(HDB *hdbp); //remove all lists from dbroot, this will wipe all lists
int hdb_mv(const char *src, const char *target); //this is faster than delete
int hhdb_mv(HDB *hdbp, const char *src, const char *target); //this is faster than delete
int hdb_set_root(const char *root);
int hhdb_set_root(HDB *hdbp, const char *root);
int hdb_verify_root(const char *root); //return 0 on valid root
char *hdb_get_root();
char *hhdb_get_root(HDB *hdbp);
int hdb_read_lock(); //global enviroment read lock
int hdb_write_lock(); //global enviroment write lock
int hdb_release_lock(); //global enviroment release lock
int hdb_lock(const char *list, int timeout); //lock a list
int hdb_unlock(const char *list);
int hdb_check_lock(const char *list);
int hdb_trig_disable(int type, const char *list, const char *key);
int hdb_trig_enable(int type, const char *list, const char *key);
int hdb_set_user(char *user);
int hdb_set_configi(int paramter, int value);
int hhdb_set_configi(HDB *hdbp, int paramter, int value);
int hdb_get_configi(int parameter);
int hhdb_get_configi(HDB *hdbp, int parameter);
int hdb_get_access();

//logfile stuff
int hdb_open_log(int facility, char *name);
int hdb_print_log(const char *format, ...);
void hdb_print_output(FILE *f, int format, int atime, int mtime, char *list, char *key, char *value);
int hdb_close_log();

int hdb_close();
int hhdb_close(HDB *hdbp);
//debug stuff
void print_cache();
int hdb_print_hash_cache(); //TODO.. remove

enum {
	HDB_FIRST,
	HDB_LAST,
	HDB_NEXT,
	HDB_PREV
};

int hdb_connect(const char *host, int port, char *user, char *pwd,  int crypt);
int hdb_disconnect();
char *hdb_net_get(const char *cmd);
char *hhdb_net_get(HDB *hdbp, const char *cmd);
int hdb_net_set(const char *cmd);
int hdb_net_print_fmt(FILE *fd, const char *cmd, int bash_friendly);
int hdb_net_print(FILE *fd, const char *cmd);

struct hdb_stat_t {
	time_t atime;
	time_t mtime;
	time_t ctime;	
};
typedef struct hdb_stat_t HDBSTAT;

/******************************************
 Regexp stuff	
******************************************/
int hhdb_dump_regex(HDB *hdbp, char *list, char *key, char *value);
int hdb_dump_regex(char *list, char *key, char *value);
int hhdb_dump_glob(HDB *hdbp, char *list, char *key, char *value);
int hdb_dump_glob(char *list, char *key, char *value);

//used by hhdb_dump_xxx
#define HDB_OUTPUT_LIST 	1
#define HDB_OUTPUT_KEY  	2
#define HDB_OUTPUT_VALUE 	4
#define HDB_OUTPUT_MTIME        8	
#define HDB_OUTPUT_ATIME        16 
#define HDB_OUTPUT_TIME         24 //atime + mtime 
#define HDB_OUTPUT_RELTIME      32 //relative time in minutes

enum { HDB_ACCESS_FILE, HDB_ACCESS_NETWORK };
enum { HDB_TRIG_WRITE, HDB_TRIG_READ, HDB_TRIG_MOVE, HDB_TRIG_DELETE, HDB_TRIG_CREATE };
enum { HDB_CACHE_NONE, HDB_CACHE_HASH, HDB_CACHE_BINARY, HDB_CACHE_LINEAR }; 
enum { HDB_TYPE_STR, HDB_TYPE_LINK, HDB_TYPE_INT, HDB_TYPE_EXEC, HDB_TYPE_FILE };
enum { HDB_LOG_NONE, HDB_LOG_CONSOLE, HDB_LOG_SYSLOG, HDB_LOG_FILE, HDB_LOG_HDB };
enum { HDB_LOCK_NONE, HDB_LOCK_BDB };

enum { HDB_CONFIG_HASH_MAX_SIZE, 
       HDB_CONFIG_HASH_SIZE, 
       HDB_CONFIG_CACHE_TYPE,
       HDB_CONFIG_LOG_FACILITY,
       HDB_CONFIG_LOCK,
       HDB_CONFIG_STAT
};

enum {
	HDB_DISABLE,
	HDB_ENABLE
};

enum { 
HDB_OK,			/* 0 - No error */
HDB_ERROR,		/* 1 - No further specified error */
HDB_INVALID_KEY, 	/* 2 - Key is NULL or has invalid characters */
HDB_INVALID_INPUT,	/* 3 - Invalid data was passed into the function. 0xBAADF00D */
HDB_KEY_NOT_FOUND,	/* 4 - Key Not found */
HDB_LIST_NOT_FOUND,	/* 5 - List Not found */
HDB_DBP_ERROR,		/* 6 - Unable to get dbp */
HDB_CURSOR_ERROR,	/* 7 - dbp->cursor errors */
HDB_BDB_ERROR, 		/* 8 - Error in BDB lib calls */
HDB_MALLOC_ERROR, 	/* 9 - Malloc failed */
HDB_OPEN_ERROR, 	/* 10 - Failed to open directory or file */
HDB_ROOT_ERROR,		/* 11 - Setting hdb root failed */ 
HDB_HIVE_ERROR,		/* 12 - The stored hives in BDB is mallformed */
HDB_SHELL_ERROR,	/* 13 - Failed to execute shell command. */
HDB_SYNC_ERROR,		/* 14 - Failed to sync */
HDB_SECURITY_ERROR,     /* 15 - Access denied to underlying files/directories */
HDB_NOT_IMPLEMENTED,    /* 16 - Some functions are not implemented in diff lib versions */
HDB_LOCK_ERROR,         /* 17 - Locking error */
HDB_NETWORK_ERROR,      /* 18 - Network error */
};

//the most used have 0 1 2 3 to speed up finding.
enum {
HDB_GET_VAL,
HDB_SET_VAL,
HDB_PRINT,
HDB_DUMP,
HDB_DELETE_LIST,
HDB_CREATE_LIST,
HDB_GET_REC,
HDB_GET_RAW,
HDB_SET_RAW,
HDB_SET_EXEC,
HDB_SET_FILE,
HDB_SET_LINK,
HDB_GET_SUBLIST,
HDB_GET_SUBLIST_CUR,
HDB_GET_SUBLIST_CUR_FULL,
HDB_GET_SUBLIST_FULL,
HDB_GET_SIZE,
HDB_GET_CUR,
HDB_DEL_VAL,
HDB_GET_ERROR,
HDB_PRINT_SUBLIST_FULL,
HDB_PRINT_LIST,
HDB_PRINT_LISTR,
HDB_SYNC,
HDB_INCR,
HDB_ADD,
HDB_EXIST,
HDB_WIPE,
HDB_EXIT,
HDB_PRINT_FULL,
HDB_PRINT_SUBLIST,
HDB_SET_ROOT,
HDB_GET_ROOT,
HDB_ROOT,
HDB_NONE,
HDB_DEBUG,
HDB_MOVE,
HDB_QUIT,
HDB_VERSION,
HDB_LOG,
HDB_HELP,
HDB_STATUS,
HDB_DUMP_FLAT,
HDB_UPDATE_VAL,
HDB_DUMP_FLAT_REGEX,
HDB_DUMP_FLAT_GLOB,
HDB_STAT,
HDB_PRINT_STAT,
HDB_CONFIG,
HDB_NUM_FUNC
};

#endif


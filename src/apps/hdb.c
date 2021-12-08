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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <libgen.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define CMD_VERSION "$Id: hdb.c,v 1.3 2006/05/26 13:00:39 hampusdb Exp $"
#define WITH_FILE_DUMPING 	1

int hdb_dump(int level, char *list);
int hdb_dump_flat(int level, char *list);
char * next_tag(char *buf, char *result, char *delim);
int    Oflag = 0; //LOCK
int hdb_net = 0; //use network-hdb to connect

void cleanup(){
	//this will sync and exit
	hdb_close();
	if(hdb_net){
		hdb_disconnect();
	}
	if(Oflag){
		hdb_unlock(NULL);
	}
	return;
}


//extern int opterr = 0;


void usage(){
	fprintf(stderr, "hdb client %s", CMD_VERSION);
#ifdef EVH_SUPPORT
	fprintf(stderr, " +trig");
#endif
	fprintf(stderr, "\n");

	fprintf(stderr, 
			"Usage: hdb [-l list] [switch \"args ..\"]\n"
			"   -l list       List to operate on\n"
			"   -c list       Create list\n"
			"   -r list       Remove list\n"
			"   -p list       Print all values from list\n"
			"   -P list       Print all values from list with full path\n"
			"   -s \"values\"   Set value to list\n" 
			"   -g key        Get value from list\n"
			"   -d key        Delete key from list\n"
			"   -k key        Key to use with -s switch\n"
			"   -n num        Get record number from list. (first = recno 1)\n"
			"   -L            Print list of lists\n"
			"   -S list       Print sublist of this list. (one level)\n"
			"   -Z list       Print sublist of this list with full path. (one level)\n"
			"   -N list       Print number of of elements in this sublist (one level)\n"
			"   -x list       Dump output in xml like format\n"
			"   -f list       Dump output in flat file format (list/sublist/key=value)\n"
			"   -m list       Use meta values\n"
			"   -t type       Use meta type (str,lnk,exe,fil,int). str=default\n"
			"   -T            Disable triggers (does nothing if not compiled with triggers)\n"
			"   -b            Output in bash friendly format (key=value)\n"
			"   -D            Output debug info\n"
			"   -w            Wipe all lists\n"
			"   -q            Quite output\n"
			"   -R <dir>      Set hdb root directory (env=HDBROOT)\n"
			"   -O <timeout>  Wait for timeout seconds and get lock in this list\n"
			"   -u <uri>      Connect to uri (hdb://user:pwd@host:port)\n"
			"   -h            Show this help\n"
			);
	exit(-1);

}

int mflag = 0;
int main(int argc, char **argv){

#if HDB_NET_SWITCH
	int found_lock=0; 
#endif
	int i = 0;
	int timeout = 0;
	int op = 0;
	char value[HDB_VALUE_MAX];
	char list[HDB_PATH_MAX];
	int type=HDB_TYPE_STR;
	char key[HDB_KEY_MAX];
	char recbuf[MAXLENGTH];
	char hdbroot[HDB_PATH_MAX];
	char cmdbuf[MAXLENGTH];
	char uri[MAXLENGTH];
	struct passwd *pw; 
	char host[255];
	int iport=HDB_PORT;
	char user[255];
	char pwd[255];
	char *p = NULL;
	char **plist=NULL;
	int ret = 1;
	int recno = 0;
	int cflag = 0; //Create list flag
	int rflag = 0; //Remove list flag
	int lflag = 0; //List flag
	int Dflag = 0; //Debug output
	int sflag = 0; //Set value flag
	int dflag = 0; //Delete value flag
	int gflag = 0; //Get value flag
	int nflag = 0; //Number flag
	int kflag = 0; //Key flag
	int Lflag = 0; //List flag
	int Tflag = 0; //Disable trigger flag
	int pflag = 0; //Print flag
	int Pflag = 0; //Print flag full list
	int Sflag = 0; //List Sublists
	int Zflag = 0; //List Sublist with full list path
	int Nflag = 0; //List No of sublist 
	int wflag = 0; //List No of sublist 
	int bflag = 0; //bash friendly
	int xflag = 0; //hdb-dump
	int fflag = 0; //hdb-dump-flat
	int Rflag = 0; //Set hdb root flag
	int tflag = 0; //type
	int uflag = 0; //URI flag
	int qflag = 0; //quite output flag

	strcpy(value, "");
	strcpy(key, "");
	strcpy(list, "");
	strcpy(hdbroot, "");
	strcpy(cmdbuf, "");

	while ((op = getopt(argc, argv, "S:LThqbk:c:O:u:l:R:Dws:d:p:g:r:n:Z:P:N:x:f:mt:")) != EOF){
		switch (op) {
			case 'u':
				uflag++;
				strncpy(uri, optarg, sizeof(uri));
				break;
			case 'l':
				lflag++;
				strncpy(list, optarg, sizeof(list));
				break;
			case 'q':
				qflag++;
				break;
			case 'T':
				Tflag++;
				break;
			case 'f':
				fflag++;
				strncpy(list, optarg, sizeof(list));
				break;
			case 'x':
				xflag++;
				strncpy(list, optarg, sizeof(list));
				break;
			case 't':
				tflag++;
				if(!strcmp(optarg, "str")){
					type = HDB_TYPE_STR;
				} else if (!strcmp(optarg, "int")){
					type = HDB_TYPE_INT;
				} else if (!strcmp(optarg, "lnk")){
					type = HDB_TYPE_LINK;
				} else if (!strcmp(optarg, "exe")){
					type = HDB_TYPE_EXEC;
				} else if (!strcmp(optarg, "fil")){
					type = HDB_TYPE_FILE;
				} else {
					fprintf(stderr, "ERR - Unknown type %s\n", optarg);
					usage();
				}
				break;
			case 'm':
				mflag++;
				break;
			case 'O':
				Oflag++;
				timeout = atoi(optarg);
				break;
			case 'R':
				Rflag++;
				strncpy(hdbroot, optarg, sizeof(list));
				break;
			case 'D':
				Dflag++;
				//strncpy(dev, optarg, sizeof(dev));
				break;
			case 's':
				strncpy(value, optarg, sizeof(value));
				sflag++;
				break;
			case 'k':
				strncpy(key, optarg, sizeof(key));
				kflag++;
				break;
			case 'd':
				strncpy(key, optarg, sizeof(key));
				dflag++;
				break;
			case 'L':
				Lflag++;
				break;
			case 'b':
				bflag++;
				break;
			case 'g':
				strncpy(key, optarg, sizeof(key));
				gflag++;
				break;
			case 'p':
				strncpy(list, optarg, sizeof(list));
				pflag++;
				lflag++;
				break;
			case 'P':
				strncpy(list, optarg, sizeof(list));
				Pflag++;
				lflag++;
				break;
			case 'S':
				strncpy(list, optarg, sizeof(list));
				Sflag++;
				lflag++;
				break;
			case 'Z':
				strncpy(list, optarg, sizeof(list));
				Zflag++;
				lflag++;
				break;
			case 'r':
				strncpy(list, optarg, sizeof(list));
				rflag++;
				lflag++;
				break;
			case 'w':
				wflag++;
				break;
			case 'N':
				strncpy(list, optarg, sizeof(list));
				lflag++;
				Nflag++;
				break;
			case 'n':
				strncpy(recbuf, optarg, sizeof(recbuf));
				recno = atoi(recbuf);
				nflag++;
				break;
			case 'c':
				strncpy(list, optarg, sizeof(list));
				lflag++;
				cflag++;
				break;
			case 'h':
				usage();
				break;
			default:
				usage();
				break;
		}
	}
	if((pw = getpwuid(getuid())) ){
		hdb_set_user(pw->pw_name);
	}

	strcpy(host, "127.0.0.1");
	if(uflag){
		if((p=strstr(uri, "hdb://")) && (p+=6)){
			if(
					(p = next_tag(p, user, ":")) &&
					(p = next_tag(p, pwd, "@")) &&
					(p = next_tag(p, host, ":"))) 
			{
				; //ok
				iport=atoi(p);
			}
			else {
				fprintf(stderr, "hdb://user:pwd@127.0.0.1/list/sublist\n");
				exit(1);
			}
		}
		else {
			strcpy(user, "");
			strcpy(pwd, "");
		}
	}



	hdb_net=hdb_get_access();	
#if HDB_NET_SWITCH
	found_lock=hdb_check_lock(HDB_LOCKFILE);

	//if the daemon isn't started. Pass on the light-hdb
	if(hdb_net == HDB_ACCESS_NETWORK && found_lock){
		if(Dflag){
			fprintf(stderr, "INFO - Switching to light-hdb\n");	
		}
		return execvp("lhdb", argv);
	}
	//if the daemon is running. Use it.
	if(hdb_net == HDB_ACCESS_FILE && !found_lock){
		if(Dflag){
			fprintf(stderr, "INFO - Switching to network-hdb\n");	
		}
		return execvp("nhdb", argv);
	}
#endif

	if(hdb_net){
		if(Dflag){
			fprintf(stderr, "Connecting to %s port %i\n", host, iport);
		}
		if(hdb_connect(host, iport, NULL, NULL, 0)){
			if(Dflag){
				fprintf(stderr, "ERR - Unable to connect to HDB daemon.\n");
			}
			return 1;
		}
		if(getenv("HDBROOT")){
			if(hdb_set_root(getenv("HDBROOT"))){
				if(!qflag){
					fprintf(stderr, "ERR Unable to set root\n");
				}
				hdb_disconnect();
				exit(1);
			}
		}	
	}
	if(Tflag){
		hdb_trig_disable(0,NULL, NULL);
	}

	if(Rflag){
		if(hdb_set_root(hdbroot)){
			if(!qflag){
				printf("failed to set root to %s\n", hdbroot);
			}
			cleanup();
			exit(1);
		}
	}
	if(Oflag){
		if(!lflag){
			fprintf(stderr, "Missing list. Use -l\n");
			exit(1);
		}
		if(hdb_lock(list, timeout)){
			if(!qflag){
				fprintf(stderr, "Failed to get lock on %s\n", list);
			}
			exit(1);
		}
		//fprintf(stderr, "lock succeeded\n");
		//sleep(30);
	}

	if(Lflag){
		if(hdb_net){
			ret = hdb_net_print(stdout, "LIST\n");
		}
		else {
			//NOTE! -L changed was LISTR is now LIST
			//
			/*
			snprintf(cmdbuf, sizeof(cmdbuf), 
					"cd %s && find ./ -type d | cut -c3- | sed 1d",
					hdb_get_root());
			system(cmdbuf);
			*/
			if((ret = hdb_scan_sublist("", &plist))==-1){
				return 1;
			}
			for(i=0;i<ret;i++){
				printf("%s\n", plist[i]);
			}
		}
		cleanup();
		exit(0);
	}
	if(wflag){
		ret = hdb_wipe();
		cleanup();
		return ret;
	}
	if(fflag){
		if(!strcmp(basename(list), ".")){
			if(hdb_net){
				ret = hdb_net_print(stdout, "DUMPFLAT\n");
			}
			else {
				ret = hdb_dump_flat(1, "");
			}
		}
		else { 
			if(hdb_net){
				snprintf(value, sizeof(value), "DUMPFLAT %s\n", list);
				ret = hdb_net_print(stdout, value); 
			}
			else {
				ret = hdb_dump_flat(0, list);
			}
		}

		cleanup();
		return ret;
	}
	if(xflag){
		if(!strcmp(basename(list), ".")){
			if(hdb_net){
				ret = hdb_net_print(stdout, "DUMP\n");
			}
			else {
				ret = hdb_dump(-1, list);
			}
		} else {
			if(hdb_net){
				snprintf(value, sizeof(value), "DUMP %s\n", list);
				ret = hdb_net_print(stdout, value); 
			}
			else {
				ret = hdb_dump(0, list);
			}
		}
		cleanup();
		return ret;
	}

	if(Zflag){
		while(*(list+strlen(list)-1) == '/'){
			list[strlen(list)-1] = '\0';
		}
		i = 1;
		if (Dflag) {
			fprintf(stderr, "Fetching sublists of %s\n", list);
		}
		if(hdb_net){
			snprintf(value, sizeof(value), "SUBLISTFULL %s\n", list);
			if(hdb_net_print(stdout, value)){
				if (Dflag) fprintf(stderr, "SUBLISTFULL cmd failed\n");
			}
		}
		else {
			if((ret = hdb_scan_sublist_full(list, &plist))==-1){
				return 1;
			}
			
			for(i=0;i<ret;i++){
				printf("%s\n", plist[i]);
			}

			hdb_scan_sublist_close(plist);
			/*
			while(!hdb_get_sublist_cur_full(list, HDB_NEXT, value)){
				printf("%s\n", value);
			}
			*/
		}
		cleanup();
		return 0;
	}

	if(Sflag){
		if (Dflag) {
			fprintf(stderr, "Fetching sublists of %s\n", list);
		}
		if(hdb_net){
			snprintf(value, sizeof(value), "SUBLIST %s\n", list);
			if(hdb_net_print(stdout, value)){
				if (Dflag){
					fprintf(stderr, "SUBLIST cmd failed\n");
				}
			}
		}
		else {
			if((ret = hdb_scan_sublist(list, &plist))==-1){
				return 1;
			}
			
			for(i=0;i<ret;i++){
				printf("%s\n", plist[i]);
			}

			hdb_scan_sublist_close(plist);

			/*
			while(!hdb_get_sublist_cur(list, HDB_NEXT, value)){
				printf("%s\n", value);
			}
			*/
		}
		cleanup();
		return 0;
	}

	if(!lflag && !Rflag)
	{
		fprintf(stderr, "Missing -l flag\n");
		usage();
	}

	if(sflag && !kflag){
		fprintf(stderr, "Missing key for set. Add -k switch\n");
		usage();
	}

	/*
	   if(Dflag){
	   fprintf(stderr, "list:%s\n", list);
	   }
	 */

	if(cflag){
		if (Dflag){
			fprintf(stderr, "Creating list %s\n", list);
		}
		if((ret = hdb_create_list(list)) != 0){
			/*
			if(!qflag){
				fprintf(stderr, "Failed to create list. errno %i\n", ret);
			}
			*/
		}
	}

	if(rflag){
		if (Dflag){
			fprintf(stderr, "Removing list %s\n", list);
		}
		if((ret = hdb_delete_list(list)) != 0){
			if(!qflag){
				fprintf(stderr, "Failed to remove list. errno %i\n", ret);
			}
		}
	}

	if(sflag){
		if (Dflag){
			fprintf(stderr, "Setting value %s in key %s with type %i\n", value, key, type);
		}
		//if((ret = hdb_set_val(list, key, value)) != 0){
		if((ret = hdb_set_raw(list, key, value, type)) != 0){
			if(!qflag){
				fprintf(stderr, "Failed to set value. errno %i\n", ret);
			}
		}
		else {
			hdb_sync();
		}
	}

	if(gflag){
		if (Dflag) {
			fprintf(stderr, "Getting value from list %s key %s\n", list, key);
		}

		if((ret = hdb_get_nval(list, key, sizeof(value), value)) != 0){
			if(!qflag){
				fprintf(stderr, "Failed to get value. errno %i\n", ret);
			}
		}
		else {
			if(bflag){
				printf("%s=%s\n", key, value);
			}
			else {
				printf("%s\n", value);
			}
		}
	}

	if(nflag){
		if (Dflag){
			fprintf(stderr, "Getting %ith value from list %s\n", recno, list);
		}
		ret = hdb_get_rec(list, recno, key, value);
		if(ret == 0){
			printf("%s %s\n", key, value);
		}
		else {
			if(!qflag){
				fprintf(stderr, "Could not find element number %i\n",recno);
			}
		}
	}

	if(Nflag){
		if(Dflag){
			fprintf(stderr, "Getting nr of sublists from list %s\n", list);
		}
		printf("%i\n", hdb_get_size(list));
		ret = 0;
	}

	if(dflag){
		if (Dflag) {
			fprintf(stderr, "Deleting value from list %s key %s\n", list, key);
		}
		if((ret = hdb_del_val(list, key)) != 0){
			if(!qflag){
				fprintf(stderr, "Failed to delete value. errno %i\n", ret);
			}
		}
	}

	if(hdb_net){
		if(pflag){
			snprintf(value, sizeof(value), "PRINT %s\n", list);
			ret = hdb_net_print_fmt(stdout, value, bflag ? 1 : 0);
		}
		else if(Pflag){
			snprintf(value, sizeof(value), "PRINTFULL %s\n", list);
			ret = hdb_net_print_fmt(stdout, value, bflag ? 1 : 0);
		}
	}
	else {
		if(pflag || Pflag){
			//remove trailing ///// from list
			while(*(list+strlen(list)-1) == '/'){
				list[strlen(list)-1] = '\0';
			}
			ret=0;
			if((ret=hdb_get_cur(list, HDB_FIRST, key, value))){
				cleanup();
				//pass on the error from the lib
				return ret;
			}
			do {

				//while((ret = hdb_get_rec(list, i++, key, value)) == 0){
				//printf("%i %i %s %s\n", i, ret, key, value);	
				if(pflag){
					if(!strcmp(value, "")){
						if(bflag){
							printf("%s=\n", key);
						}
						else {
							printf("%s\n", key);	
						}
					}
					else {
						if(bflag){
							printf("%s=%s\n", key, value);
						}
						else {
							printf("%s %s\n", key, value);	
						}
					}
				}
				else if (Pflag){
					if(!strcmp(value, "")){
						if(bflag){
							printf("%s_%s=\n", list, key);
						}
						else {
							printf("%s/%s\n", list, key);	
						}
					}
					else {
						if(bflag){
							printf("%s_%s='%s'\n", list, key, value);
						}
						else {
							printf("%s/%s %s\n", list, key, value);	
						}
					}
				}
			} while (!hdb_get_cur(list, HDB_NEXT, key, value));
		}
	}
	cleanup();
	return ret;
}

int hdb_dump(int level, char *list){

	char sublist[HDB_PATH_MAX];
	char key[HDB_KEY_MAX];
	char value[HDB_LIST_MAX];
	char cmd[MAXLENGTH];
	char *v=NULL;
	char *tabs = NULL;
	int type = 0;
	HDBC *hdbc=NULL;
	//this order must match HDB_TYPE_XXX.. fix this
	char meta[] = {'$', '@', '%', '!', '>'};

	if(list == NULL){
		return 0;
	} 

	if(level > 0){
		tabs = malloc(sizeof(char)*level+8);
		memset(tabs, ' ', level+8);
		tabs[level] = '\0';
	}
	else {
		tabs = malloc(2);
		tabs[0] = '\0';
	}

	if(list && strcmp(basename(list), ".")){
		printf("%s<%s>\n", tabs, basename(list));
	} 

	if(!hdb_get_cur(list, HDB_FIRST, key, value)){
		do {
			if(mflag){
				if(!(v = hdb_get_raw(list, key, &type))){
					;;//printf("%s %c%s %s\n", tabs, meta[type], key, v);
				}
				printf("%s %c%s %s\n", tabs, meta[type], key, v);
#ifdef WITH_FILE_DUMPING
				//printf("%s <data>\n", tabs);
				fflush(stdout);
				if(type==HDB_TYPE_FILE){
					sprintf(cmd, "uuencode -m %s %s", v, v);
					system(cmd);		
				}
				fflush(stdout);
				//printf("%s </data>\n", tabs);
#endif
				free(v);
			}
			else {
				printf("  %s %s %s\n", tabs, key, value);
			}
		} while (!hdb_get_cur(list, HDB_NEXT, key, value));
	}


	if((hdbc = hdb_sublist_copen(hdbc, list)) != NULL){
		while(!hdb_sublist_cget_full(hdbc, sublist, sizeof(sublist))){
			hdb_dump(++level, sublist);
			level--;
		}
		hdb_sublist_cclose(hdbc);	
	}

	
	/*
	while(!hdb_get_sublist_cur_full(list, HDB_NEXT, sublist)){
		hdb_dump(++level, sublist); //printf("%s\n", sublist);
		level--;
	}
	*/


	if(list && strcmp(basename(list), ".")){
		printf("%s</%s>\n", tabs, basename(list));
	} 

	free(tabs);
	return 0;
}

int hdb_dump_flat(int level, char *list){

	char sublist[HDB_PATH_MAX];
	char key[HDB_KEY_MAX];
	char value[HDB_VALUE_MAX];
	char *v=NULL;
	int type = 0;
	//this order must match HDB_TYPE_XXX.. fix this
	char meta[] = {'$', '@', '%', '!', '>'};

	if(!hdb_get_cur(list, HDB_FIRST, key, value)){
		do {
			if(mflag){
				if(!(v = hdb_get_raw(list, key, &type))){
					;//printf("%s %c%s %s\n", tabs, meta[type], key, v);
				}
				printf("%s/%c%s=%s\n", list, meta[type], key, v);
			}
			else {
				printf("%s/%s=%s\n", list, key, value);
			}
		} while (!hdb_get_cur(list, HDB_NEXT, key, value));
	}

	while(!hdb_get_sublist_cur_full(list, HDB_NEXT, sublist)){
		hdb_dump_flat(++level, sublist); //printf("%s\n", sublist);
		level--;
	}

	return 0;
}
//returns pointer to the next element after delim
//next element is aved in result
char * next_tag(char *buf, char *result, char *delim){

	char *p = NULL;
	//printf("found x %s\n", buf);
	if((p = strstr(buf, delim)) && !(*p++=0)){
		strncpy(result, buf, 255);
	}
	return p;
}


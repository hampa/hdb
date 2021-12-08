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
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <ctype.h>
#include <glob.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "hdb.h"

#define CMD_VERSION "$Id: hdb-import.c,v 1.1 2006/03/12 13:32:13 hampusdb Exp $"
#define WITH_FILE_DUMP 1

char *strip(char *);
char *getnexttag(char *, char *);
void list_incr(char *, int);
int list_glob(char *list, int action);
int is_number(char *s);
int import_flat(char *line, char *cur_list, char *delim, int do_merge);
int import_xml(char *line, int do_merge);

char list_tree[255][255];
int level = 0;
int file_embed = 0; //we are currently printing an embeded file


int push(const char *list);
void pop();
int get_current_list(char *list);
int set_value(char *, char *, char *);
int create_tmp_list(const char *template, char *list, int size);

void usage(){
	fprintf(stderr, 
	"hdb-import [switch] <file.hdb> (- for stdin)\n"
	"  -m              Merge will not erase list before inserting (-before list will delete it)\n"
	"  -f              Treat input as flat format (/list/sublist/key value)\n"
	"  -d <delimiter>  Delimiter when importing with -f (Default ' ')\n"
	"  -r <list>       New root. Attach to this tree\n"
	"  -t              Import as meta types (!=exec, @=link, >=file). Use before keys\n"
	"  -D              Debug info\n"
	"  -h              Help (this one)\n"
	);
	fprintf(stderr, "version %s\n", CMD_VERSION);
	
	exit(1);
}
int mflag = 0;
int Dflag = 0;
int tflag = 0;
int new_values = 0;
int deleted_values = 0;
int update_values = 0;
int same_values = 0;
int fflag = 0;
int rflag = 0;
char root[255];
char tmp_list[MAXLENGTH];

int main(int argc, char **argv){
	int i = 0;
	FILE *f = stdin;
	char line[512];
	char *stripped=NULL;
	char delimiter[255];
	char filename[255];
	char cur_list[255];
	int hdb_net=0;
#if HDB_NET_SWITCH
	int found_lock=0;
#endif

#if WITH_FILE_DUMP
	FILE *filetmp=NULL;
	char *filetmpname=NULL;
	char buf[MAXLENGTH];
#endif

	memset( filename, 0, sizeof(filename) );
	if(argc == 1){
		usage();
		exit(1);
	}
	strcpy(delimiter, " ");
	strcpy(cur_list, "");
	
	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-D")){
			Dflag++;
		}
		else if(!strcmp(argv[i], "-d")){
			strncpy(delimiter, argv[i+1], sizeof(delimiter));
			fflag++;
		}
		else if(!strcmp(argv[i], "-f")){
			fflag++;
		}
		else if(!strcmp(argv[i], "-m")){
			mflag++;
		}
		else if(!strcmp(argv[i], "-t")){
			tflag++;
		}
		else if(!strcmp(argv[i], "-r")){
			strncpy(root, argv[i+1], sizeof(root));
			rflag++;
		}
		else if(!strcmp(argv[i], "-h")){
			usage();
		}
		else{
			strncpy(filename, argv[i], sizeof(filename));
		}
	}
	if(*filename == 0){
		fprintf(stderr, "What file should I import?\n%s -h for usage\n", argv[0]);
		exit(1);
	}

	if(strcmp(filename, "-")){
		if((f = fopen(filename, "r")) == NULL){
			fprintf(stderr, "ERR Unable to open %s\n", filename);
			usage();
		}
	}
   hdb_net=hdb_get_access();
#if HDB_NET_SWITCH
        found_lock=hdb_check_lock(HDB_LOCKFILE);

        //if the daemon isn't started. Pass on the light-hdb
        if(hdb_net == HDB_ACCESS_NETWORK && found_lock){
				if(Dflag){
					printf("INFO - Switching to light-hdb\n");
				}
                return execvp("lhdb-import", argv);
        }
        //if the daemon is running. Use it.
        if(hdb_net == HDB_ACCESS_FILE && !found_lock){
				if(Dflag){
                	printf("INFO - Switching to network-hdb\n");
				}	
                return execvp("nhdb-import", argv);
        }
#endif

	if(hdb_net && hdb_connect("127.0.0.1", 12429, NULL, NULL, 0)){
        	fprintf(stderr, "ERR - Unable to connect to HDB daemon.\n");
		return 1;
	}

	//create a temporary hdb-table for storing backlog
	if(!fflag && create_tmp_list("/tmp/hdb-import", tmp_list, sizeof(tmp_list))){
		fprintf(stderr, "ERR - Unable to create hdb tmp list %s\n", tmp_list);
		return 1;
	}

	while(fgets(line,sizeof(line),f)){
#ifdef WITH_FILE_DUMP
		//found embed tag
		if(!strncmp(line, "begin-base64 ", strlen("begin-base64 "))){
			file_embed=1;
		}

		if(file_embed){
			//open
			if(filetmp==NULL){
				if((filetmpname=tempnam(NULL, "hdb-import"))==NULL){
					fprintf(stderr, "tempnam failed : %s\n", filetmpname);
					exit(1);
				}
	
				if((filetmp = fopen(filetmpname, "w"))==NULL){
					fprintf(stderr, "Unable to open tmp file\n");
					exit(1);
				}
			}

			//print
			fprintf(filetmp, line);

			//close
			if(!strcmp(line, "====\n")){
				//convert
				fclose(filetmp);
				filetmp=NULL;
				sprintf(buf, "uudecode %s", filetmpname);
				system(buf);					
				unlink(filetmpname);
				free(filetmpname);
				filetmpname=NULL;
				//turn of
				file_embed=0;
			}
			continue;
		}
#endif

		stripped = strip(line);

		//ignore commented lines
		if(*stripped == '#'){
			if(Dflag){
				printf("INFO Ignoring comment %s\n", line);
			}
			continue;
		}


		//skip empty lines
		if(*stripped == 0 ){
			continue;
		}


		//import the stripped line in flat format and continue
		if(fflag){
			import_flat(stripped, cur_list, delimiter, mflag);
		}
		//import the stripped line in xml format
		else {
			import_xml(stripped, mflag);
		}

	}
	hdb_sync();

	if(!fflag){
		hdb_delete_list(tmp_list);
	}

	if(hdb_net){
		hdb_disconnect();
	}

	printf("INFO new=%i updated=%i skipped=%i deleted=%i\n", new_values, update_values, same_values, deleted_values);

	return 0;
}


char *getnexttag(char *s, char *buf){

	char *e=NULL;
	char *p=NULL;
	char tmp[1024];

	if (!s || *s=='\0' || !buf){
		return NULL;
	}

	strcpy(tmp, s);
	p=tmp;

        if( (*p== '<' && (e=strstr(p, ">")) && (e++)) ||
            (*p=='\'' && (e=strstr(p+1, "'")) && (e++)) ||
            ((e=strstr(p, "<"))))
        {
                *e='\0';
        }

        strcpy(buf, tmp);
        return s + (p-tmp) + strlen(p);

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

int push(const char *list){
	char cur_list[255];

	strcpy(list_tree[level++], list);
	get_current_list(cur_list);
	hdb_create_list(cur_list);

	return 0;
}

void pop(){
	level--;
}

int get_current_list(char *list){
	int i = 0;
	strcpy(list, "");
	for(i = 0; i < level; i++){
		if(i != 0){
			strcat(list, "/");
		}
		strcat(list, list_tree[i]);
	}

	return 0;
}

int set_value(char *list, char *key, char *value){

	char old_value[255];
	int type = HDB_TYPE_STR;

	if(key==NULL){
		return 0;
	}
	if(mflag && *key == '-'){
		key++;
		hdb_del_val(list, key);	
		if(Dflag){
			printf("INFO deleting %s/%s\n", list, key);
		}
		deleted_values++;
		return 0;
	}
	else if(mflag && !hdb_get_nval(list, key, sizeof(old_value), old_value)){
		if(!strcmp(old_value, value)){
			if(Dflag){
				printf("INFO skipping %s/%s/%s\n", list, key, value);
			}
			same_values++;
		}
		else {
			if(Dflag){
				printf("INFO updating %s/%s %s->%s\n", list, key, old_value, value);
			}
			update_values++;
		}
	}
	else {
		if(Dflag){
			if(mflag){
				printf("INFO creating %s/{%s} '%s'\n", list, key, value);
			}
			else {
				printf("INFO creating %s/%s/%s\n", list, key, value);
			}
		}
		new_values++;
	}
	if(tflag){
		if(*key == '!'){
			type=HDB_TYPE_EXEC;
			key++;
		}
		else if (*key == '@'){
			type=HDB_TYPE_LINK;
			key++;
		}
		else if (*key == '>'){
			type=HDB_TYPE_FILE;
			key++;
#ifdef WITH_FILE_DUMP
			file_embed = 1; //we are expecting uuencoded after this key
#endif
		}
		else {
			type=HDB_TYPE_STR;
		}
	}
	
	if(hdb_set_raw(list, key, value, type)){
		fprintf(stderr, "ERR Unable to save value %s at key %s in list %s with type %i\n", value, key, list, type);
		return 1;
	}


	return 0;
}

int is_number(char *s){
	while(*s){
		if(!isdigit(*s++)){
			return 1;
		}
	}
	return 0;
}

void list_incr(char *list, int size){

	int i = 1;
	char *s = NULL;
	char *buf = NULL;

	buf = malloc(size*sizeof(char));

	strncpy(buf, list, size);
	s = rindex(buf, ':');

	if(s && *++s && !is_number(s)){
		i = atoi(s) + 1;
		s[-1]=0;
	}

	snprintf(list, size, "%s:%i", buf, i);
	free(buf);
}


int list_glob(char *list, int action){
	char rootlist[MAXLENGTH];
	char path[4096];
	char root[MAXLENGTH];
	int root_size;

	char *p=NULL;

	int i=0;
	glob_t globbuf;

	snprintf(root, sizeof(root), "%s", hdb_get_root());
	snprintf(rootlist, sizeof(rootlist), "%s%s", root, list);
	root_size = strlen(root);
	glob(rootlist, GLOB_ONLYDIR, NULL, &globbuf);
	
	for(i=0;i<globbuf.gl_pathc;i++){
		//sanity check
		//remove rootlist again
		if(!realpath(globbuf.gl_pathv[i], path)){
			fprintf(stderr, "realpath failed\n");
			return 1;
		}
		if(strlen(path) < root_size-1){
			fprintf(stderr, "globbing outside root strlen(%s) = %i root_size %i\n",
			path, strlen(path), root_size);
			return 1;
		}
		p = path;
		p += root_size;
		if(!strcmp(p, "")){
			strcpy(p, ".");
		}	
	
		switch(action){
		case HDB_DELETE_LIST:
			if(!hdb_delete_list(p)){
				printf("delete list %s succeded\n", p);	
			}
			else {
				printf("delete list %s failed\n", p);	
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

int import_flat(char *stripped, char *cur_list, char *delimiter, int do_merge){

	char *list=NULL;
	char *key=NULL;
	char *value=NULL;
	char *d;
	char *delim;

	if(!(delim = strstr(stripped, delimiter))){
		fprintf(stderr, "ERR missing delimiter '%s'.. skipping\n",delimiter);
		return 1;
	}

	if(strlen(delim+1)>255){
		fprintf(stderr, "ERR value to long.. skipping\n");
		return 1;
	}

	value = delim+1;
	*delim = '\0';
	d = strdup(stripped);
	list = dirname(d);
	key = basename(stripped);


	if(do_merge){
		set_value(list, key, value);
	}
	else {
		if(strcmp(cur_list, list)){
			hdb_delete_list(list);
			strncpy(cur_list, list, 255);
		}
		set_value(list, key, value);
	}
	free(d);
	return 0;
}

int import_xml(char *line, int do_merge){

	char *p=NULL;
	char *stripped=NULL;
	static int root_is_set=0;
	char buf[MAXLENGTHX2];
	char key[MAXLENGTH];
	char cur_list[MAXLENGTH];
	char value[MAXLENGTH];
	char inc_list[MAXLENGTH];
	char *tag;
	int i=0;

	strcpy(buf, "");

	p=line;

	//there can be several tags on one line <list>key value<list>
	while((p=getnexttag(p, buf))){

		stripped = strip(buf);	

		memset(key, 0, sizeof(key) );
		memset(value, 0, sizeof(value) );

		//is it a start tag or end tag
		if(*stripped == '<' && stripped[strlen(stripped) - 1] == '>'){

			//its an endtag.. pop
			if(stripped[1] == '/'){
				pop();
				continue;
			}


			//<key> -> key
			tag = stripped;
			tag++;
			tag[strlen(tag) - 1] = '\0';


			if(root_is_set == 0){
				if(rflag){
					if(!mflag){
						hdb_delete_list(root);
					}
					push(root);
				}
				else {
					if(!mflag){
						hdb_delete_list(tag); 
					}
					if(Dflag){
						if(!hdb_exist(tag)){
							printf("INFO Merging to root tag %s\n", tag);
						}
						else {
							printf("INFO New root tag %s\n", tag);
						}
					}
				}
				root_is_set = 1;
			}

			// Support for multiple lists with same name
			// Lists are internally saved as list:n
			if(!mflag){
				get_current_list(cur_list);
				if(strcmp(cur_list, "")){
					snprintf(inc_list, sizeof(inc_list), "%s/%s", cur_list, tag);
				}
				else {
					strcpy(inc_list, tag);
				}
				//does the list exist in our tmp database
				if(!hdb_key_exist(tmp_list, inc_list)){

					//find last number for this list. sublist:99 -> sublist:100
					do {
						list_incr(inc_list, 255);
					} while(!hdb_exist(inc_list));

					//we need to push this tag later
					tag = basename(inc_list);	
					if(Dflag){
						printf("INFO adding prefix. New list %s\n", inc_list); 
					}
				}
				else {
					//list doesn't exist erase all list:n
					strcat(inc_list, ":*");
					if(list_glob(inc_list, HDB_DELETE_LIST)){
						fprintf(stderr, "Unable to glob list %s\n", inc_list);
					}
				}
			}

			push(tag);

			if(!mflag){
				get_current_list(cur_list);
				// save the list in temp database for later checking
				if((i=hdb_set_val(tmp_list, cur_list, cur_list))){
					fprintf(stderr, "Unable to save tag '%s' to tmp_list %s errno=%i\n", 
							tag, tmp_list, i);
				}
			}
		}
		else {
			//the first element was not a root tag.. make config the root
			if(root_is_set == 0){
				if(rflag){
					if(!mflag){
						hdb_delete_list(root);
					}
					push(root);
				}
				else {
					if(!mflag){
						hdb_delete_list("config"); 
					}
					push("config");
				}
				root_is_set = 1;
			}
			get_current_list(cur_list);
			sscanf(stripped, "%s %[^\n]", key, value);
			set_value(cur_list, key, value);
		}

	}

	return 0;
}

//make a unique temporary file name.
int create_tmp_list(const char *template, char *list, int size){
	
	//since we could be using network hdb on a different machine we cant use mktemp type functions
	//TODO.. add a hdb_create_tmp_list maby?
	snprintf(list, size, "%s-%i-%i", template, getpid(), (int)time(NULL));

	if(!hdb_exist(list)){
		return 1;
	}
	
	return hdb_create_list(list);
	
}


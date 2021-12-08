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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hdb.h"
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>

int cursor = 0;
int i = 0, ii=0, iii=0;
int x = 0;
int v = 0;
int bool = 0;
char *list = "tapir";
char value[MAXLENGTH];
char hdbroot[MAXLENGTH];
char value2[MAXLENGTH];
char bigvalue[4096];
char bigvalue2[4096];
char bigkey[4096];
char bigkey2[4096];
char key[MAXLENGTH];
char key2[MAXLENGTH];
char *invalid_paths[] = {"../../", "~", ".", "/etc", "/var", "/", "/home", NULL};
char *valid_paths[] = {"/var/db/hdb", "/tmp/hdb/xxx", NULL};
char *sublists[] = {"sublist1", "sublist2", "sublist3", NULL};
char *trues[] = {"On", "ON", "true", "TRUE", "True", "yes", "Yes",  "YES", "1", "1000", NULL};
char *falses[] = {"OFF", "Off", "false", "FALSE", "False", "no", "No",  "NO", "0", NULL};
//char *values[] = {"", "1", "12", "дце+*?!@~"," 12 ", " asdasd ", "      ",NULL};
char *values[] = {"x", "1", "12", "=%+*?!@~","12", "asdasd", "yyyy",NULL};
char sublist[MAXLENGTH];
char sublist2[MAXLENGTH];
char buf[512];
char basedir[MAXLENGTH];
char *result;
char *pres; 
char *pval;
int ret =0;
void run_test(int nr);
int last_run_test=-1;

extern int db_write_lock();

void usage(){
	fprintf(stderr, "Usage: hdb-tapir [--test <no>|--random <repeats>|--root <hdbroot>]\n"); 
	exit(1);
}

void printstatus(int i){
	if(i == 0){ printf("PASS - "); }
	else { printf("FAIL - (returned=%i) ", i); ret = 1;}
}

void print_fail_status(int i){
	if(i != 0){ printf("PASS - (returned=%i) ", i); }
	else { printf("FAIL - %i", i); ret = 1;}
}


int test_hdb_exist(){
	printf("INFO - testing hdb_exist\n");

	strcpy(value, "testing_exist");
	hdb_create_list(value);

	if(hdb_exist(value)){
		printf("FAIL - hdb_exist - list %s does NOT exist\n", value);
		ret++;
	}
	else {
		printf("PASS - hdb_exist - list %s exist\n", value);
	}
	strcpy(value, "adfjdfkdkajqwee123123");
	if(!hdb_exist(value)){
		printf("FAIL - hdb_exist - list %s exist\n", value);
		ret++;
	}
	else {
		printf("PASS - hdb_exist - list %s does NOT exist\n", value);
	}

	return ret;
}

int test_hdb_size(){
	printf("INFO - testing hdb_size\n");
	hdb_delete_list("test_conf_size");
	hdb_create_list("test_conf_size");
	if((i = hdb_get_size("test_conf_size")) == 0){
		printf("PASS - hdb_size - empty list returned size %i\n", i);
	}
	else {
		printf("FAIL - hdb_size - empty list returned size %i\n", i);
		ret++;
	}

	hdb_create_list("test_conf_size/hej");
	if((i = hdb_get_size("test_conf_size")) == 1){
		printf("PASS - hdb_size - single list returned size %i\n", i);
	}
	else {
		printf("FAIL - hdb_size - single list returned size %i\n", i);
		ret++;
	}

	hdb_create_list("test_conf_size/hej2");
	if((i = hdb_get_size("test_conf_size")) == 2){
		printf("PASS - hdb_size - double list returned size %i\n", i);
	}
	else {
		printf("FAIL - hdb_size - double list returned size %i\n", i);
		ret++;
	}

	hdb_delete_list("test_conf_size/hej2");
	if((i = hdb_get_size("test_conf_size")) == 1){
		printf("PASS - hdb_size - after delete list returned size %i\n", i);
	}
	else {
		printf("FAIL - hdb_size - after delete list returned size %i\n", i);
		ret++;
	}

	if((i = hdb_get_size("bogusbogus")) == 0){
		printf("PASS - hdb_size - bogus list returned size %i\n", i);
	}
	else {
		printf("FAIL - hdb_size - bogust list returned size %i\n", i);
		ret++;
	}

	return ret;
}

int test_hdb_misc(){

	printf("INFO - testing mics key stuff\n");
	hdb_create_list("hej");
	hdb_create_list("svejs");
	hdb_create_list("apa");
	hdb_delete_list("hej");
	hdb_delete_list("apa");


	hdb_get_nval(".", "hej", sizeof(value), value);

	for(i = 0; i < 20; i++){
		sprintf(sublist, "123%i", i);
		hdb_create_list(list);
	}
	for(x = 0; x < 100; x++){
		for(i = 0; i < 20; i++){
			sprintf(sublist, "123%i", i);
			sprintf(key, "key%i", i);
			hdb_set_val(sublist, key, "hej");
		}
		for(i = 0; i < 20; i++){
			sprintf(sublist, "123%i", i);
			sprintf(key, "key%i", i);
			pval = hdb_get_val(sublist, key);
			free(pval);
		}
	}

	hdb_set_val("222", "enint", "value_222");
	hdb_set_val("333", "enint", "value_333");
	hdb_set_val("444", "enint", "value_444");
	hdb_set_val("555", "enint", "value_555");
	hdb_set_val("666", "enint", "value_666");
	hdb_set_val("777", "enint", "value_666");
	hdb_set_val("888", "enint", "value_666");
	hdb_set_val("999", "enint", "value_666");

	hdb_set_val("1000", "enint", "value_666");

	if(hdb_set_val("1111", "enint", "value_666"))
		printf("ret false again\n");
	pval = hdb_get_val("333", "enint");
	//printf("got value 3 = %s\n", pval);
	free(pval);
	if (hdb_set_val("yooo", "enint", "hejsa"))
		printf("returned false\n");


	pval = hdb_get_val("yooo", "enint");

	free(pval);	
	///////////////////////////////////////////////////////////
	printf("INFO - testing storing and getting to big values (>MAXLENGTH)\n");

	//first.. make MAX_SIZE  fill with 'A'
	memset(bigvalue, 65, 4096);	


	///////////////////////////////////////////////////////////
	printf("INFO - testing storing and getting to big values (>MAXLENGTH)\n");

	//first.. make MAX_SIZE 
	memset(bigvalue, 65, 4096);	
	bigvalue[MAXLENGTH-1] = '\0';
	i = hdb_set_val(list, "maxsize", bigvalue);
	printstatus(i);
	printf("hdb_set_val - value to maxsize strlen=%i\n", strlen(bigvalue));

	strcpy(bigvalue2, "");
	i = hdb_get_nval(list, "maxsize", MAXLENGTH, bigvalue2);
	if(strcmp(bigvalue, bigvalue2)){
		printf("FAIL set MAX_SIZEd(%i) value is not same as get MAX_SIZEd(%i)\n", strlen(bigvalue), strlen(bigvalue2));
		ret = 1;
	}
	else {
		printstatus(i);
		printf("hdb_get_nval - returned size is=%i\n", strlen(bigvalue));
	}
	

	//second.. MAX_SIZE + 1
	memset(bigvalue, 65, 4096);	
	bigvalue[MAXLENGTH+1] = '\0';
	i = hdb_set_val(list, "maxsizeplusone", bigvalue);
	print_fail_status(i);
	printf("hdb_set_val - value to maxsize + 1, size=%i\n", strlen(bigvalue));

	strcpy(bigvalue2, "");
	i = hdb_get_nval(list, "maxsizeplusone", MAXLENGTH, bigvalue2);
	print_fail_status(i);
	printf("hdb_get_nval - Getting value size + 1. It should not exist\n");


	return ret;
}

int test_hdb_get_pval(){
	printf("INFO - testing hdb_get_pval\n");
	hdb_delete_list("pval");
	printf("INFO - emtpy value %s\n", hdb_get_pval("pval", "pungen"));
	printf("INFO - emtpy list and value %s\n", hdb_get_pval("pvalxxxx", "pungen"));
	hdb_set_val("pval", "key", "value");
	printf("INFO - searching for key got = %s\n", hdb_get_pval("pval", "key"));
	if(!strcmp("", hdb_get_pval("pval", "nokey"))){
		printf("PASS - pval works with strcmp got empty\n");
	}
	else {
		printf("FAIL - pval does works with strcmp did not get empty\n");
		ret=1;
	}

	if(!strcmp("value", hdb_get_pval("pval", "key"))){
		printf("PASS - pval works with strcmp\n");
	}
	else {
		printf("FAIL - pval does not work with strcmp\n");
		ret=1;
	}
	return ret;
}

#ifndef HDB_NET
int test_hdb_cget(){

	HDBC *hdbc=NULL;
	HDBC *hdbc1=NULL;
	HDBC *hdbc2=NULL;

	char key[256];
	char key1[256];
	char key2[256];
	char value[256];
	char value1[256];
	char value2[256];

	hdbc = hdb_copen(hdbc, "cgetfinnsej");
	if(hdbc){
		printf("FAIL - hdb_copen on void list should return NULL\n");
		ret++;
	}

	hdb_set_val("cget", "key1", "value1");
	hdb_set_val("cget", "key2", "value2");
	hdb_set_val("cget", "key3", "value3");
	hdb_set_val("cget", "key4", "value4");
	hdbc = hdb_copen(hdbc, "cget");
	if(hdbc==NULL){
		printf("FAIL - hdb_copen returned NULL\n");
	}
	else {
		printf("PASS - hdb_copen returned %p\n", (void *)hdbc);
	}

	while(!(i=hdb_cget(hdbc, key, value))){
		printf("PASS - %s %s\n", key, value);
	}
	hdb_cclose(hdbc);

	hdbc1 = hdb_copen(hdbc1, "cget");
	hdbc2 = hdb_copen(hdbc2, "cget");
	hdb_cget(hdbc1, key1, value1);
	hdb_cget(hdbc2, key2, value2);
	printf("PASS - first c1 %p %s %s c2 %p %s %s \n", hdbc1, key1, value1, hdbc2, key2, value2);
	hdb_cget(hdbc1, key1, value1);
	hdb_sync();
	//printf("sync done\n");
	hdb_cget(hdbc2, key2, value2);
	printf("PASS - next  c1 %s %s c2 %s %s\n", key1, value1, key2, value2);
	hdb_cclose(hdbc1);
	hdb_cclose(hdbc2);

	return ret;
}
#endif

int test_hdb_get_cur(){	

	int result=0;

	printf("INFO - testing hdb_get_cur\n");

	hdb_delete_list("cur");
	hdb_create_list("cur");

	strcpy(key,   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	strcpy(value, "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy");
	if( hdb_get_cur("cur", HDB_FIRST, key, value) && 
			hdb_get_cur("cur", HDB_LAST, key, value) && 
			hdb_get_cur("cur", HDB_PREV, key, value) && 
			hdb_get_cur("cur", HDB_NEXT, key, value)){
		printf("PASS - hdb_get_cur on empty list returned false\n");
		if(!strcmp(key,"") && !strcmp(value, "")){
			printf("PASS - hdb_get_cur values are empty after get in empty list\n");
		}
		else {
			printf("FAIL - hdb_get_cur values are empty after get in empty list\n");
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_cur on empty list returned false\n");
		ret++;
	}

	//make sure we get correct error codes
	//HDB_KEY_NOT_FOUND, HDB_LIST_NOT_FOUND
	if((result=hdb_get_cur("cur", HDB_FIRST, key, value))==HDB_KEY_NOT_FOUND){
		printf("PASS - hdb_get_cur HDB_KEY_NOT_FOUND empty list\n");
	}
	else {
		printf("FAIL - hdb_get_cur got result %i wanted HDB_KEY_NOT_FOUND\n",result);
		ret++;
	}

	if((result=hdb_get_cur("nonexistantlist", HDB_FIRST, key, value))==HDB_LIST_NOT_FOUND){
		printf("PASS - hdb_get_cur HDB_LIST_NOT_FOUND from non-existant list\n");
	}
	else {
		printf("FAIL - hdb_get_cur got result %i wanted HDB_LIST_NOT_FOUND\n",result);
		ret++;
	}
	
	//add some values
	hdb_set_val("cur", "key1", "value1");
	hdb_set_val("cur", "key2", "value2");
	hdb_set_val("cur", "key3", "value3");
	hdb_set_val("cur", "key4", "value4");
	hdb_set_val("cur", "key5", "value5");

	if((x=hdb_get_cur("cur", HDB_FIRST, key, value))==0){
		if(!strcmp(key, "key1") && !strcmp(value, "value1")){
			printf("PASS - first value is %s %s\n", key, value);
		}
		else {
			printf("FAIL - first value is %s %s\n", key, value);
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_cur HDB_FIRST error %i\n", x );
		ret++;
	}

	if((x=hdb_get_cur("cur", HDB_NEXT, key, value))==0){
		if(!strcmp(key, "key2") && !strcmp(value, "value2")){
			printf("PASS - next value is %s %s\n", key, value);
		}
		else {
			printf("FAIL - next value is %s %s\n", key, value);
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_cur HDB_NEXT error %i\n", x );
		ret++;
	}

	if((x=hdb_get_cur("cur", HDB_NEXT, key, value))==0){
		if(!strcmp(key, "key3") && !strcmp(value, "value3")){
			printf("PASS - next value is %s %s\n", key, value);
		}
		else {
			printf("FAIL - next value is %s %s\n", key, value);
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_cur HDB_NEXT error %i\n", x );
		ret++;
	}

	if((x=hdb_get_cur("cur", HDB_PREV, key, value))==0){
		if(!strcmp(key, "key2") && !strcmp(value, "value2")){
			printf("PASS - prev value is %s %s\n", key, value);
		}
		else {
			printf("FAIL - prev value is %s %s\n", key, value);
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_cur HDB_PREV error %i\n", x );
		ret++;
	}


	if((x=hdb_get_cur("cur", HDB_LAST, key, value))==0){
		if(!strcmp(key, "key5") && !strcmp(value, "value5")){
			printf("PASS - last value is %s %s\n", key, value);
		}
		else {
			printf("FAIL - last value is %s %s\n", key, value);
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_get_rec HDB_LAST error %i\n", x);
		ret++;
	}
	return ret;
}

int test_hdb_sync(){

	printf("INFO - testing hdb_sync\n");
	hdb_delete_list("sync");
	hdb_create_list("sync");

	hdb_set_val("sync", "key", "value");
	hdb_sync();
#ifdef HDB_NET
	sprintf(buf, "nhdb -R %s -l sync -k key -s hejsvejs", hdbroot);
#else
	sprintf(buf, "lhdb -R %s -l sync -k key -s hejsvejs", hdbroot);
#endif
	system(buf);
	result = hdb_get_val("sync", "key");
	if(result == NULL){
		printf("FAIL - hdb_sync - database not updated after sync result==NULL\n");
		ret++;
	} else if(!strcmp(result, "hejsvejs")){
		printf("PASS - hdb_sync - database updated after sync\n");
	}
	else {
		printf("FAIL - hdb_sync - database not updated after sync. got %s\n", result);
		ret++;
	}

	free(result);

	//should not crash.. happend before
	hdb_create_list("hej");
	hdb_delete_list("hej");
	hdb_sync();

	return ret;
}

int test_hdb_incr(){
	printf("INFO - Testing hdb_incr\n");
	hdb_delete_list(list);
	hdb_create_list("tapir");
	for(x = 0; x<100;  x++) {
		if(hdb_incr("tapir", "intvalue")){
			printf("FAIL to incr in\n");
			break;
		}
	}
	if(x == 100){
		printf("PASS - incr worked on 100 values\n");
	}
	else {
		printf("FAIL - incr worked on 100 values\n");
		ret++;
	}
	return ret;
}

int test_hdb_log(){

#ifndef HDB_NET 
	printf("INFO - testing hdb log\n");
	if(hdb_open_log(HDB_LOG_CONSOLE, NULL)){
		printf("FAIL - hdb_open_log HDB_LOG_CONSOLE\n"); 
		ret++;
	}
	hdb_print_log("%s\n", "send log to stdout");

	hdb_close_log();

	if(hdb_open_log(HDB_LOG_FILE, "test_hdb_log.log")){
		printf("FAIL - hdb_open_log HDB_LOG_FILE\n");
		ret++;
	}
	hdb_print_log("%s\n", "hello from tapir");
	hdb_close_log();

	if(hdb_open_log(HDB_LOG_SYSLOG, NULL)){
		printf("FAIL - hdb_open_log HDB_LOG_SYSLOG\n");
		ret++;
	}
	hdb_print_log("%s\n", "hello syslog from tapir");
	hdb_close_log();


#endif
	return ret;
}

int test_hdb_delete_list(){
	printf("INFO - testing hdb_delete_list\n");

#ifndef HDB_NET 
	//no rm -rf tests
	/*
	if(hdb_set_val("alist", "akey", "avalue")){
		printf("FAIL - failed to create list\n");
	}
	else {
		system("rm -rf /var/db/hdb/alist");
		if(!hdb_exist("alist")){
			printf("FAIL - hdb_exist say alist is there\n");
		}
		if((pval=hdb_get_val("alist", "akey"))){
			printf("FAIL - got value %s that was externaly rm -rf\n", pval);
			free(pval);
		}
		else {
			printf("PASS - external hdb-light rm -rf test passed\n");
		}
	}
	*/
#endif

	i = hdb_delete_list("jagfinnsinte");
	printstatus(i);
	printf("removing not existing list\n");

	i = hdb_delete_list("");
	print_fail_status(i);
	printf("removing empty list\n");

	i = hdb_delete_list(NULL);
	print_fail_status(i);
	printf("removing NULL list\n");

	hdb_create_list("yo");
	i = hdb_delete_list("yo");
	printstatus(i);
	printf("hdb_delete_list - removing existing list\n");
	sprintf(buf, "ls %s/yo > 2&>1 /dev/null", hdbroot);
	if(!(system(buf))){
		printf("FAIL hdb_delete_list did not remove list\n");
		ret++;
	}

	return ret;
}

int test_hdb_create_list(){
	
	/////////////////////////////////////////////
	char buf[256];
	//sprintf(buf, "1/2/%i/%i",x, x % 2);	
	//hdb_create_list("a/a/x");
	//hdb_create_list("b/b/x");
	for(x=0;x<200;x++){
		sprintf(buf, "list_%i",x);	
		hdb_create_list(buf);
	}
	//hdb_create_list("a/b/x");
	//return 0;
	//enable this test to check for lsof exhaustion
	//for(x=0;x<10000;x++){
	for(x=0;x<30;x++){
		// nested calls.. must not leak
		sprintf(buf, "1/2/%i/%i",x, x % 2);	
		if(hdb_create_list(buf)){
			printf("FAIL - failed to create list %s\n", buf);
			ret++;
			break;	
		}
	}
	//hdb_close();

	printf("INFO - testing hdb_create_list\n");
	i = hdb_create_list(list);
	printstatus(i);
	printf(" - hdb_create_list tapir\n");

	i = hdb_create_list("");
	print_fail_status(i);
	printf(" - hdb_create_list empty list\n");

	i = hdb_create_list("");
	print_fail_status(i);
	printf(" - hdb_create_list NULL list\n");

	sprintf(buf, "/tmp/xyz%i", (int)time(NULL));
	i = hdb_create_list(buf);
	print_fail_status(i);
	printf(" - hdb_create_list - creating invalid list %s\n", buf);

	i = hdb_create_list(".apa");
	print_fail_status(i);
	printf(" - hdb_create_list - creating invalid list .apa\n");

	i = hdb_create_list("../../../apa.conf");
	print_fail_status(i);
	printf(" - hdb_create_list - creating invalid list ../../../apa.conf\n");

	i = hdb_create_list("123/qwe/abc");
	printstatus(i);
	printf(" - hdb_create_list - creating more sublists in one 123/qwe/abc\n");

	i = hdb_create_list("123/qwe/hej/svejs");
	printstatus(i);
	printf(" - hdb_create_list multiple create list\n");

	for (i = 0; i < 10; i++){

		sprintf(buf, "tapir/x%i", i);
		if(hdb_create_list(buf)){
			printf("FAIL - unable to create list %s\n", buf);
			ret++;
		}
		if(hdb_set_val(buf, "abc", "def")){
			printf("FAIL - unable to set_val list=%s\n", buf);
			ret++;
		}
	}
	for (i = 0; i < 10; i++){
		sprintf(buf, "tapir/x%i", i);
		if((result = hdb_get_val(buf, "abc")) != NULL){
			printf("PASS - hdb_get_val changing sublists list=%s value=%s\n",
					buf, result);
		}
		else {
			printf("FAIL - hdb_get_val changing sublists list=%s\n", buf);
			ret++;
		}
		free(result);
	}
	return ret;
}

int test_hdb_set_val(){
	printf("INFO - testing hdb_set_val\n");

	hdb_delete_list("autocreatelist");
	if(hdb_set_val("autocreatelist", "nykel", "varde")){
		printf("FAIL - hdb_set_val setting value in non existent list should create list\n");
		ret++;
	}
	else {
		if((pres= hdb_get_val("autocreatelist", "nykel")) != NULL){
			if(!strcmp(pres, "varde")){
				printf("PASS hdb_set_val (non existent list) should create list\n");
			}
			else {
				printf("FAIL hdb_get_val (non existent list) got %s wanted varde\n", pres);
				ret++;
			}
		}
		else {
			printf("FAIL hdb_get_val (non existen list) returned NULL\n");
			ret++;
		}
		free(pres);
	}

	return ret;
}

int test_hdb_set_int(){
	printf("INFO - testing saving of integers in database, hdb_set_int etc\n");

	i = hdb_set_int(list, "enint", 20);
	printstatus(i);
	printf("hdb_set_int\n");

	v = hdb_get_int(list, "enint");
	if(v == 20){
		printf("PASS - hdb_set_int with int\n");
	}
	else {
		printf("FAIL - (returned=%i) hdb_set_int\n", v);
		ret = 1;
	}

	i = hdb_get_nval(list, "enint", MAXLENGTH, value);
	if(!strcmp(value, "20")){
		printf("PASS - hdb_get_nval - getting value stored with set_int\n");
	}
	else {
		printf("FAIL (returned=%i) hdb_get_nval - getting value stored with set_int value=%s\n", i, value);
		ret = 1;
	}

	i = hdb_set_val(list, "enint", "666");
	printstatus(i);
	printf("hdb_set_val - storing int %i as string\n", i);

	v = hdb_get_int(list, "enint");
	if(v == 666){
		printf("PASS hdb_get_int - getting int value that was stored as char\n");
	}
	else{
		printf("FAIL (returned=%i) hdb_get_int - getting int value that was stored as char\n", v);
		ret = 1;
	}

	i = hdb_set_int(list, "enint", 0xFFFFFFFF);
	printstatus(i);
	printf("hdb_set_val - storing to big int\n");

	v = hdb_get_int(list, "enint");
	printstatus(i);
	printf("hdb_set_val - retreiving to big int %i\n", v);

	return ret;
}

int test_hdb_get_val(){

	printf("INFO - testing hdb_get_val and hdb_get_nval\n");

	i = 0;
	
	strcpy(value, "123456789");
	strcpy(key, "akey9999");
	hdb_set_val(list, key, value);
	if((result = hdb_get_val(list, key)) == NULL){
		printf("FAIL - hdb_get_val returned NULL\n");
		ret++;
	}
	else if(!strcmp(result, value)){
		printf("PASS - hdb_get_val basic test\n");
	}
	else {
		printf("FAIL - hdb_get_val basic test\n");
		ret++;
	}

	hdb_set_val("list/sublist", "key", "value");
	if((result = hdb_get_val("list//////sublist", "key")) == NULL){
		printf("FAIL - hdb_get_val list/////sublist return NULL\n");
		ret++;
	}
	hdb_set_val("nlist/sublist", "key", "nvalue");
	if(hdb_get_nval("nlist/////sublist", "key", sizeof(value), value)){
		printf("FAIL - hdb_get_nval nlist/////sublist return error\n");
		ret++;
	}
	else if(strcmp(value, "nvalue")){
		printf("FAIL - hdb_get_nval nlist/////sublist return %s\n", value);
		ret++;
	}

	hdb_create_list("tapir");
	while(values[i] != NULL){
		strcpy(value, "");
		if(hdb_set_val("tapir", values[i], values[i])){
			printf("FAIL hdb_set_val - setting key=\"%s\" value=\"%s\"\n",
					key, value);
			ret++;
		}
        printf("INFO - testing with value '%s'\n", values[i]);
		if((pval = hdb_get_val("tapir", values[i])) != NULL){
			if(!strcmp(values[i], pval)){
				printf("PASS hdb_get_val - wanted \"%s\" got \"%s\"\n",
						values[i], pval);
			}
			else {
				printf("FAIL hdb_get_val - wanted \"%s\" got \"%s\"\n",
						values[i], pval);
				ret++;
			}
			free(pval);
		}
		else {
			printf("FAIL hdb_get_val got NULL\n");
			ret++;
		}
		//this should kill lucky NULLs
		strcpy(value, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
		if((x = hdb_get_nval("tapir",  values[i], 256,  value)) == 0){
			if(!strcmp(values[i], value)){
				printf("PASS hdb_get_nval - wanted \"%s\" got \"%s\"\n",
						values[i], value);
			}
			else {
				printf("FAIL hdb_get_nval - wanted \"%s\" got \"%s\"\n",
						values[i], value);
				ret++;
			}
		}
		else {
			printf("FAIL - hdb_get_nval returned %i list tapir key %s\n",
					x, values[i]);

			ret++;
		}

		i++;
	}
	return ret;
}
int test_hdb_mv(){

	printf("INFO - testing hdb_mv\n");
	hdb_delete_list("mv");
	hdb_delete_list("mv_target");
	if(!hdb_set_val("mv/sub1/sub2", "key", "value")){
		if(!hdb_mv("mv/sub1/sub2", "mv_target")){
			if(!strcmp("value", hdb_get_pval("mv_target/sub2", "key"))){
				printf("PASS - get val after move ok\n");
			}
			else {
				printf("FAIL - fetching value after hdb_mv failed got %s\n",
				hdb_get_pval("mv_target/sub2", "key"));
				ret++;
			}
		}
		else {
			printf("FAIL - hdb_mv failed\n");
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_set_val failed\n");
		ret++;
	}
	//test absolut path
	if(!hdb_set_val("mv/sub3/sub4", "key", "absolute")){
		sprintf(buf, "%s/mv/xyz", hdbroot);
		if(!hdb_mv("mv/sub3/sub4", buf)){
			if(!strcmp("absolute", hdb_get_pval("mv/xyz/sub4", "key"))){
				printf("PASS - hdb_mv destination abs path\n");	
			}
			else {
				printf("FAIL - fetching value after abs path move\n");
				ret++;
			}
		}
		else {
			printf("FAIL - hdb_mv absolute destination path failed\n");
			ret++;
		}
	}
	else {
		printf("FAIL - hdb_set_val failed\n");
		ret++;
	}
	return ret;
}

int test_hdb_exec(){
	char *s =NULL;
	printf("INFO - testing hdb_exec\n");
	hdb_set_exec("exec", "key", "echo hello from exec");
	if(!strcmp(hdb_get_pval("exec", "key"), "hello from exec")){
		printf("PASS exec got '%s'\n", hdb_get_pval("exec", "key"));
	}
	else {
		s = hdb_get_val("exec", "key");
		printf("FAIL exec got %s\n", s);
		free(s);
		ret++;
	}

	return ret;
}

int test_hdb_file(){
	printf("INFO - testing hdb_file\n");
    //hm.. this test sucks
	hdb_set_file("file", "key", "/proc/sys/net/ipv4/ipfrag_time");
	if(!strcmp(hdb_get_pval("file", "key"), "30")){
		printf("PASS file got '%s'\n", hdb_get_pval("file", "key"));
	}
	else {
		printf("FAIL file got %s wanted NULL\n", hdb_get_pval("file", "key2"));
		ret++;
	}

	hdb_set_file("file", "key2", "/waht/can/i/help/you/with/");
	if(strcmp(hdb_get_pval("file", "key2"), "30")){
		printf("PASS file got '%s'\n", hdb_get_pval("file", "key2"));
	}
	else {
		printf("FAIL file got %s wanted NULL\n", hdb_get_pval("file", "key2"));
		ret++;
	}

	return ret;
}


int test_hdb_sublist_cur(){
	int outx = 0, inx = 0;
	printf("INFO _ testing hdb_sublist_cur\n");
	hdb_wipe();
	sprintf(buf, "%s/parent_cur", list);
	hdb_delete_list(buf);
	if(hdb_create_list(buf)){
		printf("FAIL - hdb_create_list() could not create tapir/parent_cur");
		ret = 1;
	}

	i = hdb_get_sublist_cur("", HDB_FIRST, sublist);
	if(i==0){
		printf("PASS - hdb_get_sublist_cur() list='' returned sublist '%s'\n", sublist);
	}
	else {
		printf("FAIL - hdb_get_sublist_cur() list='' returned sublist '%s'\n", sublist);
	}

	i = hdb_get_sublist_cur(buf, HDB_FIRST, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - HDB_FIRST on empty directory got sublist=%s retval=%i\n", sublist, i);
	i = hdb_get_sublist_cur(buf, HDB_NEXT, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - HDB_NEXT on empty directory\n");
	for(i=0;i<100;i++){
		sprintf(buf, "%s/parent_cur/subnet_%i/accesspoint_%i", list, i, i);	
		if(hdb_create_list(buf)){
			printf("FAIL - unable to create list %s\n", buf);
			ret=1;
		}
	}

	sprintf(buf, "%s/parent_cur", list);
	printf("INFO - Created 100 sublists in root=%s list=%s\n", hdb_get_root(), list);
	if(!hdb_get_sublist_cur(buf, HDB_FIRST, sublist)){
		printf("PASS - got %s returnvalue =%i\n", sublist, i); 
	}
	else {
		printf("FAIL - hdb_get_sublist_cur(%s, HDB_FIRST) got '%s'\n", buf, sublist); 
		ret=1;
	}

	sprintf(buf, "%s/parent_cur", list);	
	printf("INFO - runnign test on 100 sublist\n");
	for(i=0;i<100;i++){
		if(i==0){
			cursor=HDB_FIRST;
		}
		else {
			cursor=HDB_NEXT;
		}
		if((x=hdb_get_sublist_cur(buf, cursor, sublist))==0){
			printf("."); 
		}
		else {
			printf("FAIL - got %s retval=%i\n", sublist, x); 
			ret=1;
			break;
		}
	}

	printf("\n");
	sprintf(buf, "%s/parent_cur", list);
	if(hdb_get_sublist_cur(buf, HDB_NEXT, sublist)){
		printf("PASS - DB_GETNEXT returned no more sublist\n"); 
	}
	else {
		printf("FAIL - should be no more sublist got %s\n", sublist); 
		ret=1;
	}


	sprintf(buf, "%s/parent_cur", list);
	hdb_delete_list(buf);
	printf("INFO - hdb_sublist_cur() testing switching of sublist\n");
	for(i=0;i<8;i++){
		for(ii=0;ii<4;ii++){
			sprintf(buf, "%s/parent_cur/subnet_%i/accesspoint_%i", list, i, ii);	
			//printf("INFO creating sublist %s\n", buf);
			if(hdb_create_list(buf)){
				printf("FAIL - unable to create list %s\n", buf);
				ret=1;
			}
		}
	}
	sprintf(buf, "%s/parent_cur", list);	
	if(!hdb_get_sublist_cur_full(buf, HDB_FIRST, sublist)){
		if(!hdb_get_sublist_cur_full(sublist, HDB_FIRST, sublist2)){
			printf("PASS - HDB_FIRST on sublist %s returned %s\n", sublist, sublist2);	
		}
		else {
			printf("FAIL - got sublist %s\n", sublist2);
			ret=1;
		}
		while(!hdb_get_sublist_cur_full(sublist, HDB_NEXT, sublist2)){
			printf("PASS - HDB_NEXT on sublist %s returned %s\n", sublist, sublist2);	
		}
	}
	else {
		printf("FAIL - hdb_get_sublist_cur first list of %s failed\n", buf);
		ret=1;
	}
	printf("INFO - Testing dumping with hdb_get_sublist_cur. Dir cache test\n");

	// First create two lists with different no of sublists
	for(i=0; i<2; i++){
		sprintf(buf, "sublist_cur_1/outer_loop_%i", i);	
		if(hdb_create_list(buf)){
			printf("FAIL - unable to create list %s\n", buf);
			ret=1;
		}
		if(i>1) {
			continue;
		}	
		sprintf(buf, "sublist_cur_2/inner_loop_%i",  i);	
		if(hdb_create_list(buf)){
			printf("FAIL - unable to create list %s\n", buf);
			ret=1;
		}
	}
	// closing the first list cache should not close all list caches 
	//print_dirlist_cache(&hdb_default);
	while(!hdb_get_sublist_cur_full("sublist_cur_1", HDB_NEXT , sublist)){
		//printf("got %s\n", sublist);
		//print_dirlist_cache(&hdb_default);
		outx++;
		while(!hdb_get_sublist_cur_full("sublist_cur_2", HDB_NEXT , sublist)){
			//printf("got %s\n", sublist);
			// print_dirlist_cache(&hdb_default);
			inx++;
		}
		//print_dirlist_cache(&hdb_default);
	}
	//inx == outx * no inner loops (2)
	if(outx != 2 || inx != 4){
		printf("FAIL - hdb_get_sublist_cur nested counted wrong outer loop %i times inner loop %i\n", outx, inx);
		ret=1;
	}
	else {
		printf("PASS - hdb_get_sublist_cur nested counting\n");

	}
	//print_dirlist_cache(&hdb_default);
	return ret;
}

int test_hdb_stat(){
	HDBS hstat;
	printf("INFO - testing hdb_stat\n");
	
	hdb_set_val("list", "key", "value");

	if((ret=hdb_stat("list", "key", &hstat)) !=0 ){
		printf("FAIL - hdb_stat failed\n");	
	} else {
		printf("PASS - hdb_stat returned %li %li\n", hstat.mtime, hstat.atime); 
	}
	return ret;
}
 
int test_hdb_get_sublist(){
	printf("INFO - testing getting of sublists\n");

	sprintf(buf, "%s/parent", list);
	hdb_delete_list(buf);
	if(hdb_create_list(buf)){
		printf("FAIL hdb_create_list() could not create tapir/parent");
		ret = 1;
	}
	i = hdb_get_sublist(buf, 1, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - retrieving sublist that does not exist %s retval=%i\n", sublist, i);

	i = hdb_get_sublist(buf, 0, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - retrieving sublist that does not exist\n");

	i = hdb_get_sublist(buf, 999, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - retrieving sublist that does not exist\n");


	i = 0;
	while(sublists[i] != NULL){
		sprintf(buf, "%s/%s/%s", list, "parent", sublists[i]);
		if(hdb_create_list(buf)){
			printf("FAIL hdb_create_list() could not create %s\n",
					buf);
			ret = 1;
		}	
		else {
			printf("PASS hdb_create_list() Created sublist %s\n", buf);
		}
		i++;
	}

	strcpy(sublist, "");
	sprintf(buf, "%s/parent", list);
	x = 0;
	i = hdb_get_sublist(buf, 1, sublist);
	if(!strcmp(sublist, sublists[0]) && i==0){
		printf("PASS - hdb_get_sublist returned correct value\n");
	}
	else {
		printf("FAIL - hdb_get_sublist returned wrong return(%i)value %s\n"
				, i, sublist);
		ret++;
	}

	i = hdb_get_sublist(buf, 2, sublist);
	if(!strcmp(sublist, sublists[1]) && i==0){
		printf("PASS - hdb_get_sublist returned correct value\n");
	}
	else {
		printf("FAIL - hdb_get_sublist returned wrong return(%i)value %s\n"
				, i, sublist);
		ret++;
	}

	i = hdb_get_sublist(buf, 3, sublist);
	if(!strcmp(sublist, sublists[2]) && i==0){
		printf("PASS - hdb_get_sublist returned correct value\n");
	}
	else {
		printf("FAIL - hdb_get_sublist returned wrong return(%i)value %s\n"
				, i, sublist);
		ret++;
	}

	i = hdb_get_sublist_full(buf, 3, sublist);
	if(strcmp("tapir/parent/sublist3", sublist) && i != 0){
		printf("FAIL hdb_get_sublist_full() - FAIL returned %i %s\n",
				i, sublist);
		ret++;
	}
	else {
		printf("PASS hdb_get_sublist_full() sublist = %s\n", sublist);
	}

	i = hdb_get_sublist(buf, 4, sublist);
	print_fail_status(i);
	printf("hdb_get_sublist - more lists than available (%i)\n",i);

	return ret;
}

int test_hdb_get_bool(){

	printf("INFO - testing hdb_get_bool\n");
	i = 0;
	bool = 99999;
	while(trues[i] != NULL){
		if((x = hdb_set_val(list, "bool", trues[i])) == 0){
			printf("PASS - saving boolean %s\n", trues[i]);
		}
		else {
			printf("FAIL - could not save boolean value %s\n", trues[i]);
			ret++;
		}
		bool = hdb_get_bool(list, "bool");
		if(bool != 0){
			printf("PASS - hdb_get_bool %s is %i\n", trues[i], bool);
		}
		else {
			printf("FAIL - hdb_get_bool %s is %i\n", trues[i], bool);
			ret++;
		}
		i++;
	}

	i = 0;
	bool = 99999;
	while(falses[i] != NULL){
		if((x = hdb_set_val(list, "bool", falses[i])) == 0){
			printf("PASS - saving boolean %s\n", falses[i]);
		}
		else {
			printf("FAIL - could not save boolean value %s\n", falses[i]);
			ret++;
		}
		bool = hdb_get_bool(list, "bool");
		if(bool == 0){
			printf("PASS - hdb_get_bool %s is %i\n", falses[i], bool);
		}
		else {
			printf("FAIL - hdb_get_bool %s is %i\n", falses[i], bool);
			ret++;
		}
		i++;
	}
	hdb_set_int(list, "bool_false", 0);
	bool = hdb_get_bool(list, "bool_false");
	if(bool == 0){
		printf("PASS - hdb_get_bool saved (0) as int bool = %i\n", bool);
	}
	else {
		printf("FAIL - hdb_get_bool saved (0) as int bool = %i\n", bool);
		ret++;
	}

	hdb_set_int(list, "bool_true", 1);
	bool = hdb_get_bool(list, "bool_true");
	if(bool == 1){
		printf("PASS - hdb_get_bool saved (1) as int bool = %i\n", bool);
	}
	else {
		printf("FAIL - hdb_get_bool saved (1) as int bool = %i\n", bool);
		ret++;
	}

	//bool does not exist
	bool = hdb_get_bool(list, "hejsansvejsan");
	if(bool == 0){
		printf("PASS hdb_get_bool not exist bool = %i\n", bool);
	}
	else {
		printf("FAIL hdb_get_bool not exist bool = %i\n", bool);
		ret++;
	}

	return ret;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
int test_hdb_get_rec(){
	printf("INFO - testing hdb_get_rec\n");
	hdb_delete_list("rectest");
	hdb_create_list("rectest");

	//make sure no true values
	for(i = 0; i < 10; i++){
		if(!hdb_get_rec("rectest", i+1, key2, value2)){
			printf("FAIL hdb_get_rec returned true for a non existent value %i %s %s\n",
					i+1,key2, value2);
			ret++;
		}
	}

	for(i = 0; i < 10; i++){
		sprintf(key, "key%i", i);
		sprintf(value, "value%i", i);
		//printf("INTO - saveing %i key %s value %s\n", i, key, value);
		hdb_set_val("rectest", key, value);	
	}
	for(i = 0; i < 10; i++){
		sprintf(key, "key%i", i);
		sprintf(value, "value%i", i);
		strcpy(value2, "");
		strcpy(key2, "");
		if((x = hdb_get_rec("rectest", i+1, key2, value2)) == 0){
			if(!strcmp(value2, value) && !strcmp(key2, key)){
				printf("PASS - hdb_get_rec key %s value %s\n", key2, value2);
			}
			else {
				printf("FAIL hdb_get_rec %i got key=%s value=%s expected key=%s value=%s\n",
						i+1, key2, value2, key, value);
				ret++;
			}
		}
		else {
			printf("FAIL - hdb_get_rec returned %i\n", x);
			ret++;
		}

    }

    return ret;
}

int test_hdb_root(){

	char buf[255];

	int i = 0;
        printf("INFO - testing hdb_root\n");	
	hdb_set_root("/tmp/hdb/");
	for(i=0; i< 10; i++){
		sprintf(buf, "/tmp/hdb/testroot%i", i);
		sprintf(value, "value%i", i);
		if(hdb_set_root(buf)){
			printf("FAIL - failed to set root %s\n", buf);
		}
		if(hdb_set_val("list/sublist/", "key", value)){
			printf("FAIL - failed to set value to new root %s\n", buf);
		}
		hdb_set_root("/tmp/hdb/");
		if(hdb_set_root(buf)){
			printf("FAIL - failed to set root %s\n", buf);
		}
		if(strcmp(hdb_get_pval("list/sublist", "key"), value)){
			printf("FAIL - got value %s\n", hdb_get_pval("list/sublist", "key"));
		}
	}
	//print_cache();
	//hdb_set_root("/tmp/hdb/");
	//hdb_wipe();
	//print_cache();
	//hdb_set_val("list/tjoho", "KeYYY", "value");
	//print_cache();


/*
    //security checks
    setenv("HDBROOT", "/tmp/hdb", 1);
    set_hdb_root();
    if(!strcmp(basedir, "/tmp/hdb")){
        printf("PASS - HDBROOT set to /tmp/ - allowed basedir=(%s)\n", basedir);
    }
    else {
        printf("FAIL - HDBROOT not affecting basedir\n");
        ret = 1; 
    }
    //trailing / test
    setenv("HDBROOT", "/h/hampuss/hdb", 1);
    set_hdb_root();
    if(!strcmp(basedir, "/h/hampuss/hdb")){
        printf("PASS - HDBROOT testing missing trailing /h/hampuss/hdb (%s)\n", basedir);
    }
    else {
        printf("FAIL - HDBROOT missing trailing / in %s\n", basedir);
        ret = 1; 
    }

    system("rm -rf /tmp/dbtesthdb123123");
    setenv("HDBROOT", "/tmp/dbtesthdb123123/", 1);
    if(set_hdb_root()){
        printf("FAIL - set_hdb_root could not set dir /tmp/dbtesthdb123123/\n");
        ret++;
    }
    else {
        if(system("ls /tmp/dbtesthdb123123")){
            printf("FAIL set_hdb_root did not create dir /tmp/dbtesthdb123123\n");
            ret++;
        }
        else {
            printf("PASS set_hdb_root created directory /tmp/dbtesthdb123123/\n");
        }
    }
*/
    //setenv("HDBROOT", "/var/db/hdb", 0);
    printf("INFO - Setting root\n");
    hdb_set_root("/var/db/hdb/before");
    printf("INFO - saving list key value\n");
    hdb_set_val("list", "key", "value");
    printf("INFO - Setting new root\n");
    hdb_set_root("/var/db/hdb/after");
    printf("INFO - fetching old list from new root\n");
    printf("INFO - got %s\n", hdb_get_pval("list", "key"));

    hdb_set_root("/var/db/hdb");
    printf("INFO - hdb_get_root %s\n", hdb_get_root());
    i=0;
    while(valid_paths[i] != NULL){
        if((x = hdb_set_root(valid_paths[i])) == 0){
            printf("PASS - HDBROOT trying to set valid root %s got %s retval=%i\n", 
                    valid_paths[i], hdb_get_root(), x);
        }
        else {
            printf("FAIL - HDBROOT trying to set valid root %s got %s retval=%i\n", 
                    valid_paths[i], hdb_get_root(), x);
            ret++;
        }
        i++;
    }

#ifdef HDB_NET
    hdb_disconnect();
    if((i=hdb_connect("127.0.0.1", HDB_PORT, NULL, NULL, 0))){
	printf("FAIL - failed to connect to localhost. errno %i. critical exiting\n",i);
	exit(1);
	ret++;
    }
    if(strcmp(HDBROOT, hdb_get_root())){
	printf("FAIL - hdb root is wrong when first login\n");
	ret++;
    }
    else {
	printf("PASS - hdb root is correct when first login\n");
    }

    printf("INFO - Login logout 100 times and check for correct root\n");
    for(x=0;x<100; x++){
            hdb_disconnect();
            if((i=hdb_connect("127.0.0.1", HDB_PORT, NULL, NULL, 0))){
                    printf("FAIL - failed to connect to localhost. errno %i\n",i);
                    ret++;      
            }   
            if(strcmp(HDBROOT, hdb_get_root())){
                    printf("FAIL - hdb root is wrong when first login\n");
                    ret++;
                        break;
            }           
            hdb_set_root("/var/db/hdb/xxxxxxxxx");
     }          
	
#endif
  
    i = 0;
    while(invalid_paths[i] != NULL){
        if((x = hdb_set_root(invalid_paths[i])) != 0){
            printf("PASS - HDBROOT trying to set invalid root %s got %s retval=%i\n", 
                    invalid_paths[i], hdb_get_root(), x);
        }
        else {
            printf("FAIL - HDBROOT trying to set invalid root %s got %s retval=%i\n", 
                    invalid_paths[i], hdb_get_root(), x);
            ret++;

        }
        i++;
    }
	//reset hdbroot
    	if(hdb_set_root(hdbroot)){
		printf("FAIL - failed to reset hdbroot %s\n", hdbroot);
		ret++;
	}

    return ret;
}

int test_hdb_dump_regex(){
	char buf[256];
	sprintf(buf, "list/sublist%i", time(NULL));
	hdb_set_val(buf, "key1", "value1");
	hdb_set_val(buf, "key2", "value2");
	hdb_set_val(buf, "key3", "value3");
	hdb_dump_regex("list/sublis*", ".*", ".*");
	return 0;
}

int test_hdb_net_send(){

#ifdef HDB_NET
        char *v = NULL;
	char buf[4096];
	char *pvalue = NULL;
	int errcode = 0;
    	char *crap[] = {"asdasd asdlfkj asdf asdf asdf asdf asdf asdf asdf asdf asdf asdf asdf\n", 
        "\"\"\"\"\"\"\"\n", 
        "                                                    x    \n", 
        "getx asdflkjasdfjlk gsdfalkjasfdlkjasdfjlk asfdlkjasfdkjlasdflkjsfad\n",
        "setx asdflkjasdfjlk gsdfalkjasfdlkjasdfjlk asfdlkjasfdkjlasdflkjsfad\n",
        NULL};
    	char *escape[][2] = {
				{"set list k  'value 123'\n", "value 123"},
				{"set list k  a \n", "a"},
				{"set list k  b \n", "b"},
				{"set list k   c\n", "c"},
				{" set  list   k    e  \n", "e"},
				{" set  list   k    'f  '\n", "f  "},
				{" set  list   k    'g  '\n", "g  "},
				{" set  list   k    'h\"'\n", "h\""},
				{" set  list   \"k\"    \"iii\"\n", "iii"},
				{" set  list   \"k\"    \"i'i'i\"\n", "i'i'i"},
				{NULL, NULL}
			};
				
	
	i=0;
	
	printf("INFO - testing escaping of input\n");
	//theese should be OK
	while(escape[i][0]){
		//printf("INFO - hdb_net_set %s\n", escape[i][0]);
		if((errcode= hdb_net_set(escape[i][0]))){
			printf("FAIL - hdb_net_set %s returned %i\n", escape[i][0], errcode);
		}
		if(strcmp(escape[i][1], hdb_get_pval("list", "k"))){
			printf("FAIL - hdb_net_set #%s# wanted #%s# got #%s#\n", 
			escape[i][0],
			escape[i][1],
			hdb_get_pval("list", "k"));
			ret++;
		}
		i++;
	}

	//theese should fail
	if(!hdb_net_set("set 2 3 4 5 6 7 8 9\n")){
		printf("FAIL - hdb_net_set 8 elements should only be aloud\n");	
	}
    memset(bigvalue, 65, 4096);	

	//MAXLENGTH is one to many should fail
	bigvalue[MAXLENGTH] = '\0';
	sprintf(buf, "set list key %s\n", bigvalue);
	if(!hdb_net_set(buf)){
		printf("FAIL - hdb_net_set list > MAXLENGTH\n");
	}

	//MAXLENGTH-1 is Ok should pass 
	bigvalue[MAXLENGTH-1] = '\0';
	sprintf(buf, "set list key %s\n", bigvalue);
	if(hdb_net_set(buf)){
		printf("FAIL - hdb_net_set strlen(value) == MAXLENGTH\n");
	}
	
    memset(bigvalue, 65, 4096);	

	printf("INFO - hdb_net_send bigvalue should fail\n");
	if(!hdb_net_set(bigvalue)){
		printf("FAIL - hdb_net_send bigvalue\n");
		ret++;
	}
	i=0;

	printf("INFO - hdb_net_send sending crap commands\n");
        while(crap[i] != NULL){
            printf("INFO - hdb_net_send sending cmd '%s", crap[i]);
            if(!hdb_net_set(crap[i])){
                printf("FAIL - hdb_net_send sending cmd error\n");
                ret++;
            }
            else {
                printf("PASS - hdb_net_send got '%s'\n", v);
                free(v);
            }
            i++;
        }
	printf("INFO - hdb_net_set running fastcmd tests\n"); 
	for(i=0;i<100;i++){
	//for(i=0;i<50;i++){
	//for(i=0;i<45;i++){
	//for(i=0;i<45;i++){
	//for(i=44;i<45;i++){ //ok
	//for(i=44;i<46;i++){ //fail
	//for(i=45;i<46;i++){ //fail
		
		//dont wipe or set new root
		if(i==HDB_ROOT || i==HDB_WIPE){
			continue;
		}
		strcpy(key, "");
		for(x=0;x<10;x++){
			sprintf(value, "%i %s\n", i, key);	
			//printf("INFO - running hdb_net_set cmd=%s",value);
			hdb_net_set(value);

			if((errcode=hdb_set_val("fastcmd", "key", "value"))){
				ret++;
				printf("FAIL - hdb_net_set failed after fast cmd '%s' errcode %i\n", value, errcode);
				hdb_net_set("12429\n"); //bail out
				return 1;
			}
			else {
				if((result = hdb_get_val("fastcmd", "key")) == NULL){
					printf("FAIL - fast cmd is NULL\n");
				}
				pvalue = hdb_get_pval("fastcmd", "key");
				if(strcmp("value", pvalue)){
					printf("FAIL - hdb_net_set fastcmd test failed cmd '%s' FAIL got value '%s' wanted 'value' get_val returned %s\n", 
					value, pvalue, result);
					hdb_net_set("12429\n"); //bail out
					ret++;	
					return 1;
				}
				else {
					; //printf("PASS - hdb_net_set fastcmd passed\n");
				}
				free(result);
				result=NULL;
				strcat(key, " key");
			}
		}
	}
#endif 
        return ret;
}

void test_hdb_cut(){
	int i=0;
	char listbuf[MAXLENGTH];
	/*
	char lbuf[256];
	char kbuf[256];
	char vbuf[256];
	*/

	struct listkeyvalue {
		char *lkv; //a parsable list/key=value
		char *list;   //the value we want
		char *key;   //the key we want
		char *value;   //the value we want
	};

	static struct listkeyvalue lkv[]={ 
                    //TODO//{"list/sublist/key \"1 + 1 = 2\"","list/sublist/","key","\"1 + 1 = 2\""},
		    {"list/sublist/key=value","list/sublist/","key","value"},
		    {"list/sublist/key value","list/sublist/","key","value"},
                    {"list/sublist/key","list/sublist/","key",""},
                    {"list/sublist/key=\"1 + 1 = 2\"","list/sublist/","key","\"1 + 1 = 2\""},
                    {"list/","list/","",""},
                    {NULL,NULL,NULL,NULL}};

	do {
		//if(hdb_cut(lkv[i].lkv, MAXLENGTH, lbuf, kbuf, vbuf)){
		if(hdb_cut(lkv[i].lkv, MAXLENGTH, listbuf, key, value)){
			ret++;
			//printf("FAIL - %s -> '%s' '%s' '%s'\n", lkv[i].lkv, lbuf, kbuf, vbuf);
			printf("FAIL - hdb_cut%s -> '%s' '%s' '%s'\n", lkv[i].lkv, listbuf, key, value);
		}
		else {
			//printf("PASS - %s -> '%s' '%s' '%s'\n", lkv[i].lkv, lbuf, kbuf, vbuf);
			if(!strcmp(lkv[i].list, listbuf) && !strcmp(lkv[i].key, key) && !strcmp(lkv[i].value, value)){
				printf("PASS - hdb_cut %s -> '%s' '%s' '%s'\n", lkv[i].lkv, listbuf, key, value);
			}
			else {
				printf("FAIL - hdb_cut %s -> '%s' '%s' '%s'\n", lkv[i].lkv, listbuf, key, value);
				ret++;
			}
		}
	} while(lkv[++i].lkv);
}

void test_hdb_lock(){

	//hampa.. current dev
	printf("testing write lock\n");


    	hdb_set_val("lock", "hej", "svejs");
    	if(hdb_lock("lock", 0)){
        	printf("FAIL - hdb_lock failed\n");
        	ret++;
    	}
    	else {
        	printf("PASS - hdb_lock passed\n");
    	}

    if(hdb_unlock("lock")){
        printf("FAIL - hdb_unlock failed\n");
        ret++;
    }
    else {
        printf("PASS - hdb_lock passed\n");
    }
    if(hdb_lock("lock", 0)){
        printf("FAIL - hdb_lock failed\n");
        ret++;
    }
    else {
        printf("PASS - hdb_lock passed\n");
    }

    printf("INFO - waitng five seconds for lock.. should fail\n");


}

void test_hdb_abspath(){
	int i;

	char buf[255];

	//mixing between abspath and realpath.
	for(i=0;i<10;i++){
		/*
		sprintf(buf, "abspath/abspath%i", i);
		if(hdb_create_list(buf)){
			ret++;
			printf("FAIL - test_hdb_abspath() - unable to create list %s\n", buf);
		}
		*/
		sprintf(buf, "/tmp/hdbtapir/abspath_%i_%i", getpid(), i);
		//if(hdb_create_list(buf)){
		if(hdb_set_val(buf, "list", "key")){
			ret++;
			printf("FAIL - test_hdb_abspath() - unable to create list %s\n", buf);
		}
	}
}

void test_hdb_scan_sublist(){

	int i = 0;
	char **scanlist;
	
	//create in disorder
	hdb_create_list("scania/list5");
	hdb_create_list("scania/list1");
	hdb_create_list("scania/list4");
	hdb_create_list("scania/list3");
	hdb_create_list("scania/list2");

	//i = _scan_sublist("scania", &scanlist, 0, 0);
	i = hdb_scan_sublist("scania", &scanlist);

	if(i!=5){
		printf("FAIL - test_hdb_scan_sublist() returend %i numlists\n", i);
		ret++;
	}
	else {
		printf("PASS - test_hdb_scan_sublist() correct number of sublists (%i)\n", i);
	} 
	if(scanlist[5] != NULL){
		printf("FAIL - test_hdb_scan_sublist() last element not NULL %s\n",scanlist[5]);
		ret++;
	}

	if(!strcmp(scanlist[4], "list2")){
		printf("PASS - test_hdb_scan_sublist() last element highest inode\n");	
	}
	else {
		printf("FAIL - test_hdb_scan_sublist() last element wrong order \n");	
	}
	/*
	for(x=0;x<i;x++){
		printf("scanlist '%s'\n", scanlist[x]);
	}
	*/
	hdb_scan_sublist_close(scanlist);

	hdb_scan_sublist_full("scania", &scanlist);

	if(!strcmp(scanlist[3], "scania/list3")){
		printf("PASS - test_hdb_scan_sublist() full - last element highest inode\n");	
	}
	else {
		printf("FAIL - test_hdb_scan_sublist() full - last element wrong order \n");	
	}
	/*
	for(x=0;x<i;x++){
		printf("scanlist full '%s'\n", scanlist[x]);
	}
	*/
	
	
}

void test_hdb_set_tortyr(){
	int err=0;
	int i = 0;
	char buf[255];
	printf("INFO - setting 10000 values\n");
	for(i = 0; i < 500; i++){
		sprintf(buf, "tjo%i/%i/%i/site:Tellas%i", i%2, i%2, i, i);
		//printf("%s\n", buf);
		if(!(err=hdb_set_val(buf, "key", "value asdf asdf asdf adsf asdf"))){
			; //fprintf(stderr, ".");
		}
		else {
			ret++;
			printf("FAIL - hdb_set_tortyr() - unable to set value errcode %i buf=%s\n", err, buf);
			hdb_net_set("12429"); //assert message
			exit(1);
		}
	}
	//printf("\n");
	printf("INFO - DONE\n");
	return;
}	

#define TESTS 30.0f 
void random_tapir(int repeats){

	int j=0;
	srand(time(NULL));
	while(repeats--){
		j=(int) (TESTS*rand()/(RAND_MAX));	
		printf("INFO - running test nr %i\n", j);
		ret=0;
		run_test(j);
		if(ret){
			printf("FAIL - test nr %i failed\n", j);
		}
		else {
			printf("PASS - test nr %i passed\n", j);
		}
	}
	printf("INFO - all random tapir tests run\n");
}
void run_test(int nr){
		
	switch(nr){
	case 0: test_hdb_set_tortyr(); break;
	case 1: test_hdb_mv(); break;
	case 2: test_hdb_root(); break;
	case 3: test_hdb_lock(); break;
	case 4: test_hdb_get_val(); break;
	case 5: test_hdb_exist(); break;
	case 6: test_hdb_size(); break;
	case 7: test_hdb_get_pval(); break;
	case 9: test_hdb_incr(); break;
	case 10: test_hdb_delete_list(); break;
	case 11: test_hdb_create_list();break;
	case 12: test_hdb_set_val();break;
	case 13: test_hdb_set_int(); break;
	case 15: test_hdb_exec(); break;
	case 16: test_hdb_file(); break;
	case 18: test_hdb_get_sublist(); break;
	case 19: test_hdb_get_rec();break;
	case 20: test_hdb_get_bool();break;
	case 17: test_hdb_sublist_cur(); break;
#ifndef HDB_NET
	case 14: test_hdb_scan_sublist(); break;
	case 21: test_hdb_sync(); break;
	// case 22: test_dirlist_cache(); break;
#ifndef HDBD
	case 8: test_hdb_get_cur();break;
#endif
	case 28: test_hdb_cget(); break;
#else
	case 23: test_hdb_misc(); break;
	case 24: test_hdb_net_send(); break;
#endif
	case 25: test_hdb_log(); break;
	case 26: test_hdb_abspath(); break;
	case 29: test_hdb_stat(); break;
	case 30: test_hdb_dump_regex(); break;
	default: 
		printf("INFO - No such test %i\n", nr);
		break;
	}

	if(ret){
		printf("FAIL - test %i failed. Last test was %i\n", nr, last_run_test);
		hdb_close();
		exit(1);
	}
	//verify that root hasn't been changed
	if(strcmp(hdb_get_root(), hdbroot)){
		printf("FAIL - test %i failed. modified root %s to %s\n", nr, hdbroot, hdb_get_root());
		hdb_close();
		exit(1);
	}
	last_run_test=nr;

}

void tapir(){
	test_hdb_cut();
    	test_hdb_lock();
	test_hdb_log();
	test_hdb_root();
	test_hdb_set_tortyr();
	test_hdb_abspath();
	//test_hdb_mv();
	test_hdb_root();
	test_hdb_get_val();
	test_hdb_exist();
	test_hdb_size();
	test_hdb_get_pval();
	test_hdb_incr();
	test_hdb_delete_list();
	test_hdb_create_list();
	test_hdb_set_val();
	test_hdb_set_int();
	//test_hdb_mv();
	test_hdb_exec();
	test_hdb_file();
	test_hdb_get_sublist();
	test_hdb_get_rec();
    	test_hdb_get_bool();
	test_hdb_sublist_cur();
#ifndef HDB_NET
	test_hdb_sync();
	test_hdb_cget();
#ifndef HDBD
	test_hdb_get_cur();	
#endif
    	test_hdb_net_send();
	test_hdb_misc();
#endif
}

int main(int argc,  char **argv){

	int rflag=0;
	int tflag=-1; //we have a zero test
	int repeats=1;
	
	strcpy(hdbroot, HDBROOT);

	for(i=1;i<argc-1;i++){
		if(!strcmp(argv[i], "--test")){
			tflag = atoi(argv[++i]);
		}
		else if(!strcmp(argv[i], "--repeats")){
		 	repeats = atoi(argv[++i]);	
		}
		else if(!strcmp(argv[i], "--random")){
			rflag = atoi(argv[++i]);
		}		
		else if(!strcmp(argv[i], "--root")){
			strcpy(hdbroot, argv[++i]);
		}		
		else {
			usage();
		}
	}
	 
#ifdef HDB_NET
	printf("INFO - connecting to localhost\n");
	if(hdb_connect("127.0.0.1", HDB_PORT , NULL, NULL, 0)){
		printf("FAIL - failed to connect to localhost\n");
		return 1;
	}
	printf("PASS - Connected\n");
#endif

	printf("INFO - Using hdb root %s\n", hdbroot);
	if(hdb_set_root(hdbroot)){ //set root for this session
		printf("FAIL - Unable to set --root %s\n", hdbroot);
		hdb_close();
#ifdef HDB_NET
		hdb_disconnect();
#endif
		exit(1);
	}
	hdb_wipe();

	if(tflag!=-1){
		printf("INFO - Running test no %i %i times\n", tflag, repeats);
		while(repeats--!=0){
			run_test(tflag);
		}
	}
	else if (rflag>0){
		printf("INFO - Running random tests %i times\n", rflag);
		random_tapir(rflag);
	}
	else {
		tapir();
	}	

	if(ret == 0){
		printf("PASS - All tests succeded\n");
	}
	else {
		printf("FAIL - One or more tests failed\n");
	}

	hdb_sync();
	hdb_close();

#ifdef HDB_NET
	printf("disconnecting\n");
	hdb_disconnect();
#endif

	// close and do lsof before closing...
	//printf("INFO - Please check for any open files with lsof or any memory tests and hit any key to quit\n");
	//getchar();
	return ret;
}

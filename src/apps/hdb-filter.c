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
#include "hdb.h"
#include <unistd.h>

#define CMD_VERSION "$Id: hdb-filter.c,v 1.1 2006/03/12 13:32:13 hampusdb Exp $"

#define TOKEN_LENGTH 1024
#define TOKEN_DELIM  '%'
#if 0 
#define DBG(fmt...) fprintf(stderr, fmt)
#else
#define DBG(fmt...)
#endif

enum state { TOKEN_NONE, TOKEN_START };

static char * next_token(char *s);
static int db_method(char *prefix, char *token, char *postfix);
static int dbif_method(char *prefix, char *token, char *postfix);
static int sh_method(char *prefix, char *token, char *postfix);
static int include_method(char *prefix, char *token, char *postfix);

static int printing = 1;

int main(int argc, char ** argv)
{
  int c;
  char t[TOKEN_LENGTH + 1];
  int i;
  int hdb_net=0;
#ifdef HDB_NET_SWITCH
  int found_lock=0;
#endif
  int state = TOKEN_NONE;
	if(argc > 2){
		printf("hdb-filter V. %s\n", CMD_VERSION);
		exit(1);
	}

   hdb_net=hdb_get_access();
#if HDB_NET_SWITCH
        found_lock=hdb_check_lock(HDB_LOCKFILE);

        //if the daemon isn't started. Pass on the light-hdb
        if(hdb_net == HDB_ACCESS_NETWORK && found_lock){
				DBG("INFO - Switching to light-hdb\n");
                return execvp("lhdb-filter", argv);
        }
        //if the daemon is running. Use it.
        if(hdb_net == HDB_ACCESS_FILE && !found_lock){
                DBG("INFO - Switching to network-hdb\n");
                return execvp("nhdb-filter", argv);
        }
#endif

if(hdb_net){
  if(hdb_connect("127.0.0.1", 12429, NULL, NULL, 0)){
        DBG("ERR - Unable to connect to HDB daemon.\n");
		return 1;
  }	
  if(getenv("HDBROOT")){
	  if(hdb_set_root(getenv("HDBROOT"))){
		  fprintf(stderr, "ERR Unable to set root\n");
		  hdb_disconnect();
		  exit(1);
	  }
  }	
}

  while((c = getchar()) != EOF)
  {
    switch(state)
    {
      case TOKEN_START : 
	if (c == TOKEN_DELIM)
	{
	  int  ret;
	  char *prefix  = NULL;
	  char *method  = NULL;
	  char *token   = NULL;
	  char *postfix = NULL;
	  char *s = strdup(t);
	  char n;
	 
	  prefix  = s;
	  method  = next_token(prefix);
	  token   = next_token(method);
	  postfix = next_token(token);

	  DBG("prefix = %s\n", prefix);
	  DBG("method = %s\n", method);
	  DBG("token = %s\n", token);
	  DBG("postfix = %s\n", postfix);

	  if (token == NULL)
	  {
	    ret = 0;
	  }
	  else if (!strcmp(method, "DB"))
	  {
	    ret = db_method(prefix, token, postfix);
	  }
	  else if (!strcmp(method, "DBIF"))
	  {
	    ret = dbif_method(prefix, token, postfix);
            n = getchar(); if (n != '\n') ungetc(n, stdin);
	  }
	  else if (!strcmp(method, "ELSE"))
	  {
	    ret = 1;
	    printing = !printing;
            n = getchar(); if (n != '\n') ungetc(n, stdin);
	  }
	  else if (!strcmp(method, "FI"))
	  {
	    ret = 1;
	    printing = 1;
            n = getchar(); if (n != '\n') ungetc(n, stdin);
	  }
	  else if (!strcmp(method, "SH"))
	  {
	    ret = sh_method(prefix, token, postfix);
	  }
	  else if (!strcmp(method, "INCLUDE"))
	  {
	    ret = include_method(prefix, token, postfix);
	  }
	  else
	  {
	    ret = 0;
	  }
	  
	  if (!ret)
	  {
	    if (printing) printf("%c%s%c", TOKEN_DELIM, t, TOKEN_DELIM);
	  }
	  free(s);
	  state = TOKEN_NONE;	  	  
	}
	else if (c == '\n' || i == TOKEN_LENGTH)
	{
	  state = TOKEN_NONE;
	  if (printing) printf("%c%s%c", TOKEN_DELIM, t, c);
	}
	else
	{
	  t[i++] = c;	  
	}
	break;
      case TOKEN_NONE :
	if (c == TOKEN_DELIM)
	{
	  state = TOKEN_START;
	  i = 0;
	  memset(t, 0, TOKEN_LENGTH);
	}
	else
	{
	  if (printing) putchar(c);
	}
	break; 
    default: break;
    }
  }
	if(hdb_net){
		hdb_disconnect();
	}
  return 0;
}

static char* next_token(char *s)
{
  if (s == NULL)
  {
    return s;
  }

  while(*s)
  {
    if (*s == '\\')
    {
      s++;
      if (*s) s++;
      continue;
    }

    if (*s == ':')
    {
      *s = '\0';
      return (s+1);
    }
    s++;
  }
  return NULL;
}

static int db_method(char *prefix, char *token, char *postfix)
{	        
  char list[256];
  char *db_key;
  char *val;
  int i;

  if (!printing)
    return 1;

  db_key = strrchr(token, '.');
  DBG("db_key = %s\n", db_key);
  if (db_key)
  {
    *db_key = 0; 
    db_key++;
  }
  i = 0;
  while (*token && i < 256)
  {
    if (*token == '.')
    {
      *token = '/';
    }
    else if (*token == '\\')
    {
      token++;
      if (*token == '\0') break;
    }
    list[i] = *token;
    i++;
    token++;
  }
  list[i] = 0;
  val = hdb_get_val(list, db_key);
  DBG("val = %s\n", val);
  if (val && strcmp(val, ""))
  {
    printf("%s%s%s", 
	   prefix  ? prefix  : "",
	   val     ? val     : "",
	   postfix ? postfix : "");
    free(val);
  }
  return 1;
}

static int dbif_method(char *prefix, char *token, char *postfix)
{	        
  char list[256];
  char *db_key;
  int  val;
  int i;

  db_key = strrchr(token, '.');
  DBG("db_key = %s\n", db_key);
  if (db_key)
  {
    *db_key = 0; 
    db_key++;
  }
  i = 0;
  while (*token && i < 256)
  {
    if (*token == '.')
    {
      *token = '/';
    }
    else if (*token == '\\')
    {
      token++;
      if (*token == '\0') break;
    }
    list[i] = *token;
    i++;
    token++;
  }
  list[i] = 0;
  val = hdb_get_bool(list, db_key);
  DBG("val = %d\n", val);
  if (val)
  {
    printing = 1;
  }
  else
  {
    printing = 0;
  }
  return 1;
}

static int sh_method(char * prefix, char *token, char *postfix)
{
  FILE* p;
  int c;

  if (!printing)
    return 1;

  p = popen(token, "r");
  if (p == NULL)
  {
    return 1;
  }

  c = fgetc(p);
  if (c == EOF)
  {
    pclose(p);
    return 1;
  }

  printf("%s%c", prefix ? prefix : "", c);

  while ((c = fgetc(p)) != EOF)
  {
    if (c == '\\')
    {
      continue;
    }
    putchar(c);
  }
  pclose(p);
  printf("%s", postfix ? postfix : "");
  return 1;
}

static int include_method(char *prefix, char *token, char * postfix)
{
  FILE* p;
  int c;
  
  if (!printing) 
    return 1;

  p = fopen(token, "r");
  if (p == NULL)
  {
    return 1;
  }

  c = fgetc(p);

  if (c == EOF)
  {
    pclose(p);
    return 1;
  }

  printf("%s%c", prefix ? prefix : "", c);

  while ((c = fgetc(p)) != EOF)
  {
    putchar(c);
  }
  pclose(p);
  printf("%s", postfix ? postfix : "");
  return 1;  
}

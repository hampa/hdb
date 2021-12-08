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
//based no code from Terje Eggestad Midway released under GPL
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "urlencode.h"

int urlnencode(char * encode, const char * uncoded, int len);

/******************************************************
 * Some internal helper functions
 ******************************************************/
/* nibble return the hex char for the lower 4 bits in c. */
static char nibble(char c)
{
  c &= 0xf;
  if (c < 10) return c + '0';
  return c + 0x37;
};

/* hex accepts a char c and return a pointer to 
   the low and high nibble, hex encoded.
   Note that high and low are NOT NULL encoded.
   e.g c = "/" then high -> '2' low -> 'F';
*/
static void hex (char c, char * high, char * low)
{
  *low  =  nibble(c);
  c = c >> 4;
  *high =  nibble(c);
};

/* The opposite of hex(), but retirn an int since 
 * we return -1 of high or low are not a hexdigit [0-9A-Za-z]
 * the return range is [-1:255].
 */
static int unhex(char high, char low)
{
   int res = 0;
   if (isdigit(high)) high = (0xf & high);
   else if (isxdigit(high)) high = (0xf & high) + 9;
   else return -1;

   if (isdigit(low)) low = (0xf & low);
   else if (isxdigit(low)) low = (0xf & low) + 9;
   else return -1;

   res = high & 0xf;
   res <<= 4;
   res += low & 0xf;

   return res;
};

/*********************************************************
 * API for do string (field) en/decode. 
 * they (un)escapes chars illegal in a URL.
 *********************************************************/

/* url n encode
   URL encode a buffer, by copy. It allows NULL in uncoded strings.
   ret is a pointer to at char * that will point to a 
   malloced buffer holding the encoded string.
   uncoded is the input string, and len the number of octets.
   it returns the number of bytes in the return buffer or 0
   on error.
*/
int urlnencodedup(char ** ret, const char * uncoded, int len)
{
  
  char * out;
  int outlen;

  /* Input control */  
  if ( (ret == NULL) || (uncoded == NULL) || (len < 0) ) {
    errno = EINVAL;
    return -1;
  };

  /* new buffer */
  out = (char *) malloc(len*3+1);
  if (out == NULL) return 0;

  outlen = urlnencode(out, uncoded, len);
  *ret = out;
  return outlen;
};

/* it is here assumed that encode is a buffer large enough, which is
   maximum 3 times the size of uncoded */
int urlnencode(char * encode, const char * uncoded, int len)
{
  int i;

  int j;

  /* Input control */  
  if ( (encode == NULL) || (uncoded == NULL) || (len < 0) ) {
    errno = EINVAL;
    return -1;
  };
  
  /* if len == 0 uncoded is null terminated. */
  if (len == 0) len = strlen(uncoded);

  /* do actually copy, and encode. 
     We could allow $-_.+!*'() to pass unencoded according to 
     "HTML The Definitive Guide" O`Reilly Nutshell series.
     But + should be space!!!
     However we are not wrong to overencode.
  */
  j = 0;
  for (i = 0; i< len; i++) {
    if (isalnum(uncoded[i])) 
      encode[j++] = uncoded[i];
    else {
      encode[j++] = '%';
      hex(uncoded[i], &encode[j], &encode[j+1]);
      j += 2;
    };
  };
  encode[j] = '\0';
  return j;
};

/* A couple of simple calls that do not allow octet strings containing
   NULL. */
char * urlencodedup(char * str)
{
  char * out;
  urlnencodedup(&out, str, 0);
  return out;
};

int urlencode(char * out, const char * str)
{
  return urlnencode(out, str, 0);
};


/* urldecode url decode a buffer, urldecodedup allocs the buffer.  ret
   is a pointer to a char * that will point to a malloc`ed return
   buffer.  */
int urldecodedup(char ** ret, char * encoded)
{
  int len;

  /* Input control */
  if ( (ret == NULL) || (encoded == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  len = strlen(encoded);
  *ret = (char *) malloc(len+1);
  if (*ret == NULL) return -1; /* errno set by malloc() */

  len = urldecode(*ret, encoded);
  return len;
};

/* plain must be a buffer at the same length as encoded.  we return
   the numbers of bytes in the plain buffer. encoded must be NULL
   terminated (of course), but we decode up to the first NULL, & or =
   whatever comes first.  return number of octets in return buffer.  */
int urldecode(char * plain, char * encoded)
{
	char * equal, * ampersand;
	int c, i, len;
	int j = 0;


	if ( (plain == NULL) || (encoded == NULL) ) {
		errno = EINVAL;
		return -1;
	};

	len = strlen(encoded);

	equal = strchr(encoded, '=');
	ampersand = strchr(encoded, '&');

	if ((ampersand != NULL) && (ampersand - encoded < len)) 
		len = ampersand - encoded;
	if ((equal != NULL) && (equal - encoded < len))
		len = equal - encoded;

	/* on decode, we only have to look for % and +, % must be 
	   followed by two hex chars. */
	for (i = 0; i< len; i++) {
		if (encoded[i] == '%') {
			i++;
			c = unhex (encoded[i], encoded[i+1]);
			if (c == -1) {
				printf("Failed to get hex value of %c(%d)%c(%d)", 
						encoded[i], encoded[i], encoded[i+1], encoded[i+1]);
				errno = EINVAL;
				return -1;
			};
			i++;
			plain[j++] = (char) c;
		} else if (encoded[i] == '+') {
			plain[j++] = ' ';
		} else {
			plain[j++] = encoded[i];
		};
	};
	plain[j] = '\0';
	return j;
};


#if 0 
int main(){
	char buf[256];
	char encode[256*3+1];
	char plain[256];
	char *values[] = {"x", "1", "12", "       +*?!@~","12", "asdasd", "yyyy",NULL};
	int ret=0;

	strcpy(buf, "asdfasdf");
	if(ret=urldecode(encode, buf)){
		printf("FAILed toencode empty=%s ret=%i\n", encode, ret);
	}
	urlencode(encode, values[3]);
	//strcpy(encode , "%25E4%25F6%25E5%252B%252A%253F%2521%2540%257E");
	urldecode(plain, encode);
	printf("encode %s -> plain %s\n", encode, plain);

	strcpy(buf, "hej \nsvejs\n");
	urlencode(encode, buf);

	printf("encode : %s\n", encode);
	urldecode(plain, encode);
	printf("decode : %s\n", plain);	

	return 0;
} 
#endif

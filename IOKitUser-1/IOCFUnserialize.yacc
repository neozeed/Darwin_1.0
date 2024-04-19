/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * HISTORY
 *
 * 09 Dec 99 rsulack created by rsulack
 */

// parser for unserializing OSContainer objects serialized to XML
//
// to build :
//	bison -p IOCFUnserialize -o IOCFUnserialize.tab.c IOCFUnserialize.yacc
//
//
//
//
//
//
//		 DO NOT EDIT IOCFUnserialize.tab.c
//
//			this means you!
//
//
//
//
//

     
%{
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFDictionary.h>

typedef	struct object {
	struct object	*next;
	struct object	*free;
	struct object	*elements;
	CFTypeRef	object;
	CFStringRef	key;		// for dictionary
	int		size;
	void		*data;		// for data
	char		*string;	// for string & symbol
	long long 	number;		// for number
	int		idref;
} object_t;

static int yyparse();
static int yyerror(char *s);
static int yylex();

static CFAllocatorRef cfAllocator;

static object_t * newObject();
static void freeObject(object_t *o);

static object_t *buildCFDictionary(object_t *);
static object_t *buildCFArray(object_t *);
static object_t *buildCFSet(object_t *);
static object_t *buildCFString(object_t *);
static object_t *buildCFData(object_t *);
static object_t *buildCFNumber(object_t *);
static object_t *buildCFBoolean(object_t *o);

static void rememberObject(int, CFTypeRef);
static object_t *retrieveObject(int);

// resultant object of parsed text
static CFTypeRef parsedObject;

#define YYSTYPE object_t *


%}
%token KEY
%token NUMBER
%token STRING
%token DATA
%token IDREF
%token BOOLEAN
%token SYNTAX_ERROR     
%% /* Grammar rules and actions follow */

input:	  /* empty */		{ parsedObject = NULL; YYACCEPT; }
	| object		{ parsedObject = $1->object;
				  $1->object = 0;
				  freeObject($1);
				  YYACCEPT;
				}
	| SYNTAX_ERROR		{
				  yyerror("syntax error");
				  YYERROR;
				}
	;

object:	  dict			{ $$ = buildCFDictionary($1); }
	| array			{ $$ = buildCFArray($1); }
	| set			{ $$ = buildCFSet($1); }
	| string		{ $$ = buildCFString($1); }
	| data			{ $$ = buildCFData($1); }
	| number		{ $$ = buildCFNumber($1); }
	| boolean		{ $$ = buildCFBoolean($1); }
	| idref			{ $$ = retrieveObject($1->idref);
				  if ($$) {
				    CFRetain($$->object);
				  } else { 
				    yyerror("forward reference detected");
				    YYERROR;
				  }
				  freeObject($1);
				}
	;

//------------------------------------------------------------------------------

dict:	  '{' '}'		{ $$ = $1;
				  $$->elements = NULL;
				}
	| '{' pairs '}'		{ $$ = $1;
				  $$->elements = $2;
				}
	;

pairs:	  pair
	| pairs pair		{ $$ = $2;
				  $$->next = $1;
				}
	;

pair:	  key object		{ $$ = $1;
				  $$->key = $$->object;
				  $$->object = $2->object;
				  $$->next = NULL; 
				  $2->object = 0;
				  freeObject($2);
				}
	;

key:	  KEY			{ $$ = buildCFString($1); }
	;

//------------------------------------------------------------------------------

array:	  '(' ')'		{ $$ = $1;
				  $$->elements = NULL;
				}
	| '(' elements ')'	{ $$ = $1;
				  $$->elements = $2;
				}
	;

set:	  '[' ']'		{ $$ = $1;
				  $$->elements = NULL;
				}
	| '[' elements ']'	{ $$ = $1;
				  $$->elements = $2;
				}
	;

elements: object		{ $$ = $1; 
				  $$->next = NULL; 
				}
	| elements object	{ $$ = $2;
				  $$->next = $1;
				}
	;

//------------------------------------------------------------------------------

number:	  NUMBER
	;

data:	  DATA
	;

string:	  STRING
	;

idref:	  IDREF
	;

boolean:  BOOLEAN
	;

%%

static int		lineNumber = 0;
static const char	*parseBuffer;
static int		parseBufferIndex;

#define currentChar()	(parseBuffer[parseBufferIndex])
#define nextChar()	(parseBuffer[++parseBufferIndex])
#define prevChar()	(parseBuffer[parseBufferIndex - 1])

#define isSpace(c)	((c) == ' ' || (c) == '\t')
#define isAlpha(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define isDigit(c)	((c) >= '0' && (c) <= '9')
#define isAlphaDigit(c)	((c) >= 'a' && (c) <= 'f')
#define isHexDigit(c)	(isDigit(c) || isAlphaDigit(c))
#define isAlphaNumeric(c) (isAlpha(c) || isDigit(c) || ((c) == '-')) 

static char yyerror_message[128];

int
yyerror(char *s)  /* Called by yyparse on error */
{
	sprintf(yyerror_message, "IOCFUnserialize: %s near line %d\n", s, lineNumber);
	return 0;
}

#define TAG_MAX_LENGTH		32
#define TAG_MAX_ATTRIBUTES	32
#define TAG_BAD			0
#define TAG_START		1
#define TAG_END			2
#define TAG_EMPTY		3
#define TAG_COMMENT		4

static int
getTag(char tag[TAG_MAX_LENGTH],
       int *attributeCount, 
       char attributes[TAG_MAX_ATTRIBUTES][TAG_MAX_LENGTH],
       char values[TAG_MAX_ATTRIBUTES][TAG_MAX_LENGTH] )
{
	int length = 0;
	int c = currentChar();
	int tagType = TAG_START;

	*attributeCount = 0;

	if (c != '<') return TAG_BAD;
        c = nextChar();		// skip '<'

        if (c == '?' || c == '!') {
                while ((c = nextChar()) != 0) {
                        if (c == '\n') lineNumber++;
                        if (c == '>') {
                                (void)nextChar();
                                return TAG_COMMENT;
                        }
                }
        }

	if (c == '/') {
		c = nextChar();		// skip '/'
		tagType = TAG_END;
	}
        if (!isAlpha(c)) return TAG_BAD;

	/* find end of tag while copying it */
	while (isAlphaNumeric(c)) {
		tag[length++] = c;
		c = nextChar();
		if (length >= (TAG_MAX_LENGTH - 1)) return TAG_BAD;
	}

	tag[length] = 0;

//printf("tag %s, type %d\n", tag, tagType);
	
	// look for attributes of the form attribute = "value" ...
	while ((c != '>') && (c != '/')) {
		while (isSpace(c)) c = nextChar();

		length = 0;
		while (isAlphaNumeric(c)) {
			attributes[*attributeCount][length++] = c;
			if (length >= (TAG_MAX_LENGTH - 1)) return TAG_BAD;
			c = nextChar();
		}
		attributes[*attributeCount][length] = 0;

		while (isSpace(c)) c = nextChar();
		
		if (c != '=') return TAG_BAD;
		c = nextChar();
		
		while (isSpace(c)) c = nextChar();

		if (c != '"') return TAG_BAD;
		c = nextChar();
		length = 0;
		while (c != '"') {
			values[*attributeCount][length++] = c;
			if (length >= (TAG_MAX_LENGTH - 1)) return TAG_BAD;
			c = nextChar();
		}
		values[*attributeCount][length] = 0;

		c = nextChar(); // skip closing quote

//printf("	attribute '%s' = '%s', nextchar = '%c'\n", attributes[*attributeCount], values[*attributeCount], c);

		(*attributeCount)++;
		if (*attributeCount >= TAG_MAX_ATTRIBUTES) return TAG_BAD;
	}

	if (c == '/') {
		c = nextChar();		// skip '/'
		tagType = TAG_EMPTY;
	}
	if (c != '>') return TAG_BAD;
	c = nextChar();		// skip '>'

	return tagType;
}

static char *
getString()
{
	int c = currentChar();
	int start, length, i, j;
	char * tempString;

	start = parseBufferIndex;
	/* find end of string */

	while (c != 0) {
		if (c == '\n') lineNumber++;
		if (c == '<') {
			break;
		}
		c = nextChar();
	}

	if (c != '<') return 0;

	length = parseBufferIndex - start;

	/* copy to null terminated buffer */
	tempString = (char *)malloc(length + 1);
	if (tempString == 0) {
		printf("IOCFUnserialize: can't alloc temp memory\n");
		goto error;
	}

	// copy out string in tempString
	// "&amp;" -> '&', "&lt;" -> '<', "&gt;" -> '>'

	i = j = 0;
	while (i < length) {
		c = parseBuffer[start + i++];
		if (c != '&') {
			tempString[j++] = c;
		} else {
			if ((i+3) > length) goto error;
			c = parseBuffer[start + i++];
			if (c == 'l') {
				if (parseBuffer[start + i++] != 't') goto error;
				if (parseBuffer[start + i++] != ';') goto error;
				tempString[j++] = '<';
				continue;
			}	
			if (c == 'g') {
				if (parseBuffer[start + i++] != 't') goto error;
				if (parseBuffer[start + i++] != ';') goto error;
				tempString[j++] = '>';
				continue;
			}	
			if ((i+3) > length) goto error;
			if (c == 'a') {
				if (parseBuffer[start + i++] != 'm') goto error;
				if (parseBuffer[start + i++] != 'p') goto error;
				if (parseBuffer[start + i++] != ';') goto error;
				tempString[j++] = '&';
				continue;
			}
			goto error;
		}	
	}
	tempString[j] = 0;

//printf("string %s\n", tempString);

	return tempString;

error:
	if (tempString) free(tempString);
	return 0;
}

static long long
getNumber()
{
	unsigned long long n = 0;
	int base = 10;
	int c = currentChar();

	if (!isDigit (c)) return 0;

	if (c == '0') {
		c = nextChar();
		if (c == 'x') {
			base = 16;
			c = nextChar();
		}
	}
	if (base == 10) {
		while(isDigit(c)) {
			n = (n * base + c - '0');
			c = nextChar();
		}
	} else {
		while(isHexDigit(c)) {
			if (isDigit(c)) {
				n = (n * base + c - '0');
			} else {
				n = (n * base + 0xa + c - 'a');
			}
			c = nextChar();
		}
	}
//printf("number 0x%x\n", (unsigned long)n);
	return n;
}

// taken from CFXMLParsing/CFPropertyList.c

static const signed char __CFPLDataDecodeTable[128] = {
    /* 000 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 010 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 020 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 030 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* ' ' */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* '(' */ -1, -1, -1, 62, -1, -1, -1, 63,
    /* '0' */ 52, 53, 54, 55, 56, 57, 58, 59,
    /* '8' */ 60, 61, -1, -1, -1,  0, -1, -1,
    /* '@' */ -1,  0,  1,  2,  3,  4,  5,  6,
    /* 'H' */  7,  8,  9, 10, 11, 12, 13, 14,
    /* 'P' */ 15, 16, 17, 18, 19, 20, 21, 22,
    /* 'X' */ 23, 24, 25, -1, -1, -1, -1, -1,
    /* '`' */ -1, 26, 27, 28, 29, 30, 31, 32,
    /* 'h' */ 33, 34, 35, 36, 37, 38, 39, 40,
    /* 'p' */ 41, 42, 43, 44, 45, 46, 47, 48,
    /* 'x' */ 49, 50, 51, -1, -1, -1, -1, -1
};

#define CFDATA_ALLOC_SIZE 4096

static void *
getData(unsigned int *size)
{
    int numeq = 0, acc = 0, cntr = 0;
    int tmpbufpos = 0, tmpbuflen = 0;
    unsigned char *tmpbuf = (unsigned char *)malloc(CFDATA_ALLOC_SIZE);

    int c = currentChar();
    *size = 0;
	
    while (c != '<') {
        c &= 0x7f;
	if (c == 0) {
		free(tmpbuf);
		return 0;
	}
	if (c == '=') numeq++; else numeq = 0;
	if (c == '\n') lineNumber++;
        if (__CFPLDataDecodeTable[c] < 0) {
	    c = nextChar();
            continue;
	}
        cntr++;
        acc <<= 6;
        acc += __CFPLDataDecodeTable[c];
        if (0 == (cntr & 0x3)) {
            if (tmpbuflen <= tmpbufpos + 2) {
                tmpbuflen += CFDATA_ALLOC_SIZE;
		tmpbuf = (unsigned char *)realloc(tmpbuf, tmpbuflen);
            }
            tmpbuf[tmpbufpos++] = (acc >> 16) & 0xff;
            if (numeq < 2)
                tmpbuf[tmpbufpos++] = (acc >> 8) & 0xff;
            if (numeq < 1)
                tmpbuf[tmpbufpos++] = acc & 0xff;
        }
	c = nextChar();
    }
    *size = tmpbufpos;
    return tmpbuf;
}

static int
yylex()
{
	int c, i;
	int tagType;
	char tag[TAG_MAX_LENGTH];
	int attributeCount;
	char attributes[TAG_MAX_ATTRIBUTES][TAG_MAX_LENGTH];
	char values[TAG_MAX_ATTRIBUTES][TAG_MAX_LENGTH];

	if (parseBufferIndex == 0) lineNumber = 1;

 top:
	c = currentChar();

	/* skip white space  */
	if (isSpace(c)) while ((c = nextChar()) != 0 && isSpace(c)) {};

	/* keep track of line number, don't return \n's */
	if (c == '\n') {
		lineNumber++;
		(void)nextChar();
		goto top;
	}
	
	if (!c || c == ',') {
		(void)nextChar();
		return c;
	}

	tagType = getTag(tag, &attributeCount, attributes, values);
	if (tagType == TAG_BAD) return SYNTAX_ERROR;
	if (tagType == TAG_COMMENT) goto top;

	// this code handles empty tags, for idrefs we ignore the tag
	// for this to work all idrefs must be unique across the whole serialization
	if (tagType == TAG_EMPTY) {
		if (!strcmp(tag, "true") || !strcmp(tag, "false")) {
				yylval = newObject();
				yylval->number = *tag == 't';
				return BOOLEAN;
		}
		for (i=0; i < attributeCount; i++) {
			if (!strcmp(attributes[i], "IDREF")) {
				yylval = newObject();
				yylval->idref = strtol(values[i], NULL, 0);
				return IDREF;
			}
		}
		return SYNTAX_ERROR;
	}

	// handle allocation and check of "ID" tag up front
	yylval = newObject();
	yylval->idref = -1;
	for (i=0; i < attributeCount; i++) {
		if (attributes[i][0] == 'I' && attributes[i][1] == 'D' && !attributes[i][2]) {
			yylval->idref = strtol(values[i], NULL, 0);
		}
	}

	switch (*tag) {
	case 'a':
		if (!strcmp(tag, "array")) {
			return (tagType == TAG_START) ? '(' : ')';
		}
		break;
	case 'd':
		if (!strcmp(tag, "dict")) {
			return (tagType == TAG_START) ? '{' : '}';
		}
		if (!strcmp(tag, "data")) {
			unsigned int size;
			yylval->data = getData(&size);
			yylval->size = size;
			if ((getTag(tag, &attributeCount, attributes, values) != TAG_END) || strcmp(tag, "data")) {
				return SYNTAX_ERROR;
			}
			return DATA;
		}
		break;
	case 'i':
		if (!strcmp(tag, "integer")) {
			yylval->number = getNumber();
			yylval->size = 64;	// default
			for (i=0; i < attributeCount; i++) {
				if (!strcmp(attributes[i], "size")) {
					yylval->size = strtoul(values[i], NULL, 0);
				}
			}
			if ((getTag(tag, &attributeCount, attributes, values) != TAG_END) || strcmp(tag, "integer")) {
				return SYNTAX_ERROR;
			}
			return NUMBER;
		}
		break;
	case 'k':
		if (!strcmp(tag, "key")) {
			yylval->string = getString();
			if (!yylval->string) {
				return SYNTAX_ERROR;
			}
			if ((getTag(tag, &attributeCount, attributes, values) != TAG_END)
			   || strcmp(tag, "key")) {
				return SYNTAX_ERROR;
			}
			return KEY;
		}
		break;
	case 'p':
		if (!strcmp(tag, "plist")) {
			freeObject(yylval);
			goto top;
		}
		break;
	case 's':
		if (!strcmp(tag, "string")) {
			yylval->string = getString();
			if (!yylval->string) {
				return SYNTAX_ERROR;
			}
			if ((getTag(tag, &attributeCount, attributes, values) != TAG_END)
			   || strcmp(tag, "string")) {
				return SYNTAX_ERROR;
			}
			return STRING;
		}
		if (!strcmp(tag, "set")) {
			if (tagType == TAG_START) {
				return '[';
			} else {
				return ']';
			}
		}
		break;

	default:
		// XXX should we ignore invalid tags?
		return SYNTAX_ERROR;
		break;
	}

	return 0;
}

// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#

// "java" like allocation, if this code hits a syntax error in the
// the middle of the parsed string we just bail with pointers hanging
// all over place, so this code helps keeps all together

static object_t *objects = 0;
static object_t *freeObjects = 0;

object_t *
newObject()
{
	object_t *o;

	if (freeObjects) {
		o = freeObjects;
		freeObjects = freeObjects->next;
	} else {
		o = (object_t *)malloc(sizeof(object_t));
		bzero(o, sizeof(object_t));
		o->free = objects;
		objects = o;
	}
	
	return o;
}

void
freeObject(object_t *o)
{
	o->next = freeObjects;
	freeObjects = o;	
}

void
cleanupObjects()
{
	object_t *t, *o = objects;

	while (o) {
		if (o->object) {
			printf("IOCFUnserialize: releasing object o=%x object=%x\n", (int)o, (int)o->object);
			CFRelease(o->object);
		}
		if (o->data) {
			printf("IOCFUnserialize: freeing   object o=%x data=%x\n", (int)o, (int)o->data);
			free(o->data);
		}
		if (o->key) {
			printf("IOCFUnserialize: releasing object o=%x key=%x\n", (int)o, (int)o->key);
			CFRelease(o->key);
		}
		if (o->string) {
			printf("IOCFUnserialize: freeing   object o=%x string=%x\n", (int)o, (int)o->string);
			free(o->string);
		}

		t = o;
		o = o->free;
		free(t);
	}
}

// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#

static CFMutableDictionaryRef tags;

static void 
rememberObject(int tag, CFTypeRef o)
{
//printf("remember idref %d\n", tag);

	CFDictionarySetValue(tags, (const void *) tag,  (const void *) o);
}

static object_t *
retrieveObject(int tag)
{
	CFTypeRef ref;
	object_t *o;

//printf("retrieve idref '%d'\n", tag);

	ref  = (CFTypeRef)CFDictionaryGetValue(tags, (const void *)tag);
	if (!ref) return 0;

	o = newObject();
	o->object = ref;
	return o;
}

// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#
// !@$&)(^Q$&*^!$(*!@$_(^%_(*Q#$(_*&!$_(*&!$_(*&!#$(*!@&^!@#%!_!#

object_t *
buildCFDictionary(object_t * header)
{
	object_t *o, *t;
	int count = 0;
        CFMutableDictionaryRef dict;

	// get count and reverse order
	o = header->elements;
	header->elements = 0;
	while (o) {
		count++;
		t = o;
		o = o->next;

		t->next = header->elements;
		header->elements = t;
	}

        dict = CFDictionaryCreateMutable(cfAllocator, count,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	if (header->idref >= 0) rememberObject(header->idref, dict);

	o = header->elements;
	while (o) {
                CFDictionarySetValue(dict, o->key, o->object);

                CFRelease(o->key); 
                CFRelease(o->object); 
		o->key = 0;
		o->object = 0;

		t = o;
		o = o->next;
		freeObject(t);
	}
	o = header;
	o->object = dict;
	return o;
};

object_t *
buildCFArray(object_t * header)
{
	object_t *o, *t;
	int count = 0;
	CFMutableArrayRef array;

	// get count and reverse order
	o = header->elements;
	header->elements = 0;
	while (o) {
		count++;
		t = o;
		o = o->next;

		t->next = header->elements;
		header->elements = t;
	}

	array = CFArrayCreateMutable(cfAllocator, count, &kCFTypeArrayCallBacks);
	if (header->idref >= 0) rememberObject(header->idref, array);

	o = header->elements;
	while (o) {
		CFArrayAppendValue(array, o->object);

		CFRelease(o->object);
		o->object = 0;

		t = o;
		o = o->next;
		freeObject(t);
	}
	o = header;
	o->object = array;
	return o;
};

object_t *
buildCFSet(object_t *header)
{
	object_t *o, *t;
	int count = 0;
	CFMutableSetRef set;

	// get count and reverse order
	o = header->elements;
	header->elements = 0;
	while (o) {
		count++;
		t = o;
		o = o->next;

		t->next = header->elements;
		header->elements = t;
	}

	set = CFSetCreateMutable(cfAllocator, count, &kCFTypeSetCallBacks);
	if (header->idref >= 0) rememberObject(header->idref, set);

	o = header->elements;
	while (o) {
		CFSetAddValue(set, o->object);

		CFRelease(o->object);
		o->object = 0;

		t = o;
		o = o->next;
		freeObject(t);
	}
	o = header;
	o->object = set;
	return o;
};

object_t *
buildCFString(object_t *o)
{
	CFStringRef string;

	string = CFStringCreateWithCString(cfAllocator, o->string,
					   kCFStringEncodingASCII);
	if (o->idref >= 0) rememberObject(o->idref, string);

	free(o->string);
	o->string = 0;
	o->object = string;

	return o;
};

object_t *
buildCFData(object_t *o)
{
	CFDataRef data;

	data = CFDataCreate(cfAllocator, (const UInt8 *) o->data, o->size);
	if (o->idref >= 0) rememberObject(o->idref, data);

	if (o->size) free(o->data);
	o->data = 0;
	o->object = data;
	return o;
};

object_t *
buildCFNumber(object_t *o)
{
	CFNumberRef 	number;
	CFNumberType 	numType;
	const UInt8 *	bytes;

	bytes = (const UInt8 *) &o->number;
	if( o->size <= 32) {
		numType = kCFNumberSInt32Type;
#if __BIG_ENDIAN__
		bytes += 4;
#endif
	} else {
		numType = kCFNumberSInt64Type;
	}

        number = CFNumberCreate(cfAllocator, numType,
				(const void *) bytes);

	if (o->idref >= 0) rememberObject(o->idref, number);

	o->object = number;
	return o;
};

object_t *
buildCFBoolean(object_t *o)
{
	o->object = (o->number == 0) ? kCFBooleanFalse : kCFBooleanTrue;
	return o;
};


//XXX unserialize is not reentrant, MP-safe, ... without these defined
#define LOCK()
#define UNLOCK()

CFTypeRef
IOCFUnserialize(const char *buffer,
                CFAllocatorRef allocator,
                CFOptionFlags  options,
                CFStringRef *  errorString)
{
	CFTypeRef object;

	if( (!buffer) || options)
		return( 0 );

	LOCK();
	objects = 0;
	freeObjects = 0;
	yyerror_message[0] = 0;		//just in case
	parseBuffer = buffer;
	parseBufferIndex = 0;
	tags = CFDictionaryCreateMutable(allocator, 0,
		0, /* key callbacks */
		&kCFTypeDictionaryValueCallBacks);
	cfAllocator = allocator;

	if (yyparse() == 0) {
		object = parsedObject;
		if (errorString) *errorString = 0;
	} else {
		object = 0;
		if (errorString)
			*errorString = CFStringCreateWithCString(allocator,
					yyerror_message, kCFStringEncodingASCII);
	}

	cleanupObjects();
	CFRelease(tags);
	UNLOCK();

	return object;
}


//
//
//
//
//
//		 DO NOT EDIT IOCFUnserialize.tab.c
//
//			this means you!
//
//
//
//
//

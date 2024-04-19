/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/* IOUnserialize.cpp created by gvdl on 1999-3-18 */
/*
 * Comments and white space are ignored.  LL parser for the following grammar.
 * In the list transitions the trailing seperator is not mandatory.
 *
 *  Comment	    ==> # .* \n
 *
 *  Object	    ==> Dictionary
 *		      | Set
 *		      | Array
 *		      | String
 *		      | Data
 *		      | Offset
 *		      | Range
 *
 *  Dictionary	    ==> { KeyValueList }
 *
 *  KeyValueList    ==> EMPTY
 *		      | KeyValue ; KeyValueList
 *
 *  KeyValue	    ==> String = Object
 *
 *  Set		    ==> [ ObjectList ]
 *
 *  Array	    ==> ( ObjectList )
 *
 *  ObjectList	    ==> EMPTY
 *		      | Object , ObjectList
 *
 *  Data	    ==> < ByteList >
 *
 *  ByteList	    ==> EMPTY
 *		      | Byte ByteList
 *
 *  Byte	    ==> [0-9a-fA-F][0-9a-fA-F]
 *
 *  Offset	    ==> OffsetOrRange
 *		      | OffsetOrRange : Number
 *
 *  Range	    ==> OffsetOrRange - Number
 *
 *  OffsetOrRange   ==> Number
 *
 *  Number	    ==> [1-9][0-9]+
 *		      | 0[0-7]+
 *		      | 0x[0-9a-fA-F]+
 *
 *  String	    ==> Alpha CharacterList
 *		      | ' CharacterList '
 *		      | " CharacterList "
 *
 *  CharacterList   ==> EMPTY
 *		      | AnyPrintableCharacter CharacterList
 *
 *  Alpha	    ==> [a-zA-Z]
 */

static const char kBegBrace  = '{';
static const char kEndBrace  = '}';
static const char kBegParen  = '(';
static const char kEndParen  = ')';
static const char kBegSquare = '[';
static const char kEndSquare = ']';
static const char kBegAngle  = '<';
static const char kEndAngle  = '>';
static const char kDoubleQut = '"';
static const char kSingleQut = '\'';
static const char kSemiColon = ';';
static const char kComma     = ',';
static const char kColon     = ':';
static const char kHyphen    = '-';
static const char kEqualSign = '=';
static const char kBackSlash = '\\';
static const char kNewLine   = '\n';
static const char kHash	     = '#';
static const char kSpace     = ' ';

static inline bool isAlpha(const char c) {
    return (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z');
}
    
static inline bool isOctNumeric(const char c) {
    return (c >= '0' && c <= '7');
}
    
static inline bool isDecNumeric(const char c) {
    return (c >= '0' && c <= '9');
}
    
static inline bool isHexNumeric(const char c) {
    return (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'f')
	|| (c >= 'A' && c <= 'F');
}
    
class lexxer {
public:
private:
    lexxer() { };

    char *buffer;
    char errorString[128];
    char *ip;
    int unget;
    int lineNumber;
    char charNumber;
    bool lastChar;

public:
    lexxer(char *inBuf) { buffer = ip = inBuf; errorString[0] = 0; };

    bool hasError() const { return !errorString[0]; };
    int getLineNumber() const { return lineNumber; };
    int getCharNumber() const { return charNumber; };
    char *getCurrentCharPointer const { return ip; };

    const char *getErrorString() const { return errorString; };
    void setErrorString(const char *errStr)
	{ if (!hasError() strncpy(errStr, errorString, sizeof(errorString)); };

    void ungetChar(char c) { unget = c; };
    char getChar();
    char getLiteralChar();
};

/*
 *  Comment	    ==> # .* \n
 */
char lexxer::getChar()
{
    char c;

    if (hasError())
	return 0;

    if (unget != -1) {
	c = unget;
	unget = 0;
	return c;
    }	    

    if (lastChar) {
	setErrorString("Input overrun error");
	return 0;
    }

    while ( (c = *ip++) ) {
	charNumber++;
	if (c == kNewLine) {
	    lineNumber++;
	    charNumber = 0;
	}
	else if (c == kHash) {
	    while ( (c = *ip++) && c != kNewLine)
		;
	    ip--;
	}
	else if (c != kSpace)
	    break;
    }

    if (!c)
	lastChar = true;

    return c;
}

char lexxer::getLiteralChar()
{
    char c;

    if (hasError())
	return 0;

    if (unget != -1) {
	c = unget;
	unget = 0;
	return c;
    }	    

    if (lastChar) {
	setErrorString("Input overrun error");
	return 0;
    }

    c = *cp++;
    charNumber++;
    if (c == kNewLine) {
	lineNumber++;
	charNumber = 0;
    }
    else if (!c)
	lastChar = true;

    return c;
}

/****************************************************************************
 *									    *
 * Start of parser, first section is the multi char token parsers.	    *
 *									    *
 ***************************************************************************/

/*
 *  Number	    ==> [1-9][0-9]+
 *		      | 0[0-7]+
 *		      | 0x[0-9a-fA-F]+
 */
unsigned long parseNumber(char d, lexxer &lex)
{
    unsigned long num = 0;

    if ( !isDecNumeric(d) ) {
	lex.setErrorString("Don't recognise character in number");
	return 0;
    }

    if (d != '0') { /* Parse decimal number */
	lex.ungetChar(d);
	while ( (d = lex.getLiteralChar()) && isDecNumeric(d) )
	    num = num * 10 + d - '0';
    }
    else {
	d = lex.getChar();
	if (d != 'x') { /* Parse Octal Number */
	    lex.ungetChar(d);
	    while ( (d = lex.getLiteralChar()) && isOctNumeric(d) )
		num = num * 8 + d - '0';
	}
	else {	/* Parse Hex Number */
	    int nibble;
	    while ( (d = lex.getLiteralChar()) )
		if (c >= '0' && c <= '9')
		    nibble = c - '0';
		else if (c >= 'a' && c <= 'f')
		    nibble = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
		    nibble = c - 'A' + 10;
		else
		    break;	
		}
		num = num * 16 + nibble;
	    }
	}
    }
    lex.ungetChar(d);

    return num;
}

/*
 *  String	    ==> Alpha CharacterList
 *		      | ' CharacterList '
 *		      | " CharacterList "
 *
 *  CharacterList   ==> EMPTY
 *		      | AnyPrintableCharacter CharacterList
 *
 *  Alpha	    ==> [a-zA-Z]
 */
static IOObject *parseString(char d, lexxer &lex)
{
    IOString *str = 0;
    char endChar = 0;
    char *startp = lex.getCurrentCharPointer();
    char *endp;
    int len;

    if (d == kSingleQut || d == kDoubleQut) {
	len = 0;
	endChar = d;
    }
    else if ( !isAlpha(d) ) {
	lex.setErrorString("Don't recognise character in string");
	return 0;
    }
    else {
	startp--;
	len = 1;
    }

    while ( (d = lex.getLiteralChar()) ) {
	if (d == endChar)
	    break;

	len++;
	if (d == kBackSlash) {
	    if ( !lex.getLiteralChar() )
		break;
	}
	else if (!endChar) {
	    if (d == kEndBrace	|| d == kEndSquare || d == kEndParen
	    ||	d == kNewLine	|| d == kHash	   || d == kEqualSign
	    ||	d == kSemiColon || d == kComma) {
		endp = lex.getCurrentCharPointer() - 1;
		lex.ungetChar(d);
		break;
	    }
	}
	else if (d == kNewLine) {
	    lex.setErrorString("Unescaped new line in a string");
	    return 0;
	}
    }

    /* Remove trailing white space */
    if (d != endChar) {
	while (*endp == kSpace) {
	    len--;
	    endp--;
	}
    }

    IOData *data = IOData::withCapacity(len + 1);
    if (!data) {
	lex.setErrorString("Can not create space for new string - no memory");
	return 0;
    }
    else {
	char *cp = (char *) data->getBytesNoCopy();
	while (len--) {
	    d = *startp++;
	    if (d == kBackSlash)
		d = *startp++;
	    *cp++ = d;
	}
	*cp++ = 0;
	
	IOString *str = IOString::withCString(data->getBytesNoCopy());
	data->release();
	if (!str) {
	    lex.setErrorString("Can not create new string - no memory");
	    return 0;
	}
	
	return str;
    }
}

/****************************************************************************
 *									    *
 * Class parsers with generic object parser forward declaration.	    *
 *									    *
 ***************************************************************************/

static IOObject *parseObject(char d, lexxer &lex);

/*
 *  Dictionary	    ==> { KeyValueList }
 *
 *  KeyValueList    ==> EMPTY
 *		      | KeyValue ; KeyValueList
 *
 *  KeyValue	    ==> String = Object
 */
static IOObject *parseDictionary(char d, lexxer &lex)
{
    if (d != kBegBrace) {
	lex.setErrorString("Expected '{' for dictionary");
	return 0;
    }

    IODictionary *dict = IODictionary::withCapacity(1);
    while ((c = lex.getChar()) && c != kEndBrace) {
	if (lex.hasError())
	    break;

	/* Parse the key first */
	IOString *str = parseString(c, lex);
	if (!str)
	    break;

	/* Did we get an '=' value seperator */
	if (lex.getChar() != kEqualSign) {
	    lex.setErrorString("Expected '=' in dictionary key value pair");
	    break;
	}

	/* Now parse the object */
	IOObject *obj = parseObject(lex.getChar(), lex);
	if (!obj)
	    break;

	/* We have a key value so add it do dictionary */
	IOObject *dictObj = dict->setObject(obj, str);
	str->release();
	obj->release();
	if (!dictObj) {
	    lex.setErrorString("Can not add key value to dictionary - no memory");
	    break;
	}
	if (dictObj != obj)
	    dictObj->release(); /* Get rid of old entry */

	/* Finally chew the keyvalue seperator */
	if ( (c = lex.getChar()) != kSemiColon) {
	    if (!c || c == kEndBrace)
		lex.ungetChar(c);
	    else {
		lex.setErrorString("Dictionary expected a ';'");
		break;
	    }
	}
    }

    if (!c)
	lex.setErrorString("Unexpected end of input of dictionary");

    if (lex.hasError() && dict) {
	dict->release();
	dict = 0;
    }

    return dict;
}

/*
 *  Set		    ==> [ ObjectList ]
 *
 *  ObjectList	    ==> EMPTY
 *		      | Object , ObjectList
 */
static IOObject *parseSet(char d, lexxer &lex)
{
    if (d != kBegSquare) {
	lex.setErrorString("Expected '[' for set");
	return 0;
    }

    IOSet *set = IOSet::withCapacity(1);
    if (!set) {
	lex.setErrorString("Can not create new set - no memory");
	return 0;
    }

    while ((c = lex.getChar()) && c != kEndSquare) {
	if (lex.hasError())
	    break;

	IOObject *obj = parseObject(c, lex);
	if (!obj)
	    break;

	bool res = set->setObject(obj);
	obj->release();
	if (!res) {
	    lex.setErrorString("Can not add object to set - no memory");
	    break;
	}

	/* Finally chew the object seperator */
	if ( (c = lex.getChar()) != kComma) {
	    if (!c || c == kEndSquare)
		lex.ungetChar(c);
	    else {
		lex.setErrorString("Set expected a ','");
		break;
	    }
	}
    }

    if (!c)
	lex.setErrorString("Unexpected end of input of set");

    if (lex.hasError() && set) {
	set->release();
	set = 0;
    }

    return set;
}

/*
 *  Array	    ==> ( ObjectList )
 *
 *  ObjectList	    ==> EMPTY
 *		      | Object , ObjectList
 */
static IOObject *parseArray(char d, lexxer &lex)
{
    if (d != kBegParen) {
	lex.setErrorString("Expected '(' for array");
	return 0;
    }

    IOArray *array = IOArray::withCapacity(1);
    if (!array) {
	lex.setErrorString("Can not create new array - no memory");
	return 0;
    }

    while ((c = lex.getChar()) && c != kEndParen) {
	if (lex.hasError())
	    break;

	IOObject *obj = parseObject(c, lex);
	if (!obj)
	    break;

	bool res = array->setObject(obj);
	obj->release();
	if (!res) {
	    lex.setErrorString("Can not add object to array - no memory");
	    break;
	}

	/* Finally chew the keyvalue seperator */
	if ( (c = lex.getChar()) != kComma) {
	    if (!c || c == kEndParen)
		lex.ungetChar(c);
	    else {
		lex.setErrorString("Array expected a ','");
		break;
	    }
	}
    }

    if (!c)
	lex.setErrorString("Unexpected end of input of array");

    if (lex.hasError() && array) {
	array->release();
	array = 0;
    }

    return array;
}

/*
 *  Data	    ==> < ByteList >
 *
 *  ByteList	    ==> EMPTY
 *		      | Byte ByteList
 *
 *  Byte	    ==> [0-9a-fA-F][0-9a-fA-F]
 */
static IOObject *parseData(char d, lexxer &lex)
{
    IOData *data;
    unsigned char buf[16], *bp;
    int byte, nibble;

    if (d != kBegAngle) {
	lex.setErrorString("Expected '<' for data");
	return 0;
    }

    data = IOData::withCapacity(1);
    if (!data) {
	lex.setErrorString("Can not create new data - no memory");
	return 0;
    }

    endp = &buf[16];
    byte = -1;
    for (;;) {

	/* Get first nibble */
	c = lex.getChar();
	if (c >= '0' && c <= '9')
	    nibble = c - '0';
	else if (c >= 'a' && c <= 'f')
	    nibble = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    nibble = c - 'A' + 10;
	else
	    break;	
	byte = nibble << 4;

	/* Get second nibble */
	c = lex.getChar();
	if (c >= '0' && c <= '9')
	    nibble = c - '0';
	else if (c >= 'a' && c <= 'f')
	    nibble = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
	    nibble = c - 'A' + 10;
	else
	    break;	
	byte |= nibble;

	*bp++ = byte;
	byte = -1;
	if (bp >= &buf[16]) {
	    IOData *tmp = data;
	    
	    data = IOData::withCapacity(tmp->getLength() + 16);
	    if (!data) {
		tmp->release();
		lex.setErrorString("Can not create new data - no memory");
		return 0;
	    }
	    data->appendBytes(tmp);
	    data->appendBytes(buf, 16);
	    tmp->release();
	}
    }

    if (!c || c != kEndAngle)
	lex.setErrorString("Unrecognised character in data");
    else {
	IOData *tmp = data;

	if (byte != -1)
	    *bp++ = byte;
	    
	data = IOData::withCapacity(tmp->getLength() + 16);
	if (!data) {
	    tmp->release();
	    lex.setErrorString("Can not create new data - no memory");
	    return 0;
	}
	data->appendBytes(tmp);
	data->appendBytes(buf, bp - buf);
	tmp->release();
    }

    if (lex.hasError()) && data) {
	data->release();
	data = 0;
    }

    return data;
}

/*
 *  Offset	    ==> OffsetOrRange
 *		      | OffsetOrRange : Number
 *
 *  Range	    ==> OffsetOrRange - Number
 *
 *  OffsetOrRange   ==> Number
 */
static IOObject *parseOffsetOrRange(char d, lexxer &lex)
{
    char c;
    unsigned long first = parseNumber(d, lex);

    if ( lex.hasError() )
	return 0;

    if ( !(c = lex.getChar() ) {
	lex.setErrorString("Unexpected end of input in number");
	return 0;
    }
    if (c == kHyphen) {
	unsigned long second = parseNumber(lex.getChar(), lex);
	if (lex.hasError())
	    return 0;

	IOIntRange *range = IOIntRange::fromStartToEnd(first, second);
	if (!range)
	    lex.setErrorString("Can not create new int range - no memory");
	lex.ungetChar();    /* put terminator back on input */
	return range;
    }
    else {
	IOOffset *offset;
	if (c != kColon)
	    offset = IOOffset::offset(first, 32);
	else {
	    unsigned long long value = first;
	    unsigned long second = parseNumber(lex.getChar(), lex);
	    if (lex.hasError())
		return 0;

	    offset = IOOffset::offset(((value << 32) | second), 64);
	}
	if (!offset)
	    lex.setErrorString("Can not create new number - no memory");
	lex.ungetChar();    /* put terminator back on input */
	return offset;
    }
}

/*
 *  Object	    ==> Dictionary
 *		      | Set
 *		      | Array
 *		      | String
 *		      | Data
 *		      | Offset
 *		      | Range
 *
 * The table below describes the directors used to determine which parser
 * is to be started next given the current character.
 *
 * Object	    ==> { KeyValueList }	    # Dictionary
 *		      | [ ObjectList ]		    # Set
 *		      | ( ObjectList )		    # Array
 *		      | Alpha CharacterList	    # String
 *		      | ' CharacterList '	    # String
 *		      | " CharacterList "	    # String
 *		      | < ByteList >		    # Data
 *		      | numeric			    # Offset or Range
 */
static IOObject *parseObject(char d, lexxer &lex)
{
    IOObject *obj = 0;

    switch (d) {
    case kBegBrace  :	obj = parseDictionary(d, lex);	break;
    case kBegSquare :	obj = parseSet(d, lex);		break;
    case kBegParen  :	obj = parseArray(d, lex);	break;
    case kBegAngle  :	obj = parseData(d, lex);	break;

    case '0' : case '1' : case '2' : case '3' : case '4' : 
    case '5' : case '6' : case '7' : case '8' : case '9' :
	obj = parseOffsetOrRange(d, lex);	break;

    default  :
	if ( isAlpha(d) || d == kSingleQut || d == kDoubleQut )
	    obj = parseString(d, lex);
	else
	    lex.setErrorString("Unexpected character");
	break;
    }

    if (lex.hasError() && obj) {
	obj->release();
	obj = 0;
    }

    return obj;
}

IOObject*
IOUnserialize(const char *buffer, IOString **errorString)
{
    lexxer lex(buffer);

    IOObject *obj = parseObject(lex.getChar(), lex);
    if (lex.hasError())
	sprintf(errorString, "Line %d, Character %d: %s\n",
	    lex.getLineNumber(), lex.getCharNumber(), lex.getErrorString());

    return obj;
}

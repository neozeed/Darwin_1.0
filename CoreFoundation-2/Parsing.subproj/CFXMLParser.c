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
/*	CFXMLParser.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Becky Willrich
*/

#include <CoreFoundation/CFXMLParser.h>
#include "CFXMLInputStream.h"
#include "CFUniChar.h" 
#include "CFInternal.h"
#include <string.h>

struct __CFXMLParser {
    CFRuntimeBase _cfBase;

    _CFXMLInputStream *input;
    
    void **stack;
    void **top;
    UInt32 capacity;

    CFAllocatorRef alloc;
    CFXMLDataTypeDescription desc; // desc.dataString created via CFStringCreateMutableWithExternalCharactersNoCopy
    CFMutableDictionaryRef argDict;
    CFMutableArrayRef argArray;

    CFURLRef dataSource;
    CFStringEncoding encoding; // Annoying, but we must hold this long enough to report the top-most element
    UInt32 options;
    CFXMLParserCallBacks callBacks;
    void *context;

    SInt32 status;
    CFStringRef errorString;
};



CFTypeID CFXMLParserGetTypeID(void) {
    return __kCFXMLParserTypeID;
}

Boolean __CFXMLParserEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFXMLParser *parser1 = cf1;
    const struct __CFXMLParser *parser2 = cf2;
    return (parser1 == parser2);
}

CFHashCode __CFXMLParserHash(CFTypeRef cf) {
    CFXMLParserRef parser = (CFXMLParserRef)cf;
    return (CFHashCode)parser;
}

CFStringRef __CFXMLParserCopyDescription(CFTypeRef cf) {
    const struct __CFXMLParser *parser = cf;
    return CFStringCreateWithFormat(parser->alloc, NULL, CFSTR("<CFXMLParser 0x%x>"), parser);
}

CFAllocatorRef __CFXMLParserGetAllocator(CFTypeRef cf) {
    const struct __CFXMLParser *parser = cf;
    return parser->alloc;
}

void __CFXMLParserDeallocate(CFTypeRef cf) {
    const struct __CFXMLParser *parser = cf;
    CFAllocatorRef alloc = parser->alloc;
    if (parser->input) _freeInputStream(parser->input, alloc);
    if (parser->desc.dataString) CFRelease(parser->desc.dataString);
    if (parser->argDict) CFRelease(parser->argDict);
    if (parser->argArray) CFRelease(parser->argArray);
    if (parser->errorString) CFRelease(parser->errorString);
    if (parser->dataSource) CFRelease(parser->dataSource);
    CFAllocatorDeallocate(alloc, parser->stack);
    CFAllocatorDeallocate(alloc, (void *)parser);
    CFRelease(alloc);
}

void CFXMLParserSetContext(CFXMLParserRef parser, void *context) {
    CFAssert1(parser != NULL, __kCFLogAssertion, "%s(): NULL parser not permitted", __PRETTY_FUNCTION__);
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    parser->context = context;
}

void *CFXMLParserGetContext(CFXMLParserRef parser) {
    CFAssert1(parser != NULL, __kCFLogAssertion, "%s(): NULL parser not permitted", __PRETTY_FUNCTION__);
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    return parser->context;
}

void CFXMLParserSetCallBacks(CFXMLParserRef parser, CFXMLParserCallBacks *callBacks) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    CFAssert1(callBacks->createXMLStructure != NULL && callBacks->endXMLStructure != NULL && callBacks->addChild != NULL, __kCFLogAssertion, "%s(): callBacks must have non-NULL createXMLStructure, endXMLStructure, and addChild functions", __PRETTY_FUNCTION__);
    parser->callBacks = *callBacks;
}

void CFXMLParserGetCallBacks(CFXMLParserRef parser, CFXMLParserCallBacks *callBacks) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    if (callBacks) *callBacks = parser->callBacks;
}

CFURLRef CFXMLParserGetSourceURL(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    return parser->dataSource;
}

/* Returns the character index or line number of the current parse location */
CFIndex CFXMLParserGetLocation(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    if (!parser->input) {
        return 1;
    } else {
        return _inputStreamCurrentLocation(parser->input);
    }
}

CFIndex CFXMLParserGetLineNumber(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    if (!parser->input) {
        return 1;
    } else {
        return _inputStreamCurrentLine(parser->input);
    }
}

/* Returns the top-most object returned by the createXMLStructure callback */
void *CFXMLParserGetDocument(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    if (parser->capacity > 0)
        return parser->stack[0];
    else
        return NULL;
}

SInt32 CFXMLParserGetStatusCode(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    return parser->status;
}

CFStringRef CFXMLParserCopyErrorDescription(CFXMLParserRef parser) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    return CFRetain(parser->errorString);
}

void CFXMLParserAbort(CFXMLParserRef parser, SInt32 errorCode, CFStringRef errorDescription) {
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    CFAssert1(errorCode > 0, __kCFLogAssertion, "%s(): errorCode must be greater than zero", __PRETTY_FUNCTION__);
    CFAssert1(errorDescription != NULL, __kCFLogAssertion, "%s(): errorDescription may not be NULL", __PRETTY_FUNCTION__);
    __CFGenericValidateType(errorDescription, __kCFStringTypeID);

    parser->status = errorCode;
    if (parser->errorString) CFRelease(parser->errorString);
    parser->errorString = CFStringCreateCopy(NULL, errorDescription);
}


static Boolean parseXML(CFXMLParserRef pInfo);
static Boolean parseComment(CFXMLParserRef pInfo, Boolean report);
static Boolean parseProcessingInstruction(CFXMLParserRef pInfo, Boolean report);
static Boolean parseInlineDTD(CFXMLParserRef pInfo);
static Boolean parseDTD(CFXMLParserRef pInfo);
static Boolean parsePhysicalEntityReference(CFXMLParserRef pInfo);
static Boolean parseCDSect(CFXMLParserRef pInfo);
static Boolean parseEntityReference(CFXMLParserRef pInfo, Boolean report);
static Boolean parsePCData(CFXMLParserRef pInfo);
static Boolean parseWhitespace(CFXMLParserRef pInfo);
static Boolean parseAttributeListDeclaration(CFXMLParserRef pInfo);
static Boolean parseNotationDeclaration(CFXMLParserRef pInfo);
static Boolean parseElementDeclaration(CFXMLParserRef pInfo);
static Boolean parseEntityDeclaration(CFXMLParserRef pInfo);
static Boolean parseExternalID(CFXMLParserRef pInfo, Boolean alsoAcceptPublicID, CFXMLExternalID *extID);
static Boolean parseCloseTag(CFXMLParserRef pInfo, UniChar *tag, UInt32 tagLen);
static Boolean parseTagContent(CFXMLParserRef pInfo);
static Boolean parseTag(CFXMLParserRef pInfo);
static Boolean parseAttributes(CFXMLParserRef pInfo);
static Boolean parseAttributeValue(CFXMLParserRef pInfo, CFMutableStringRef str);


// Utilities; may need to make these accessible to the property list parser to avoid code duplication
static CFStringEncoding encodingForXMLData(CFDataRef data);
static void _CFReportError(CFXMLParserRef pInfo, SInt32 errNum, const char *str);
static Boolean reportNewLeaf(CFXMLParserRef pInfo); // Assumes pInfo->desc has been set and is ready to go
static void pushXMLNode(CFXMLParserRef pInfo, void *node);

static CFXMLParserRef __CFXMLParserInit(CFAllocatorRef alloc, CFURLRef dataSource, UInt32 options, CFStringRef xmlString) {
    struct __CFXMLParser *pInfo;
    alloc = (NULL == alloc) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(alloc);
    if (xmlString) {
        UniChar *characters;
        CFIndex length = CFStringGetLength(xmlString);
        pInfo = CFAllocatorAllocate(alloc, sizeof(struct __CFXMLParser) + sizeof(_CFXMLInputStream) + length*sizeof(UniChar), 0);
        __CFGenericInitBase((void *)pInfo, NULL, __kCFXMLParserTypeID);
        pInfo->input = (_CFXMLInputStream *)((UInt8 *)pInfo + sizeof(struct __CFXMLParser));
        characters = (UniChar *)(((UInt8 *)pInfo) + sizeof(struct __CFXMLParser) + sizeof(_CFXMLInputStream));
        CFStringGetCharacters(xmlString, CFRangeMake(0, length), characters);
        _inputStringInitialize(pInfo->input, characters, length);
    } else {
        pInfo = CFAllocatorAllocate(alloc, sizeof(struct __CFXMLParser), 0);
        __CFGenericInitBase((void *)pInfo, NULL, __kCFXMLParserTypeID);
        pInfo->input = NULL;
    }

    pInfo->alloc = alloc;

    pInfo->top = pInfo->stack;
    pInfo->stack = NULL;
    pInfo->capacity = 0;

    pInfo->desc.dataString = xmlString ? CFStringCreateMutableWithExternalCharactersNoCopy(alloc, NULL, 0, 0, kCFAllocatorNull) : NULL;
    pInfo->desc.additionalData = NULL;
    pInfo->argDict = NULL; // don't create these until necessary
    pInfo->argArray = NULL;

    pInfo->dataSource = dataSource ? CFRetain(dataSource) : NULL;
    pInfo->options = options;

    pInfo->callBacks.createXMLStructure = NULL;
    pInfo->callBacks.addChild = NULL;
    pInfo->callBacks.endXMLStructure = NULL;
    pInfo->callBacks.resolveExternalEntity = NULL;
    pInfo->callBacks.handleError = NULL;

    pInfo->context = NULL;
    pInfo->status = kCFXMLStatusParseNotBegun;
    pInfo->errorString = NULL;

    return pInfo;
}

CFXMLParserRef CFXMLParserCreate(CFAllocatorRef allocator, CFDataRef xmlData, CFURLRef dataSource, UInt32 options) {
    CFStringEncoding encoding;
    CFStringRef xmlString;
    struct __CFXMLParser *parser;
    CFAssert1(xmlData != NULL, __kCFLogAssertion, "%s(): NULL data not permitted", __PRETTY_FUNCTION__);
    __CFGenericValidateType(xmlData, __kCFDataTypeID);
    CFAssert1(dataSource == NULL || CFGetTypeID(dataSource) == __kCFURLTypeID, __kCFLogAssertion, "%s(): dataSource is not a valid CFURL", __PRETTY_FUNCTION__);
              
    encoding = encodingForXMLData(xmlData);
    if (encoding == kCFStringEncodingInvalidId) {
        parser = __CFXMLParserInit(allocator, dataSource, options, NULL);
        parser->encoding = encoding;
        parser->status = kCFXMLErrorUnknownEncoding;
        parser->errorString = CFStringCreateWithCString(parser->alloc, "Encountered unknown encoding", kCFStringEncodingASCII);
        return parser;
    }

    xmlString = CFStringCreateFromExternalRepresentation(allocator, xmlData, encoding);
    if (!xmlString) {
        parser = __CFXMLParserInit(allocator, dataSource, options, NULL);
        parser->encoding = encoding;
        parser->status = kCFXMLErrorEncodingConversionFailure;
        parser->errorString = CFStringCreateWithFormat(parser->alloc, NULL, CFSTR("String encoding %d not implemented; could not translate data"), encoding);
        return parser;
    }

    parser = __CFXMLParserInit(allocator, dataSource, options, xmlString);
    CFRelease(xmlString);
    parser->encoding = encoding;
    return parser;
}

Boolean CFXMLParserParse(CFXMLParserRef parser) {
    CFXMLDocumentData docData;
    __CFGenericValidateType(parser, __kCFXMLParserTypeID);
    CFAssert1(parser->callBacks.createXMLStructure != NULL && parser->callBacks.addChild != NULL && parser->callBacks.endXMLStructure != NULL, __kCFLogAssertion, "%s(): must set valid callbacks before beginning a parse", __PRETTY_FUNCTION__);
    if (parser->status != kCFXMLStatusParseNotBegun) return FALSE;
    parser->status = kCFXMLStatusParseInProgress;

    // Create the document
    parser->stack = CFAllocatorAllocate(parser->alloc, 16 * sizeof(void *), 0);
    parser->capacity = 16;
    parser->desc.dataTypeID = kCFXMLDataTypeDocument;
    docData.sourceURL = parser->dataSource;
    docData.encoding = parser->encoding;
    parser->desc.additionalData = &docData;
    parser->stack[0] = parser->callBacks.createXMLStructure(parser, &parser->desc, parser->context);
    parser->top = parser->stack;

    // Client may have called CFXMLParserAbort() during any callback, so we must always check to see if we have an error status after a callback
    if (parser->status != kCFXMLStatusParseInProgress) {
        _CFReportError(parser, parser->status, NULL);
        return FALSE;
    }
    return parseXML(parser);
}

/* The next several functions are all intended to parse past a particular XML structure.  They expect pInfo->curr to be set to the first content character of their structure (e.g. parseXMLComment expects pInfo->curr to be set just past "<!--").  They parse to the end of their structure, calling any necessary callbacks along the way, and advancing pInfo->curr as they go.  They either return void (not possible for the parse to fail) or they return a Boolean (success/failure).  The calling routines are expected to catch returned Booleans and fail immediately if FALSE is returned. */

// [3]  S ::= (#x20 | #x9 | #xD | #xA)+ 
static Boolean parseWhitespace(CFXMLParserRef pInfo) {
    CFIndex len;
    Boolean report = !(pInfo->options & kCFXMLParserSkipWhitespace);
    len = _inputStreamSkipWhitespace(pInfo->input, report ? (CFMutableStringRef)(pInfo->desc.dataString) : NULL);
    if (report && len) {
        pInfo->desc.dataTypeID = kCFXMLDataTypeWhitespace;
        pInfo->desc.additionalData = NULL;
        return reportNewLeaf(pInfo);
    } else {
        return TRUE;
    }
}

// pInfo should be just past "<!--"
static Boolean parseComment(CFXMLParserRef pInfo, Boolean report) {
    const UniChar dashes[2] = {'-', '-'};
    UniChar ch;
    report = report && (!(pInfo->options & kCFXMLParserSkipMetaData));
    if (!_inputStreamScanToCharacters(pInfo->input, dashes, 2, report ? (CFMutableStringRef)(pInfo->desc.dataString) : NULL) || !_inputStreamGetCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF,"Found unexpected EOF while parsing comment");
        return FALSE;
    } else if (ch != '>') {
        _CFReportError(pInfo, kCFXMLErrorMalformedComment, "Found \"--\" within a comment");
        return FALSE;
    } else if (report) {
        pInfo->desc.dataTypeID = kCFXMLDataTypeComment;
        pInfo->desc.additionalData = NULL;
        return reportNewLeaf(pInfo);
    } else {
        return TRUE;
    }
}

/* 
[16] PI ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
[17] PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))
 */
// pInfo should be set to the first character after "<?"
static Boolean parseProcessingInstruction(CFXMLParserRef pInfo, Boolean report) {
    UniChar piTermination[2] = {'?', '>'};
    CFMutableStringRef str;

    if (!_inputStreamScanXMLName(pInfo->input, (CFMutableStringRef)(pInfo->desc.dataString), FALSE)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedProcessingInstruction, "Found malformed processing instruction");
        return FALSE;
    }
    _inputStreamSkipWhitespace(pInfo->input, NULL);
    str = (report && *pInfo->top) ? CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull) : NULL;
    if (!_inputStreamScanToCharacters(pInfo->input, piTermination, 2, str)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing processing instruction");
        if (str) CFRelease(str);
        return FALSE;
    }

    if (str) {
        CFXMLProcessingInstructionData data;
        Boolean result;
        pInfo->desc.dataTypeID = kCFXMLDataTypeProcessingInstruction;
        data.dataString = str;
        pInfo->desc.additionalData = &data;
        result = reportNewLeaf(pInfo);
        CFRelease(str);
        return result;
    } else {
        return TRUE;
    }
}

/*
 [28] doctypedecl ::= '<!DOCTYPE' S Name (S ExternalID)? S? ('[' (markupdecl | PEReference | S)* ']' S?)? '>'
*/
static UniChar _DoctypeOpening[7] = {'D', 'O', 'C', 'T', 'Y', 'P', 'E'};
// first character should be immediately after the "<!"
static Boolean parseDTD(CFXMLParserRef pInfo) {
    UniChar ch;
    Boolean success, hasExtID = FALSE;
    CFXMLExternalID extID = {NULL, NULL};
    void *dtdStructure = NULL;
    
    // First pass "DOCTYPE"
    success = _inputStreamMatchString(pInfo->input, _DoctypeOpening, 7) && _inputStreamSkipWhitespace(pInfo->input, NULL) != 0 && _inputStreamScanXMLName(pInfo->input, (CFMutableStringRef)pInfo->desc.dataString, FALSE);
    if (success) {
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        success = _inputStreamPeekCharacter(pInfo->input, &ch);
    }
    if (success && ch != '[' && ch != '>') {
        // ExternalID
        hasExtID = TRUE;
        success = parseExternalID(pInfo, FALSE, &extID);
        if (success)  {
            _inputStreamSkipWhitespace(pInfo->input, NULL);
            success = _inputStreamPeekCharacter(pInfo->input, &ch);
        }
    }

    if (!(pInfo->options & kCFXMLParserSkipMetaData) && *(pInfo->top)) {
        pInfo->desc.dataTypeID = kCFXMLDataTypeDocumentType;
        pInfo->desc.additionalData = hasExtID ? &extID : NULL;
        dtdStructure = pInfo->callBacks.createXMLStructure(pInfo, &(pInfo->desc), pInfo->context);
        if (dtdStructure && pInfo->status == kCFXMLStatusParseInProgress) {
            pInfo->callBacks.addChild(pInfo, *pInfo->top, dtdStructure, pInfo->context);
        }
        if (pInfo->status != kCFXMLStatusParseInProgress) {
            // callback called CFXMLParserAbort()
            _CFReportError(pInfo, pInfo->status, NULL);
            return FALSE;
        }
    } else {
        dtdStructure = NULL;
    }
    if (extID.publicID) CFRelease(extID.publicID);
    if (extID.systemID) CFRelease(extID.systemID);
    pushXMLNode(pInfo, dtdStructure);

    if (success && ch == '[')  {
        // inline DTD
        _inputStreamGetCharacter(pInfo->input, &ch);
        if (!parseInlineDTD(pInfo)) return FALSE;
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        success = _inputStreamGetCharacter(pInfo->input, &ch) && ch == '>';
    } else if (success && ch == '>') {
        // End of the DTD
        _inputStreamGetCharacter(pInfo->input, &ch);
    }
    if (!success) {
        if (_inputStreamAtEOF(pInfo->input)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing DTD");
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found malformed DTD");
        }
        return FALSE;
    }

    pInfo->top --; // Remove dtdStructure from the stack

    if (success && dtdStructure) {
        pInfo->callBacks.endXMLStructure(pInfo, dtdStructure, pInfo->context);
        if (pInfo->status != kCFXMLStatusParseInProgress) {
            _CFReportError(pInfo, pInfo->status, NULL);
            return FALSE;
        }
    }
    return TRUE;
}

/*
 [69] PEReference ::= '%' Name ';'
*/
static Boolean parsePhysicalEntityReference(CFXMLParserRef pInfo) {
    UniChar ch;
    if (!_inputStreamScanXMLName(pInfo->input, (CFMutableStringRef)pInfo->desc.dataString, FALSE)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedName, "Found malformed name while parsing physical entity reference");
        return FALSE;
    } else if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing physical entity reference");
        return FALSE;
    } else if (ch != ';') {
        _CFReportError(pInfo, kCFXMLErrorMalformedName, "Found malformed name while parsing physical entity reference");
        return FALSE;
    } else if (!(pInfo->options & kCFXMLParserSkipMetaData) && *(pInfo->top)) {
        CFXMLEntityTypeID myID = kCFXMLEntityTypeParameter;
        pInfo->desc.additionalData = &myID;
        pInfo->desc.dataTypeID = kCFXMLDataTypeEntityReference;
        return reportNewLeaf(pInfo);
    } else {
        return TRUE;
    }
}

/*
 [54] AttType ::= StringType | TokenizedType | EnumeratedType
 [55] StringType ::= 'CDATA'
 [56] TokenizedType ::= 'ID' | 'IDREF'| 'IDREFS'| 'ENTITY'| 'ENTITIES'| 'NMTOKEN'| 'NMTOKENS'
 [57] EnumeratedType ::= NotationType | Enumeration
 [58] NotationType ::= 'NOTATION' S '(' S? Name (S? '|' S? Name)* S? ')'
 [59] Enumeration ::= '(' S? Nmtoken (S? '|' S? Nmtoken)* S? ')'
*/
static Boolean parseEnumeration(CFXMLParserRef pInfo, Boolean useNMTokens) {
    UniChar ch;
    Boolean done = FALSE;
    if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
        return FALSE;
    } else if (ch != '(') {
        _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        return FALSE;
    }
    _inputStreamSkipWhitespace(pInfo->input, NULL);
    if (!_inputStreamScanXMLName(pInfo->input, NULL, useNMTokens)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        return FALSE;
    }
    while (!done) {
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
            return FALSE;
        } else if (ch == ')') {
            done = TRUE;
        } else if (ch == '|') {
            _inputStreamSkipWhitespace(pInfo->input, NULL);
            if (!_inputStreamScanXMLName(pInfo->input, NULL, useNMTokens)) {
                _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
                return FALSE;
            }
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            return FALSE;
        }
    }
    return TRUE;
}

static Boolean parseAttributeType(CFXMLParserRef pInfo, CFMutableStringRef str) {
    Boolean success = FALSE;
    static UniChar attTypeStrings[6][8] = {
    {'C', 'D', 'A', 'T', 'A', '\0', '\0', '\0'},
    {'I', 'D', 'R', 'E', 'F', 'S', '\0', '\0'},
    {'E', 'N', 'T', 'I', 'T', 'Y', '\0', '\0'},
    {'E', 'N', 'T', 'I', 'T', 'I', 'E', 'S'},
    {'N', 'M', 'T', 'O', 'K', 'E', 'N', 'S'},
    {'N', 'O', 'T', 'A', 'T', 'I', 'O', 'N'} };
    if (str) _inputStreamSetMark(pInfo->input);
    if (_inputStreamMatchString(pInfo->input, attTypeStrings[0], 5) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[1], 6) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[1], 5) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[1], 2) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[2], 6) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[3], 8) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[4], 8) ||
        _inputStreamMatchString(pInfo->input, attTypeStrings[4], 7)) {
        success = TRUE;
    } else if (_inputStreamMatchString(pInfo->input, attTypeStrings[5], 8)) {
        // Notation
        if (_inputStreamSkipWhitespace(pInfo->input, NULL) == 0) {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            success = FALSE;
        } else  {
            success = parseEnumeration(pInfo, FALSE);
        }
    } else {
        success = parseEnumeration(pInfo, TRUE);
    }
    if (success && str) {
        _inputStreamGetCharactersFromMark(pInfo->input, str);
    }
    return success;
}

/*  [60] DefaultDecl ::= '#REQUIRED' | '#IMPLIED' | (('#FIXED' S)? AttValue) */
static Boolean parseAttributeDefaultDeclaration(CFXMLParserRef pInfo, CFMutableStringRef str) {
    UniChar strings[3][8] = {{'R', 'E', 'Q', 'U', 'I', 'R', 'E', 'D'}, {'I', 'M', 'P', 'L', 'I', 'E', 'D', '\0'}, {'F', 'I', 'X', 'E', 'D', '\0', '\0', '\0'}};
    UniChar ch;
    Boolean success;
    if (str) _inputStreamSetMark(pInfo->input);
    if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
        success = FALSE;
    } else if (ch == '#') {
        if (_inputStreamMatchString(pInfo->input, strings[0], 8) ||
            _inputStreamMatchString(pInfo->input, strings[1], 7)) {
            success = TRUE;
        } else if (!_inputStreamMatchString(pInfo->input, strings[2], 5) || _inputStreamSkipWhitespace(pInfo->input, NULL) == 0) {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            success = FALSE;
        } else {
            // we fall through if "#FIXED" was matched, and at least one whitespace character was stripped.
            success = parseAttributeValue(pInfo, NULL);
        }
    } else {
        _inputStreamReturnCharacter(pInfo->input, ch);
        success = parseAttributeValue(pInfo, NULL);
    }
    if (success && str) {
        _inputStreamGetCharactersFromMark(pInfo->input, str);
    }
    return success;
}

/*
 [52] AttlistDecl ::= '<!ATTLIST' S Name AttDef* S? '>'
 [53] AttDef ::= S Name S AttType S DefaultDecl
*/
static Boolean parseAttributeListDeclaration(CFXMLParserRef pInfo) {
    UniChar attList[7] = {'A', 'T', 'T', 'L', 'I', 'S', 'T'};
    CFXMLAttributeListDeclarationData attListData;
    CFXMLAttributeDeclarationData attributeArray[8], *attributes=attributeArray;
    CFIndex capacity = 8;
    UniChar ch;
    Boolean success = TRUE;
    if (!_inputStreamMatchString(pInfo->input, attList, 7) || _inputStreamSkipWhitespace(pInfo->input, NULL) == 0
        || !_inputStreamScanXMLName(pInfo->input, (CFMutableStringRef)pInfo->desc.dataString, FALSE)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        return FALSE;
    }
    attListData.numberOfAttributes = 0;
    if (!(*pInfo->top) || (pInfo->options & kCFXMLParserSkipMetaData)) {
        // Use this to mark that we don't need to collect attribute information to report to the client.  Ultimately, we may want to collect this for our own use (for validation, for instance), but for now, the only reason we would create it would be for the client.  -- REW, 2/9/2000
        attributes = NULL;
    }
    while (_inputStreamPeekCharacter(pInfo->input, &ch) && ch != '>' && _inputStreamSkipWhitespace(pInfo->input, NULL) != 0) {
        CFXMLAttributeDeclarationData *attribute = NULL;
        if (_inputStreamPeekCharacter(pInfo->input, &ch) && ch == '>')
            break;
        if (attributes) {
            if (capacity == attListData.numberOfAttributes) {
                capacity = 2*capacity;
                if (attributes != attributeArray) {
                    attributes = CFAllocatorReallocate(pInfo->alloc, attributes, capacity * sizeof(CFXMLAttributeDeclarationData), 0);
                } else {
                    attributes = CFAllocatorAllocate(pInfo->alloc, capacity * sizeof(CFXMLAttributeDeclarationData), 0);
                }
            }
            attribute = &(attributes[attListData.numberOfAttributes]);
            // Much better if we can somehow create these strings immutable - then if the client (or we ourselves) has to copy them, they will end up multiply-retained, rather than having a new alloc and data copy performed.  -- REW, 2/9/2000
            attribute->attributeName = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
            attribute->typeString = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
            attribute->defaultString = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
        }
        if (!_inputStreamScanXMLName(pInfo->input, attribute ? (CFMutableStringRef)attribute->attributeName : NULL, FALSE) || (_inputStreamSkipWhitespace(pInfo->input, NULL) == 0)) {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            success = FALSE;
            break;
        }
        if (!parseAttributeType(pInfo, attribute ? (CFMutableStringRef)attribute->typeString : NULL)) {
            success = FALSE;
            break;
        }
        if (_inputStreamSkipWhitespace(pInfo->input, NULL) == 0) {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            success = FALSE;
            break;
        }
        if (!parseAttributeDefaultDeclaration(pInfo, attribute ? (CFMutableStringRef)attribute->defaultString : NULL)) {
            success = FALSE;
            break;
        }
        attListData.numberOfAttributes ++;
    }
    if (success) {
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
            success = FALSE;
        } else if (ch != '>') {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            success = FALSE;
        } else if (attributes) {
            pInfo->desc.dataTypeID = kCFXMLDataTypeAttributeListDeclaration;
            pInfo->desc.additionalData = (void *)&attListData;
            attListData.attributes = attributes;
            success = reportNewLeaf(pInfo);
        }
    }
    if (attributes) {
        // Free up all that memory
        CFIndex index;
        for (index = 0; index < attListData.numberOfAttributes; index ++) {
            CFRelease(attributes[index].attributeName);
            CFRelease(attributes[index].typeString);
            CFRelease(attributes[index].defaultString);
        }
        if (attributes != attributeArray) {
            CFAllocatorDeallocate(pInfo->alloc, attributes);
        }
    }
    return success;
}

CF_INLINE Boolean parseSystemLiteral(CFXMLParserRef pInfo, CFXMLExternalID *extID) {
    Boolean success;
    if (extID) {
        // Squirrel these away so we can restore pInfo->desc.dataString later.  We need it to temporarily store the URL string - REW, 2/10/2000
        UniChar *oldString = (UniChar *)CFStringGetCharactersPtr((CFMutableStringRef)pInfo->desc.dataString);
        CFIndex oldLength = CFStringGetLength((CFMutableStringRef)pInfo->desc.dataString);
        if (_inputStreamScanQuotedString(pInfo->input, (CFMutableStringRef)pInfo->desc.dataString)) {
            success = TRUE;
            extID->systemID = CFURLCreateWithString(pInfo->alloc, pInfo->desc.dataString, pInfo->dataSource);
        } else {
            extID->systemID = NULL;
            success = FALSE;
        }
        CFStringSetExternalCharactersNoCopy((CFMutableStringRef)pInfo->desc.dataString, oldString, oldLength, oldLength);
    } else {
        success = _inputStreamScanQuotedString(pInfo->input, NULL);
    }
    return success;
}

/*
 [75] ExternalID ::= 'SYSTEM' S SystemLiteral | 'PUBLIC' S PubidLiteral S SystemLiteral
 [83] PublicID ::= 'PUBLIC' S PubidLiteral
 [12] PubidLiteral ::= '"' PubidChar* '"' | "'" (PubidChar - "'")* "'"
 [13] PubidChar ::= #x20 | #xD | #xA | [a-zA-Z0-9] | [-'()+,./:=?;!*#@$_%]
 [11] SystemLiteral ::= ('"' [^"]* '"') | ("'" [^']* "'") 
*/
// This does NOT report errors itself; caller can check to see if pInfo->input is at EOF to determine whether the formatting failed or unexpected EOF occurred.  -- REW, 2/2/2000
static Boolean parseExternalID(CFXMLParserRef pInfo, Boolean alsoAcceptPublicID, CFXMLExternalID *extID) {
    UniChar publicString[6] = {'P', 'U', 'B', 'L', 'I', 'C'};
    UniChar systemString[6] = {'S', 'Y', 'S', 'T', 'E', 'M'};
    Boolean success;
    if (extID) {
        extID->systemID = NULL;
        extID->publicID = NULL;
    }
    if (_inputStreamMatchString(pInfo->input, publicString, 6)) {
        success = _inputStreamSkipWhitespace(pInfo->input, NULL) != 0;
        if (extID) {
            extID->publicID = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
            success = success && _inputStreamScanQuotedString(pInfo->input, (CFMutableStringRef)extID->publicID);
        } else {
            success = success && _inputStreamScanQuotedString(pInfo->input, NULL);
        }
        if (success) {
            UniChar ch;
            if (alsoAcceptPublicID) _inputStreamSetMark(pInfo->input); // In case we need to roll back the parser
            if (_inputStreamSkipWhitespace(pInfo->input, NULL) == 0
                || !_inputStreamPeekCharacter(pInfo->input, &ch)
                || (ch != '\'' && ch != '\"')
                || !parseSystemLiteral(pInfo, extID)) {
                success = alsoAcceptPublicID;
                if (alsoAcceptPublicID) _inputStreamBackUpToMark(pInfo->input);
            } else {
                success = TRUE;
            }
        }
    } else if (_inputStreamMatchString(pInfo->input, systemString, 6)) {
        success = _inputStreamSkipWhitespace(pInfo->input, NULL) != 0 && parseSystemLiteral(pInfo, extID);
    } else {
        success = FALSE;
    }
    return success;
}

/*
 [82] NotationDecl ::= '<!NOTATION' S Name S (ExternalID |  PublicID) S? '>'
*/
static Boolean parseNotationDeclaration(CFXMLParserRef pInfo) {
    UniChar notationString[8] = {'N', 'O', 'T', 'A', 'T', 'I', 'O', 'N'};
    Boolean report = *(pInfo->top) && !(pInfo->options & kCFXMLParserSkipMetaData);
    CFXMLExternalID extID = {NULL, NULL};
    Boolean success =
        _inputStreamMatchString(pInfo->input, notationString, 8) &&
        _inputStreamSkipWhitespace(pInfo->input, NULL) != 0 &&
        _inputStreamScanXMLName(pInfo->input, report ? (CFMutableStringRef)pInfo->desc.dataString : NULL, FALSE) &&
        _inputStreamSkipWhitespace(pInfo->input, NULL) != 0 &&
        parseExternalID(pInfo, TRUE, report ? &extID : NULL);

    if (success) {
        UniChar ch;
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        success = (_inputStreamGetCharacter(pInfo->input, &ch) && ch == '>');
    }
    if (!success) {
        if (_inputStreamAtEOF(pInfo->input)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        }
    } else if (report) {
        pInfo->desc.dataTypeID = kCFXMLDataTypeNotation;
        pInfo->desc.additionalData = &extID;
        success = reportNewLeaf(pInfo);
    }
    if (extID.systemID) CFRelease(extID.systemID);
    if (extID.publicID) CFRelease(extID.publicID);
    return success;
}

/*
 [48] cp ::= (Name | choice | seq) ('?' | '*' | '+')?
 [49] choice ::= '(' S? cp ( S? '|' S? cp )* S? ')'
 [50] seq ::= '(' S? cp ( S? ',' S? cp )* S? ')'
*/
static Boolean parseChoiceOrSequence(CFXMLParserRef pInfo, Boolean pastParen) {
    UniChar ch, separator;
    if (!pastParen) {
        if (!_inputStreamGetCharacter(pInfo->input, &ch) || ch != '(') return FALSE;
        _inputStreamSkipWhitespace(pInfo->input, NULL);
    }
    if (!_inputStreamPeekCharacter(pInfo->input, &ch)) return FALSE;

    /* Now scanning cp, production [48] */
    if (ch == '(') {
        if (!parseChoiceOrSequence(pInfo, FALSE)) return FALSE;
    } else {
        if (!_inputStreamScanXMLName(pInfo->input, NULL, FALSE)) return FALSE;
    }
    if (!_inputStreamPeekCharacter(pInfo->input, &ch)) return FALSE;
    if (ch == '?' || ch == '*' || ch == '+') _inputStreamGetCharacter(pInfo->input, &ch);

    /* Now past cp */
    _inputStreamSkipWhitespace(pInfo->input, NULL);
    if (!_inputStreamGetCharacter(pInfo->input, &ch)) return FALSE;
    if (ch == ')') return TRUE;
    if (ch != '|' && ch != ',') return FALSE;
    separator = ch;
    while (ch == separator) {
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamPeekCharacter(pInfo->input, &ch)) return FALSE;
        if (ch != '(') {
            if (!_inputStreamScanXMLName(pInfo->input, NULL, FALSE)) return FALSE;
        } else if (!parseChoiceOrSequence(pInfo, FALSE)) {
            return FALSE;
        }
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) return FALSE;
    }
    return ch == ')';
}

/*
 [51] Mixed ::= '(' S? '#PCDATA' (S? '|' S? Name)* S? ')*' | '(' S? '#PCDATA' S? ')'
*/
static Boolean parseMixedElementContent(CFXMLParserRef pInfo) {
    static const UniChar pcdataString[7] = {'#', 'P', 'C', 'D', 'A', 'T', 'A'};
    UniChar ch;
    if (!_inputStreamMatchString(pInfo->input, pcdataString, 7)) return FALSE;
    _inputStreamSkipWhitespace(pInfo->input, NULL);
    if (!_inputStreamGetCharacter(pInfo->input, &ch) && (ch == ')' || ch == '|')) return FALSE;
    if (ch == ')') return TRUE;

    while (ch == '|') {
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamScanXMLName(pInfo->input, NULL, FALSE)) return FALSE;
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) return FALSE;
    }
    if (ch != ')') return FALSE;
    if (!_inputStreamGetCharacter(pInfo->input, &ch) || ch != '*') return FALSE;
    return TRUE;
}

/*
 [46] contentspec ::= 'EMPTY' | 'ANY' | Mixed | children
 [47] children ::= (choice | seq) ('?' | '*' | '+')?
 */
static Boolean parseElementContentSpec(CFXMLParserRef pInfo) {
    static UniChar eltContentEmpty[5] = {'E', 'M', 'P', 'T', 'Y'};
    static UniChar eltContentAny[3] = {'A', 'N', 'Y'};
    UniChar ch;
    if (_inputStreamMatchString(pInfo->input, eltContentEmpty, 5) || _inputStreamMatchString(pInfo->input, eltContentAny, 3)) {
        return TRUE;
    } else if (!_inputStreamPeekCharacter(pInfo->input, &ch) || ch != '(') {
        return FALSE;
    } else {
        // We want to know if we have a Mixed per production [51].  If we don't, we will need to back up and call the parseChoiceOrSequence function.  So we set the mark now.  -- REW, 2/10/2000
        _inputStreamGetCharacter(pInfo->input, &ch);
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamPeekCharacter(pInfo->input, &ch)) return FALSE;
        if (ch == '#') {
            // Mixed
            return parseMixedElementContent(pInfo);
        } else {
            if (parseChoiceOrSequence(pInfo, TRUE)) {
                if (_inputStreamPeekCharacter(pInfo->input, &ch) && (ch == '*' || ch == '?' || ch == '+')) {
                    _inputStreamGetCharacter(pInfo->input, &ch);
                }
                return TRUE;
            } else {
                return FALSE;
            }
        }
    }
}

/*
 [45] elementdecl ::= '<!ELEMENT' S Name S contentspec S? '>'
 */
static Boolean parseElementDeclaration(CFXMLParserRef pInfo) {
    Boolean report = *(pInfo->top) && !(pInfo->options & kCFXMLParserSkipMetaData);
    Boolean success;
    static UniChar eltChars[7] = {'E', 'L', 'E', 'M', 'E', 'N', 'T'};
    UniChar ch = '>';
    CFMutableStringRef contentDesc = NULL;
    success = _inputStreamMatchString(pInfo->input, eltChars, 7)
        && _inputStreamSkipWhitespace(pInfo->input, NULL) != 0
        && _inputStreamScanXMLName(pInfo->input, report ? (CFMutableStringRef)pInfo->desc.dataString : NULL, FALSE)
        && _inputStreamSkipWhitespace(pInfo->input, NULL) != 0;
    if (success) {
        if (report) _inputStreamSetMark(pInfo->input);
        success = parseElementContentSpec(pInfo);
        if (success && report) {
            contentDesc = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
            _inputStreamGetCharactersFromMark(pInfo->input, contentDesc);
        }
        if (success) _inputStreamSkipWhitespace(pInfo->input, NULL);
        success = success && _inputStreamMatchString(pInfo->input, &ch, 1);
    }
    if (!success) {
        if (_inputStreamAtEOF(pInfo->input)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        }
    } else if (report) {
        CFXMLElementTypeDeclarationData eltData;
        eltData.contentDescription = contentDesc;
        pInfo->desc.dataTypeID = kCFXMLDataTypeElementTypeDeclaration;
        pInfo->desc.additionalData = &eltData;
        success = reportNewLeaf(pInfo);
    }
    if (contentDesc) CFRelease(contentDesc);
    return success;
}

/*
 [70] EntityDecl ::= GEDecl | PEDecl
 [71] GEDecl ::= '<!ENTITY' S Name S EntityDef S? '>'
 [72] PEDecl ::= '<!ENTITY' S '%' S Name S PEDef S? '>'
 [73] EntityDef ::= EntityValue | (ExternalID NDataDecl?)
 [74] PEDef ::= EntityValue | ExternalID
 [76] NDataDecl ::= S 'NDATA' S Name
 [9]  EntityValue ::= '"' ([^%&"] | PEReference | Reference)* '"' |  "'" ([^%&'] | PEReference | Reference)* "'"
*/
static Boolean parseEntityDeclaration(CFXMLParserRef pInfo) {
    UniChar entityStr[6] = {'E', 'N', 'T', 'I', 'T', 'Y'};
    UniChar ch;
    Boolean isPEDecl = FALSE;
    CFXMLEntityData entityData;
    Boolean report = *(pInfo->top) && !(pInfo->options & kCFXMLParserSkipMetaData);
    Boolean success =
        _inputStreamMatchString(pInfo->input, entityStr, 6) &&
        (_inputStreamSkipWhitespace(pInfo->input, NULL) != 0) &&
        _inputStreamPeekCharacter(pInfo->input, &ch);

    entityData.replacementText = NULL;
    entityData.entityID.publicID = NULL;
    entityData.entityID.systemID = NULL;
    entityData.notationName = NULL;
    // We will set entityType immediately before reporting

    if (success && ch == '%') {
        _inputStreamGetCharacter(pInfo->input, &ch);
        success = _inputStreamSkipWhitespace(pInfo->input, NULL) != 0;
        isPEDecl = TRUE;
    }
    success = success && _inputStreamScanXMLName(pInfo->input, report ? (CFMutableStringRef)pInfo->desc.dataString : NULL, FALSE) && (_inputStreamSkipWhitespace(pInfo->input, NULL) != 0) && _inputStreamPeekCharacter(pInfo->input, &ch);
    if (success && (ch == '\"' || ch == '\'')) {
        // EntityValue
        // This is not quite correct - the string scanned cannot contain '%' or '&' unless it's as part of a valid entity reference -- REW, 2/2/2000
        if (report) {
            entityData.replacementText = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
            success = _inputStreamScanQuotedString(pInfo->input, (CFMutableStringRef)entityData.replacementText);
        } else {
            success = _inputStreamScanQuotedString(pInfo->input, NULL);
        }
    } else if (success) {
        // ExternalID
        success = parseExternalID(pInfo, FALSE, report ? &(entityData.entityID) : NULL);
        if (success && !isPEDecl && _inputStreamSkipWhitespace(pInfo->input, NULL) != 0) {
            // There could be an option NDataDecl
            UniChar nDataStr[5] = {'N', 'D', 'A', 'T', 'A'};
            if (_inputStreamMatchString(pInfo->input, nDataStr, 5)) {
                success = (_inputStreamSkipWhitespace(pInfo->input, NULL) != 0) && _inputStreamScanXMLName(pInfo->input, NULL, FALSE);
            }
        }
    }
    if (success) {
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        success = _inputStreamGetCharacter(pInfo->input, &ch) && ch == '>';
    }
    if (!success) {
        if (_inputStreamAtEOF(pInfo->input)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
        }
    } else {
        if (isPEDecl) entityData.entityType = kCFXMLEntityTypeParameter;
        else if (entityData.replacementText) entityData.entityType = kCFXMLEntityTypeParsedInternal;
        else if (!entityData.notationName) entityData.entityType = kCFXMLEntityTypeParsedExternal;
        else entityData.entityType = kCFXMLEntityTypeUnparsed;
        pInfo->desc.dataTypeID = kCFXMLDataTypeEntity;
        pInfo->desc.additionalData = &entityData;
        success = reportNewLeaf(pInfo);
    }
    if (entityData.entityID.publicID) CFRelease(entityData.entityID.publicID);
    if (entityData.entityID.systemID) CFRelease(entityData.entityID.systemID);
    return success;
}

/*
 [28] doctypedecl ::= '<!DOCTYPE' S Name (S ExternalID)? S? ('[' (markupdecl | PEReference | S)* ']' S?)? '>'
 [29] markupdecl ::= elementdecl | AttlistDecl | EntityDecl | NotationDecl | PI | Comment 
*/ 
// First character should be just past '['
static Boolean parseInlineDTD(CFXMLParserRef pInfo) {
    Boolean success = TRUE;
    while (success && !_inputStreamAtEOF(pInfo->input)) {
        UniChar ch;

        parseWhitespace(pInfo);
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) break;
        if (ch == '%') {
            // PEReference
            success = parsePhysicalEntityReference(pInfo);
        } else if (ch == '<') {
            // markupdecl
            if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
                _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
                return FALSE;
            }
            if (ch == '?') {
                // Processing Instruction
                success = parseProcessingInstruction(pInfo, TRUE); // We can safely pass TRUE here, because *pInfo->top will be NULL if kCFXMLParserSkipMetaData is TRUE
            } else if (ch == '!') {
                UniChar dashes[2] = {'-', '-'};
                if (_inputStreamMatchString(pInfo->input, dashes, 2)) {
                    // Comment
                    success = parseComment(pInfo, TRUE);
                } else {
                    // elementdecl | AttListDecl | EntityDecl | NotationDecl
                    if (!_inputStreamPeekCharacter(pInfo->input, &ch)) {
                        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
                        return FALSE;
                    } else if (ch == 'A') {
                        // AttListDecl
                        success = parseAttributeListDeclaration(pInfo);
                    } else if (ch == 'N') {
                        success = parseNotationDeclaration(pInfo);
                    } else if (ch == 'E') {
                        // elementdecl | EntityDecl
                        _inputStreamGetCharacter(pInfo->input, &ch);
                        if (!_inputStreamPeekCharacter(pInfo->input, &ch)) {
                            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
                            return FALSE;
                        }
                        _inputStreamReturnCharacter(pInfo->input, 'E');
                        if (ch == 'L') {
                            success = parseElementDeclaration(pInfo);
                        } else if (ch == 'N') {
                            success = parseEntityDeclaration(pInfo);
                        } else {
                            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
                            return FALSE;
                        }
                    } else {
                        _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
                        return FALSE;
                    }                        
                }
            } else {
                _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
                return FALSE;
            }
        } else if (ch == ']') {
            return TRUE;
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedDTD, "Found unexpected character while parsing inline DTD");
            return FALSE;
        }
    }
    if (success) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Found unexpected EOF while parsing inline DTD");
    }
    return FALSE;
}

/*
[43] content ::= (element | CharData | Reference | CDSect | PI | Comment)*
 */
static Boolean parseTagContent(CFXMLParserRef pInfo) {
    while (!_inputStreamAtEOF(pInfo->input)) {
        UniChar ch;
        CFIndex numWhitespaceCharacters;
        
        _inputStreamSetMark(pInfo->input);
        numWhitespaceCharacters = _inputStreamSkipWhitespace(pInfo->input, NULL);
        // Don't report the whitespace yet; if the first thing we see is character data, we put the whitespace back and report it as part of the character data.
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) break;  // break == report unexpected EOF

        if (ch != '<' && ch != '&') { // CharData
            // Back off the whitespace; we'll report it with the PCData
            _inputStreamBackUpToMark(pInfo->input);
             if (!parsePCData(pInfo)) return FALSE;
            continue;
        }

        // element | Reference | CDSect | PI | Comment
        // We can safely report any whitespace now
        if (!(pInfo->options & kCFXMLParserSkipWhitespace) && numWhitespaceCharacters != 0 && *(pInfo->top)) {
            _inputStreamReturnCharacter(pInfo->input, ch);
            _inputStreamGetCharactersFromMark(pInfo->input, (CFMutableStringRef)(pInfo->desc.dataString));
            pInfo->desc.dataTypeID = kCFXMLDataTypeWhitespace;
            pInfo->desc.additionalData = NULL;
            if (!reportNewLeaf(pInfo)) return FALSE;
            _inputStreamGetCharacter(pInfo->input, &ch);
        }
        
        if (ch == '&') {
            // Reference; for the time being, we don't worry about processing these; just report them as Entity references
            if (!parseEntityReference(pInfo, TRUE)) return FALSE;
            continue;
        }

        // ch == '<'; element | CDSect | PI | Comment
        if (!_inputStreamPeekCharacter(pInfo->input, &ch)) break;
        if (ch == '?') { // PI
            _inputStreamGetCharacter(pInfo->input, &ch);
            if (!parseProcessingInstruction(pInfo, TRUE))
                return FALSE;
        } else if (ch == '/') { // end tag; we're passing outside of content's production
            _inputStreamReturnCharacter(pInfo->input, '<'); // Back off to the '<'
            return TRUE;
        } else if (ch != '!') { // element
            if (!parseTag(pInfo))  return FALSE;
        } else {
            // Comment | CDSect
            UniChar dashes[3] = {'!', '-', '-'};
            if (_inputStreamMatchString(pInfo->input, dashes, 3)) {
                // Comment
                if (!parseComment(pInfo, TRUE)) return FALSE;
            } else {
                // Should have a CDSect; back off the "<!" and call parseCDSect
                _inputStreamReturnCharacter(pInfo->input, '<');
                if (!parseCDSect(pInfo)) return FALSE;
            }
        }
    }

    // Only way to get here is if premature EOF was found
#warning CF:Include the tag name here
    _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing tag content");
    return FALSE;
}

static UniChar _CDSectOpening[9] = {'<', '!', '[', 'C', 'D', 'A', 'T', 'A', '['};
static Boolean parseCDSect(CFXMLParserRef pInfo) {
    UniChar _CDSectClose[3] = {']', ']', '>'};
    if (!_inputStreamMatchString(pInfo->input, _CDSectOpening, 9)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedCDSect, "Encountered bad prefix to a presumed CDATA section");
        return FALSE;
    }
    if (!_inputStreamScanToCharacters(pInfo->input, _CDSectClose, 3, (CFMutableStringRef)(pInfo->desc.dataString))) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing CDATA section");
        return FALSE;
    }

    pInfo->desc.dataTypeID = kCFXMLDataTypeCDATASection;
    pInfo->desc.additionalData = NULL;
    return reportNewLeaf(pInfo);
}

/*
 [66] CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
*/
static Boolean validateCharacterReference(CFStringRef str) {
    Boolean isHex;
    CFIndex index, len = CFStringGetLength(str);
    if (len < 2) return FALSE;
    if (CFStringGetCharacterAtIndex(str, 0) != '#') return FALSE;
    if (CFStringGetCharacterAtIndex(str, 1) == 'x') {
        isHex = TRUE;
        index = 2;
        if (len == 2) return FALSE;
    } else {
        isHex = FALSE;
        index = 1;
    }

    while (index < len) {
        UniChar ch;
        ch = CFStringGetCharacterAtIndex(str, index);
        index ++;
        if (!(ch <= '9' && ch >= '0') &&
            !(isHex && ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')))) {
            break;
        }
    }
    return (index == len);
}

/*
 [67] Reference ::= EntityRef | CharRef
 [68] EntityRef ::= '&' Name ';'
*/
static Boolean parseEntityReference(CFXMLParserRef pInfo, Boolean report) {
    UniChar ch;
    CFXMLEntityTypeID typeID;
    if (!_inputStreamPeekCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing EntityReference");
        return FALSE;
    }
    if (ch == '#') {
        ch = ';';
        if (!_inputStreamScanToCharacters(pInfo->input, &ch, 1, (CFMutableStringRef)pInfo->desc.dataString)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing EntityReference");
            return FALSE;
        } else if (!validateCharacterReference(pInfo->desc.dataString)) {
            _CFReportError(pInfo, kCFXMLErrorMalformedCharacterReference, "Encountered illegal character while parsing character reference");
            return FALSE;
        }
        typeID = kCFXMLEntityTypeCharacter;
    } else if (!_inputStreamScanXMLName(pInfo->input, report ? (CFMutableStringRef)pInfo->desc.dataString : NULL, FALSE) || !_inputStreamGetCharacter(pInfo->input, &ch) || ch != ';') {
        if (_inputStreamAtEOF(pInfo->input)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing EntityReference");
            return FALSE;
        } else {
            _CFReportError(pInfo, kCFXMLErrorMalformedName, "Encountered malformed name while parsing EntityReference");
            return FALSE;
        }
    } else {
        typeID = kCFXMLEntityTypeParsedInternal;
    }
    if (report) {
        pInfo->desc.dataTypeID = kCFXMLDataTypeEntityReference;
        pInfo->desc.additionalData = &typeID;
        return reportNewLeaf(pInfo);
    } else {
        return TRUE;
    }
}

#if 0
// Kept from old entity reference parsing....
{
    switch (*(pInfo->curr)) {
        case 'l':  // "lt"
            if (len >= 3 && *(pInfo->curr+1) == 't' && *(pInfo->curr+2) == ';') {
                ch = '<';
                pInfo->curr += 3;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'g': // "gt"
            if (len >= 3 && *(pInfo->curr+1) == 't' && *(pInfo->curr+2) == ';') {
                ch = '>';
                pInfo->curr += 3;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'a': // "apos" or "amp"
            if (len < 4) {   // Not enough characters for either conversion
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", kCFStringEncodingASCII);
                return;
            }
            if (*(pInfo->curr+1) == 'm') {
                // "amp"
                if (*(pInfo->curr+2) == 'p' && *(pInfo->curr+3) == ';') {
                    ch = '&';
                    pInfo->curr += 4;
                    break;
                }
            } else if (*(pInfo->curr+1) == 'p') {
                // "apos"
                if (len > 4 && *(pInfo->curr+2) == 'o' && *(pInfo->curr+3) == 's' && *(pInfo->curr+4) == ';') {
                    ch = '\'';
                    pInfo->curr += 5;
                    break;
                }
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case 'q':  // "quote"
            if (len >= 6 && *(pInfo->curr+1) == 'u' && *(pInfo->curr+2) == 'o' && *(pInfo->curr+3) == 't' && *(pInfo->curr+4) == 'e' && *(pInfo->curr+5) == ';') {
                ch = '\"';
                pInfo->curr += 6;
                break;
            }
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
        case '#':
        {
            UniChar num = 0;
            Boolean isHex = FALSE;
            if ( len < 4) {  // Not enough characters to make it all fit!  Need at least "&#d;"
                pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", kCFStringEncodingASCII);
                return;
            }
            pInfo->curr ++;
            if (*(pInfo->curr) == 'x') {
                isHex = TRUE;
                pInfo->curr ++;
            }
            while (pInfo->curr < pInfo->end) {
                ch = *(pInfo->curr);
                if (ch == ';') {
                    CFStringAppendCharacters(string, &num, 1);
                    pInfo->curr ++;
                    return;
                }
                if (!isHex) num = num*10;
                else num = num << 4;
                if (ch <= '9' && ch >= '0') {
                    num += (ch - '0');
                } else if (!isHex) {
                    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c at line %d"), ch, lineNumber(pInfo));
                    return;
                } else if (ch >= 'a' && ch <= 'f') {
                    num += 10 + (ch - 'a');
                } else if (ch >= 'A' && ch <= 'F') {
                    num += 10 + (ch - 'A');
                } else {
                    pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unexpected character %c at line %d"), ch, lineNumber(pInfo));
                    return;                    
                }
            }
            pInfo->errorString = CFStringCreateWithCString(pInfo->allocator, "Encountered unexpected EOF", kCFStringEncodingASCII);
            return;
        }
        default:
            pInfo->errorString = CFStringCreateWithFormat(pInfo->allocator, NULL, CFSTR("Encountered unknown ampersand-escape sequence at line %d"), lineNumber(pInfo));
            return;
    }
    CFStringAppendCharacters(string, &ch, 1);
}
#endif

/*
[14] CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)
*/
static Boolean parsePCData(CFXMLParserRef pInfo) {
    UniChar ch;
    Boolean done = FALSE;
    _inputStreamSetMark(pInfo->input);
    while (!done && _inputStreamGetCharacter(pInfo->input, &ch)) {
        switch (ch) {
            case '<': 
            case '&':
                _inputStreamReturnCharacter(pInfo->input, ch);
                done = TRUE;
                break;
            case ']':
            {
                UniChar endSequence[2] = {']', '>'};
                if (_inputStreamMatchString(pInfo->input, endSequence, 2)) {
                    _CFReportError(pInfo, kCFXMLErrorMalformedParsedCharacterData, "Encountered \"]]>\" in parsed character data");
                    return FALSE;
                }
                break;
            }
            default:
                ;
        }
    }
    _inputStreamGetCharactersFromMark(pInfo->input, (CFMutableStringRef)(pInfo->desc.dataString));
    pInfo->desc.dataTypeID = kCFXMLDataTypeText;
    pInfo->desc.additionalData = NULL;
    return reportNewLeaf(pInfo);
}

/*
[42] ETag ::= '</' Name S? '>'
 */
static Boolean parseCloseTag(CFXMLParserRef pInfo, UniChar *tag, UInt32 tagLen) {
    UniChar beginEndTag[2] = {'<', '/'};
    Boolean unexpectedEOF = FALSE, mismatch = FALSE;
    if (_inputStreamMatchString(pInfo->input, beginEndTag, 2) && _inputStreamMatchString(pInfo->input, tag, tagLen)) {
        UniChar ch;
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
            unexpectedEOF = TRUE;
        } else if (ch != '>') {
            mismatch = TRUE;
        }
    } else if (_inputStreamAtEOF(pInfo->input)) {
        unexpectedEOF = TRUE;
    } else {
        mismatch = TRUE;
    }
        
    if (unexpectedEOF || mismatch) {
        if (pInfo->callBacks.handleError) {
            CFStringSetExternalCharactersNoCopy((CFMutableStringRef)(pInfo->desc.dataString), tag, tagLen, tagLen);
            if (unexpectedEOF) {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->alloc, NULL, CFSTR("Encountered unexpected EOF while parsing close tag for <%@>"), pInfo->desc.dataString);
                pInfo->status = kCFXMLErrorUnexpectedEOF;
                pInfo->callBacks.handleError(pInfo, kCFXMLErrorUnexpectedEOF, pInfo->context);
            } else {
                pInfo->errorString = CFStringCreateWithFormat(pInfo->alloc, NULL, CFSTR("Encountered malformed close tag for <%@>"), pInfo->desc.dataString);
                pInfo->status = kCFXMLErrorMalformedCloseTag;
                pInfo->callBacks.handleError(pInfo, kCFXMLErrorMalformedCloseTag, pInfo->context);
            }
        }
        return FALSE;
    }
    return TRUE;
}

/*
 [39] element ::= EmptyElementTag | STag content ETag
 [40] STag ::= '<' Name (S Attribute)* S? '>'
 [44] EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'
*/
static Boolean parseTag(CFXMLParserRef pInfo) {
    UniChar ch;
    void *tag;
    CFXMLElementData data;
    Boolean success = TRUE;
    CFIndex tagLength;
    UniChar *tagName;

    if (!_inputStreamScanXMLName(pInfo->input, (CFMutableStringRef)(pInfo->desc.dataString), FALSE)) {
        _CFReportError(pInfo, kCFXMLErrorMalformedStartTag, "Encountered malformed start tag");
        return FALSE;
    }
    // We must squirrel away the tag name immediately, because subsequent parses have the right to muck with desc.dataString. -- REW, 2/4/2000
    tagLength = CFStringGetLength(pInfo->desc.dataString);
    tagName = CFAllocatorAllocate(pInfo->alloc, sizeof(UniChar) * tagLength, 0);
    CFStringGetCharacters(pInfo->desc.dataString, CFRangeMake(0, tagLength), tagName);

    _inputStreamSkipWhitespace(pInfo->input, NULL);
    
    pInfo->desc.dataTypeID = kCFXMLDataTypeElement;
    pInfo->desc.additionalData = &data;
    if (!parseAttributes(pInfo)) return FALSE; // parsed directly into pInfo->argDict ; parseAttributes consumes any trailing whitespace
    data.attributes = pInfo->argDict;
    data.attributeOrder = pInfo->argArray;
    if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF");
        return FALSE;
    }
    if (ch == '/') {
        data.isEmpty = TRUE;
        if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
            _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF");
            return FALSE;
        }
    } else {
        data.isEmpty = FALSE;
    }
    if (ch != '>') {
        _CFReportError(pInfo, kCFXMLErrorMalformedStartTag, "Encountered malformed start tag");
        return FALSE;
    }

    if (*pInfo->top || pInfo->top == pInfo->stack) {
        CFStringSetExternalCharactersNoCopy((CFMutableStringRef) pInfo->desc.dataString, tagName, tagLength, tagLength);
        tag = pInfo->callBacks.createXMLStructure(pInfo, &(pInfo->desc), pInfo->context);
        if (tag && pInfo->status == kCFXMLStatusParseInProgress) {
            pInfo->callBacks.addChild(pInfo, *pInfo->top, tag, pInfo->context);
        }
        if (pInfo->status != kCFXMLStatusParseInProgress) {
            // callback called CFXMLParserAbort()
            _CFReportError(pInfo, pInfo->status, NULL);
            return FALSE;
        }
    } else {
        tag = NULL;
    }

    pushXMLNode(pInfo, tag);
    if (!data.isEmpty) {
        success =  parseTagContent(pInfo);
        if (success) {
            success = parseCloseTag(pInfo, tagName, tagLength);
        }
        CFAllocatorDeallocate(pInfo->alloc, tagName);
    }
    pInfo->top --;

    if (success && tag) {
        pInfo->callBacks.endXMLStructure(pInfo, tag, pInfo->context);
        if (pInfo->status != kCFXMLStatusParseInProgress) {
            _CFReportError(pInfo, pInfo->status, NULL);
            return FALSE;
        }
    }
    return success;
}

/*
 [10] AttValue ::= '"' ([^<&"] | Reference)* '"' |  "'" ([^<&'] | Reference)* "'"
 [67] Reference ::= EntityRef | CharRef
 [68] EntityRef ::= '&' Name ';'
 */
// For the moment, we don't worry about references in the attribute values.
static Boolean parseAttributeValue(CFXMLParserRef pInfo, CFMutableStringRef str) {
    UniChar quote, ch;
    Boolean success = _inputStreamGetCharacter(pInfo->input, &quote);
    if (!success || (quote != '\'' && quote != '\"')) return FALSE;
    if (str) _inputStreamSetMark(pInfo->input);
    while (_inputStreamGetCharacter(pInfo->input, &ch) && ch != quote) {
        switch (ch) {
            case '<': return FALSE;
            case '&':
                if (!parseEntityReference(pInfo, FALSE)) return FALSE;
            default:
                ;
        }
    }
    if (!_inputStreamAtEOF(pInfo->input)) {
        if (str) {
            _inputStreamReturnCharacter(pInfo->input, quote);
            _inputStreamGetCharactersFromMark(pInfo->input, str);
            _inputStreamGetCharacter(pInfo->input, &ch);
        }
        return TRUE;
    }
    return FALSE;
}

/*
 [40] STag ::= '<' Name (S Attribute)* S? '>'
 [41] Attribute ::= Name Eq AttValue
 [25] Eq ::= S? '=' S?
*/

// Expects pInfo->curr to be at the first content character; will consume the trailing whitespace.  
Boolean parseAttributes(CFXMLParserRef pInfo) {
    UniChar ch;
    CFMutableDictionaryRef dict;
    CFMutableArrayRef array;
    Boolean failure = FALSE;
    if (_inputStreamPeekCharacter(pInfo->input, &ch) == '>') {
        if (pInfo->argDict) {
            CFDictionaryRemoveAllValues(pInfo->argDict);
            CFArrayRemoveAllValues(pInfo->argArray);
        }
        return TRUE;  // No attributes; let caller deal with it
    }
    if (!pInfo->argDict) {
        pInfo->argDict = CFDictionaryCreateMutable(pInfo->alloc, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        pInfo->argArray = CFArrayCreateMutable(pInfo->alloc, 0, &kCFTypeArrayCallBacks);
    } else {
        CFDictionaryRemoveAllValues(pInfo->argDict);
        CFArrayRemoveAllValues(pInfo->argArray);
    }
    dict = pInfo->argDict;
    array = pInfo->argArray; 
    while (!failure && _inputStreamPeekCharacter(pInfo->input, &ch) && ch != '>' && ch != '/') {
        CFMutableStringRef key, value;

        key = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
        if (!_inputStreamScanXMLName(pInfo->input, key, FALSE)) {
            failure = TRUE;
            CFRelease(key);
            break;
        }
        if (CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), key) != -1) {
            CFRelease(key);
            _CFReportError(pInfo, kCFXMLErrorMalformedStartTag, "Found repeated attribute");
            return FALSE;
        }
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        if (!_inputStreamGetCharacter(pInfo->input, &ch) || ch != '=') { 
            CFRelease(key);
            failure = TRUE;
            break;
        }
        _inputStreamSkipWhitespace(pInfo->input, NULL);
        value = CFStringCreateMutableWithExternalCharactersNoCopy(pInfo->alloc, NULL, 0, 0, kCFAllocatorNull);
        if (!parseAttributeValue(pInfo, value)) {
            CFRelease(key);
            CFRelease(value);
            failure = TRUE;
            break;
        }
        CFArrayAppendValue(array, key);
        CFDictionarySetValue(dict, key, value);
        CFRelease(key);
        CFRelease(value);
        _inputStreamSkipWhitespace(pInfo->input, NULL);
    }
    if (failure) {
#warning CF:Include tag name in this error report
        _CFReportError(pInfo, kCFXMLErrorMalformedStartTag, "Found illegal character while parsing element tag");
        return FALSE;
    } else if (_inputStreamAtEOF(pInfo->input)) {
        _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing element attributes");
        return FALSE;
    } else {
        return TRUE;
    }
}

/*
 [1]  document ::= prolog element Misc*
 [22] prolog ::= XMLDecl? Misc* (doctypedecl Misc*)?
 [27] Misc ::= Comment | PI | S
 [23] XMLDecl ::= '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>' 

 We treat XMLDecl as a plain old PI, since PI is part of Misc.  This changes the prolog and document productions to
 [22-1] prolog ::= Misc* (doctypedecl Misc*)?
 [1-1] document ::= Misc* (doctypedecl Misc*)? element Misc*

 NOTE: This function assumes pInfo->stack has a valid top.  I.e. the document pointer has already been created!
*/
static Boolean parseXML(CFXMLParserRef pInfo) {
    Boolean success = TRUE, sawDTD = FALSE, sawElement = FALSE;
    UniChar ch;
    while (success && _inputStreamPeekCharacter(pInfo->input, &ch)) {
        switch (ch) {
            case ' ':
            case '\n':
            case '\t':
            case '\r':
                success = parseWhitespace(pInfo);
                break;
            case '<':
                _inputStreamGetCharacter(pInfo->input, &ch);
                if (!_inputStreamGetCharacter(pInfo->input, &ch)) {
                    _CFReportError(pInfo, kCFXMLErrorUnexpectedEOF, "Encountered unexpected EOF while parsing top-level document");
                    return FALSE;
                }
                if (ch == '!') {
                    // Comment or DTD
                    UniChar dashes[2] = {'-', '-'};
                    if (_inputStreamMatchString(pInfo->input, dashes, 2)) {
                        // Comment
                        success = parseComment(pInfo, TRUE);
                    } else {
                        // Should be DTD
                        if (sawDTD) {
                            _CFReportError(pInfo, kCFXMLErrorMalformedDocument, "Encountered a second DTD");
                            return FALSE;
                        }
                        success = parseDTD(pInfo);
                        if (success) sawDTD = TRUE;
                    }
                } else if (ch == '?') {
                    // Processing instruction
                    success = parseProcessingInstruction(pInfo, TRUE);
                } else {
                    // Tag or malformed
                    if (sawElement) {
                        _CFReportError(pInfo, kCFXMLErrorMalformedDocument, "Encountered second top-level element");
                        return FALSE;
                    }
                    _inputStreamReturnCharacter(pInfo->input, ch);
                    success = parseTag(pInfo);
                    if (success) sawElement = TRUE;
                }
                break;
            default: {
                pInfo->status = kCFXMLErrorMalformedDocument;
                pInfo->errorString = ch < 256 ?
                    CFStringCreateWithFormat(pInfo->alloc, NULL, CFSTR("Encountered unexpected character 0x%x (\'%c\') at top-level"), ch, ch) :
                    CFStringCreateWithFormat(pInfo->alloc, NULL, CFSTR("Encountered unexpected Unicode character 0x%x at top-level"), ch);

                if (pInfo->callBacks.handleError) {
                    pInfo->callBacks.handleError(pInfo, pInfo->status, pInfo->context);
                }
                return FALSE;
            }
        }
    }
    
    if (!success) return FALSE;
    if (!sawElement) {
        _CFReportError(pInfo, kCFXMLErrorElementlessDocument, "No element found in document");
        return FALSE;
    }
    return TRUE;
}


/* Utility functions used in parsing */
static CFStringEncoding encodingForXMLData(CFDataRef data) {
    const UInt8 *bytes = (UInt8 *)CFDataGetBytePtr(data);
    UInt32 length = CFDataGetLength(data);
    const UInt8 *index, *end;
    char quote;

    // Check for the byte order mark first
    if (length > 2 &&
        ((*bytes == 0xFF && *(bytes+1) == 0xFE) ||
         (*bytes == 0xFE && *(bytes+1) == 0xFF) ||
         *bytes == 0x00 || *(bytes+1) == 0x00)) // This clause checks for a Unicode sequence lacking the byte order mark; technically an error, but this check is recommended by the XML spec
        return kCFStringEncodingUnicode;

    // Scan for the <?xml.... ?> opening
    if (length < 5 || strncmp((char const *) bytes, "<?xml", 5) != 0) return kCFStringEncodingUTF8;
    index = bytes + 5;
    end = bytes + length;
    // Found "<?xml"; now we scan for "encoding"
    while (index < end) {
        UInt8 ch = *index;
        const UInt8 *scan;
        if ( ch == '?' || ch == '>') return kCFStringEncodingUTF8;
        index ++;
        scan = index;
        if (ch == 'e' && *scan++ == 'n' && *scan++ == 'c' && *scan++ == 'o' && *scan++ == 'd' && *scan++ == 'i'
            && *scan++ == 'n' && *scan++ == 'g' && *scan++ == '=') {
            index = scan;
            break;
        }
    }
    if (index >= end) return kCFStringEncodingUTF8;
    quote = *index;
    if (quote != '\'' && quote != '\"') {
        return kCFStringEncodingUTF8;
    } else {
        CFStringRef encodingName;
        const UInt8 *base = index+1; // Move past the quote character
        CFStringEncoding enc;
        UInt32 len;
        index ++;
        while (index < end && *index != quote) index ++;
        if (index >= end) return kCFStringEncodingUTF8;
        len = index - base;
        if (len == 5 && (*base == 'u' || *base == 'U') && (base[1] == 't' || base[1] == 'T') || (base[2] == 'f' || base[2] == 'F') && base[3] == '-' && base[4] == '8')
            return kCFStringEncodingUTF8;
#warning CF:Can this creation be avoided by a direct call to whatever ConvertIANACharSetName uses?
        encodingName = CFStringCreateWithBytes(CFGetAllocator(data), base, len, kCFStringEncodingISOLatin1, FALSE);
        enc = CFStringConvertIANACharSetNameToEncoding(encodingName);
        CFRelease(encodingName);
        return enc;
    }
}

static void _CFReportError(CFXMLParserRef pInfo, SInt32 errNum, const char *str) {
    if (str) {
        pInfo->status = errNum;
        pInfo->errorString = CFStringCreateWithCString(pInfo->alloc, str, kCFStringEncodingASCII);
    }
    if (pInfo->callBacks.handleError) {
        pInfo->callBacks.handleError(pInfo, errNum, pInfo->context);
    }
}

// Assumes pInfo->desc has been set and is ready to go
static Boolean reportNewLeaf(CFXMLParserRef pInfo) {
    void *xmlStruct;
    if (*(pInfo->top) == NULL) return TRUE;

    xmlStruct = pInfo->callBacks.createXMLStructure(pInfo, &(pInfo->desc), pInfo->context);
    if (xmlStruct && pInfo->status == kCFXMLStatusParseInProgress) {
        pInfo->callBacks.addChild(pInfo, *(pInfo->top), xmlStruct, pInfo->context);
        if (pInfo->status == kCFXMLStatusParseInProgress) pInfo->callBacks.endXMLStructure(pInfo, xmlStruct, pInfo->context);
    }
    if (pInfo->status != kCFXMLStatusParseInProgress) {
        _CFReportError(pInfo, pInfo->status, NULL);
        return FALSE;
    }
    return TRUE;
}

static void pushXMLNode(CFXMLParserRef pInfo, void *node) {
    pInfo->top ++;
    if (pInfo->top - pInfo->stack == pInfo->capacity) {
        pInfo->stack = CFAllocatorReallocate(pInfo->alloc, pInfo->stack, 2*pInfo->capacity, 0);
        pInfo->top = pInfo->stack + pInfo->capacity;
        pInfo->capacity = 2*pInfo->capacity;
    }
    *(pInfo->top) = node;
}

CFXMLDataTypeDescription *CFXMLCreateDataTypeDescriptionCopy(CFAllocatorRef allocator, const CFXMLDataTypeDescription *source) {
    CFXMLDataTypeDescription *destination;
    CFAssert1(source != NULL, __kCFLogAssertion, "%s(): NULL source argument not permitted", __PRETTY_FUNCTION__);
    if (allocator == NULL) {
        allocator = CFRetain(CFAllocatorGetDefault());
    } else {
        CFRetain(allocator);
    }
    
    destination = CFAllocatorAllocate(allocator, sizeof(CFXMLDataTypeDescription), 0);
    destination->_reserved = (void *)allocator;
    destination->dataTypeID = source->dataTypeID;
    if (source->dataString) {
        destination->dataString = CFStringCreateCopy(allocator, source->dataString);
    } else {
        destination->dataString = NULL;
    }
    if (!source->additionalData) {
        destination->additionalData = NULL;
        return destination;
    }

    switch(source->dataTypeID) {
        case kCFXMLDataTypeDocument:
            destination->additionalData = CFAllocatorAllocate(allocator, sizeof(CFXMLDocumentData), 0);
            if (((CFXMLDocumentData *)source->additionalData)->sourceURL) {
                // If URLs ever become mutable, this line will need to be changed....
                ((CFXMLDocumentData *)destination->additionalData)->sourceURL = CFRetain(((CFXMLDocumentData *)source->additionalData)->sourceURL);
            } else {
                ((CFXMLDocumentData *)destination->additionalData)->sourceURL = NULL;
            }

            ((CFXMLDocumentData *)destination->additionalData)->encoding = ((CFXMLDocumentData *)source->additionalData)->encoding;
            break;
        case kCFXMLDataTypeElement: {
            CFXMLElementData *destData, *srcData = (CFXMLElementData *)source->additionalData;
            CFIndex count;
            
            destination->additionalData = CFAllocatorAllocate(allocator, sizeof(CFXMLElementData), 0);
            destData = (CFXMLElementData *)destination->additionalData;
            if (srcData->attributeOrder && (count = CFArrayGetCount(srcData->attributeOrder))) {
                CFIndex idx;
                destData->attributes = CFDictionaryCreateMutable(allocator, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                destData->attributeOrder = CFArrayCreateMutable(allocator, count, &kCFTypeArrayCallBacks);

                for (idx = 0; idx < count; idx ++) {
                    CFStringRef oldKey = CFArrayGetValueAtIndex(srcData->attributeOrder, idx);
                    CFStringRef newKey = CFStringCreateCopy(allocator, oldKey);
                    CFStringRef newVal = CFStringCreateCopy(allocator, CFDictionaryGetValue(srcData->attributes, oldKey));
                    CFArrayAppendValue((CFMutableArrayRef)destData->attributeOrder, newKey);
                    CFDictionarySetValue((CFMutableDictionaryRef)destData->attributes, newKey, newVal);
                    CFRelease(newKey);
                    CFRelease(newVal);
                }
            } else {
                destData->attributes = NULL;
                destData->attributeOrder = NULL;
            }
            destData->isEmpty = srcData->isEmpty;
            break;
        }
        case kCFXMLDataTypeProcessingInstruction:
            destination->additionalData = CFAllocatorAllocate(allocator, sizeof(CFXMLProcessingInstructionData), 0);
            if (((CFXMLProcessingInstructionData *)source->additionalData)->dataString) {
                ((CFXMLProcessingInstructionData *)destination->additionalData)->dataString = CFStringCreateCopy(allocator, ((CFXMLProcessingInstructionData *)source->additionalData)->dataString);
            } else {
                ((CFXMLProcessingInstructionData *)destination->additionalData)->dataString = NULL;
            }
            break;
        case kCFXMLDataTypeEntity:
        {
            CFXMLEntityData *sourceData = (CFXMLEntityData *)source->additionalData;
            CFXMLEntityData *destData = (CFXMLEntityData *)CFAllocatorAllocate(allocator, sizeof(CFXMLEntityData), 0);
            destination->additionalData = (void *)destData;
            destData->entityType = sourceData->entityType;
            destData->replacementText = sourceData->replacementText ? CFStringCreateCopy(allocator, sourceData->replacementText) : NULL;
            destData->entityID.systemID = sourceData->entityID.systemID ? CFRetain(sourceData->entityID.systemID) : NULL;
            destData->entityID.publicID = sourceData->entityID.publicID ? CFStringCreateCopy(allocator, sourceData->entityID.publicID) : NULL;
            destData->notationName = sourceData->notationName ? CFStringCreateCopy(allocator, sourceData->notationName) : NULL;
            break;
        }
        case kCFXMLDataTypeEntityReference:
        {
            destination->additionalData = CFAllocatorAllocate(allocator, sizeof(CFXMLEntityTypeID), 0);
            *((CFXMLEntityTypeID *)(destination->additionalData)) = *((CFXMLEntityTypeID *)(source->additionalData));
            break;
        }
        case kCFXMLDataTypeDocumentType:
        case kCFXMLDataTypeNotation:
            // Both of these use a CFXMLExternalID as their additionalData
        {
            CFXMLExternalID *sourceData = (CFXMLExternalID *)source->additionalData;
            CFXMLExternalID *destData = (CFXMLExternalID *)CFAllocatorAllocate(allocator, sizeof(CFXMLExternalID), 0);
            destination->additionalData = (void *)destData;
            destData->systemID = sourceData->systemID ? CFRetain(sourceData->systemID) : NULL;
            destData->publicID = sourceData->publicID ? CFStringCreateCopy(allocator, sourceData->publicID) : NULL;
            break;
        }
        case kCFXMLDataTypeElementTypeDeclaration:
            destination->additionalData = CFAllocatorAllocate(allocator, sizeof(CFXMLElementTypeDeclarationData), 0);
            if (((CFXMLElementTypeDeclarationData *)source->additionalData)->contentDescription) {
                ((CFXMLElementTypeDeclarationData *)destination->additionalData)->contentDescription = CFStringCreateCopy(allocator, ((CFXMLElementTypeDeclarationData *)source->additionalData)->contentDescription);
            } else {
                ((CFXMLElementTypeDeclarationData *)destination->additionalData)->contentDescription = NULL;
            }
            break;
        case kCFXMLDataTypeAttributeListDeclaration:
        {
            CFXMLAttributeListDeclarationData *sourceData = (CFXMLAttributeListDeclarationData *)source->additionalData;
            CFXMLAttributeListDeclarationData *destData = CFAllocatorAllocate(allocator, sizeof(CFXMLAttributeListDeclarationData), 0);
            CFIndex index;
            destination->additionalData = (void *)destData;
            destData->numberOfAttributes = sourceData->numberOfAttributes;
            destData->attributes = sourceData->numberOfAttributes ? CFAllocatorAllocate(allocator, sizeof(CFXMLAttributeDeclarationData)*sourceData->numberOfAttributes, 0) : NULL;
            for (index = 0; index < sourceData->numberOfAttributes; index ++) {
                CFXMLAttributeDeclarationData sourceAttr = sourceData->attributes[index];
                CFXMLAttributeDeclarationData *destAttr = &(destData->attributes[index]);
                destAttr->attributeName = CFStringCreateCopy(allocator, sourceAttr.attributeName);
                destAttr->typeString = CFStringCreateCopy(allocator, sourceAttr.typeString);
                destAttr->defaultString = CFStringCreateCopy(allocator, sourceAttr.defaultString);
            }
            break;
        }
        default:
            CFAssert2(FALSE, __kCFLogAssertion, "%s(): Encountered unexpected typeID %d (additionalData should be empty)", __PRETTY_FUNCTION__, source->dataTypeID);
    }
    return destination;
}

void CFXMLDestroyDataTypeDescription(CFXMLDataTypeDescription *desc) {
    CFAllocatorRef allocator;
    CFAssert1(desc != NULL, __kCFLogAssertion, "%s(): NULL argument not permitted", __PRETTY_FUNCTION__);
    allocator = (CFAllocatorRef)desc->_reserved;

    if (desc->dataString) CFRelease(desc->dataString);
    if (desc->additionalData) {
        switch(desc->dataTypeID) {
            case kCFXMLDataTypeDocument:
                if (((CFXMLDocumentData *)desc->additionalData)->sourceURL) {
                    CFRelease(((CFXMLDocumentData *)desc->additionalData)->sourceURL);
                }
                break;
            case kCFXMLDataTypeElement: 
                if (((CFXMLElementData *)desc->additionalData)->attributes) {
                    CFRelease(((CFXMLElementData *)desc->additionalData)->attributes);
                    CFRelease(((CFXMLElementData *)desc->additionalData)->attributeOrder);
                }
                break;
            case kCFXMLDataTypeProcessingInstruction:
                if (((CFXMLProcessingInstructionData *)desc->additionalData)->dataString) {
                    CFRelease(((CFXMLProcessingInstructionData *)desc->additionalData)->dataString);
                } 
                break;
            case kCFXMLDataTypeEntity:
            {
                CFXMLEntityData *data = (CFXMLEntityData *)desc->additionalData;
                if (data->replacementText) CFRelease(data->replacementText);
                if (data->entityID.systemID) CFRelease(data->entityID.systemID);
                if (data->entityID.publicID) CFRelease(data->entityID.publicID);
                if (data->notationName) CFRelease(data->notationName);
                break;
            }
            case kCFXMLDataTypeEntityReference:
            {
                // Do nothing; additionalData has no structure of its own, with dependent pieces to release.  -- REW, 2/11/2000
                break;
            }
            case kCFXMLDataTypeDocumentType:
            case kCFXMLDataTypeNotation:
            {
                CFXMLExternalID *data = (CFXMLExternalID *)desc->additionalData;
                if (data->systemID) CFRelease(data->systemID);
                if (data->publicID) CFRelease(data->publicID);
                break;
            }
            case kCFXMLDataTypeElementTypeDeclaration:
                if (((CFXMLElementTypeDeclarationData *)desc->additionalData)->contentDescription) {
                    CFRelease(((CFXMLElementTypeDeclarationData *)desc->additionalData)->contentDescription);
                }
                break;
            case kCFXMLDataTypeAttributeListDeclaration:
            {
                CFXMLAttributeListDeclarationData *data = (CFXMLAttributeListDeclarationData *)desc->additionalData;
                CFIndex index;
                for (index = 0; index < data->numberOfAttributes; index ++) {
                    CFRelease(data->attributes[index].attributeName);
                    CFRelease(data->attributes[index].typeString);
                    CFRelease(data->attributes[index].defaultString);
                }
                CFAllocatorDeallocate(allocator, data->attributes);
                break;
            }
            default:
                CFAssert1(FALSE, __kCFLogAssertion, "%s(): Encountered unexpected typeID %d (additionalData should be empty)", desc->dataTypeID);
        }
        CFAllocatorDeallocate(allocator, desc->additionalData);
    }
    CFAllocatorDeallocate(allocator, desc);
    CFRelease(allocator);
}

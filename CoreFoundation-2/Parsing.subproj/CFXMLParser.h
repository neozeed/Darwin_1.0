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
/*	CFXMLParser.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFXMLPARSER__)
#define __COREFOUNDATION_CFXMLPARSER__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFTree.h>
#include <CoreFoundation/CFURL.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __CFXMLParser * CFXMLParserRef;

/* These are the various options you can configure the parser with.  These are
   chosen such that an option flag of 0 (kCFXMLParserNoOptions) leaves the XML
   as "intact" as possible (reports all structures; performs no replacements).
   Hence, to make the parser do the most work, returning only the pure element
   tree, set the option flag to kCFXMLParserAllOptions.

kCFXMLParserValidateDocument -
   validate the document against its grammar from the DTD, reporting any errors.
   Currently not supported.

kCFXMLParserSkipMetaData -
   silently skip over metadata constructs (the DTD and comments)

kCFXMLParserReplacePhysicalEntities -
   replace declared entities like &lt;.  Note that other than the 5 predefined
   entities (lt, gt, quot, amp, apos), these must be defined in the DTD.
   Currently not supported.

kCFXMLParserSkipWhitespace -
   skip over all whitespace that does not abut non-whitespace character data.
   In other words, given <foo>  <bar> blah </bar></foo>, the whitespace between
   foo's open tag and bar's open tag would be suppressed, but the whitespace
   around blah would be preserved.

kCFXMLParserAddImpliedAttributes -
   where the DTD specifies implied attribute-value pairs for a particular element,
   add those pairs to any occurances of the element in the element tree.
   Currently not supported.
*/

typedef enum {
    kCFXMLParserValidateDocument = (1 << 0),
    kCFXMLParserSkipMetaData = (1 << 1),
    kCFXMLParserReplacePhysicalEntities = (1 << 2),
    kCFXMLParserSkipWhitespace = (1 << 3),
    kCFXMLParserResolveExternalEntities = (1 << 4),
    kCFXMLParserAddImpliedAttributes = (1 << 5),
    kCFXMLParserAllOptions = 0x00FFFFFF,
    kCFXMLParserNoOptions = 0
} CFXMLParserOptions;

/* This list is tentative and expected to change */
typedef enum {
    kCFXMLStatusParseNotBegun = -2,
    kCFXMLStatusParseInProgress = -1,
    kCFXMLStatusParseSuccessful = 0,
    kCFXMLErrorUnexpectedEOF = 1,
    kCFXMLErrorUnknownEncoding,
    kCFXMLErrorEncodingConversionFailure,
    kCFXMLErrorMalformedProcessingInstruction,
    kCFXMLErrorMalformedDTD,
    kCFXMLErrorMalformedName,
    kCFXMLErrorMalformedCDSect,
    kCFXMLErrorMalformedCloseTag,
    kCFXMLErrorMalformedStartTag,
    kCFXMLErrorMalformedDocument,
    kCFXMLErrorElementlessDocument,
    kCFXMLErrorMalformedComment,
    kCFXMLErrorMalformedCharacterReference,
    kCFXMLErrorMalformedParsedCharacterData
} CFXMLStatus;

/* Type codes for the different possible XML data structures.*/
typedef enum {
    kCFXMLDataTypeDocument = 1,
    kCFXMLDataTypeElement = 2,
    kCFXMLDataTypeAttribute = 3,
    kCFXMLDataTypeProcessingInstruction = 4,
    kCFXMLDataTypeComment = 5,
    kCFXMLDataTypeText = 6,
    kCFXMLDataTypeCDATASection = 7,
    kCFXMLDataTypeDocumentFragment = 8,
    kCFXMLDataTypeEntity = 9,  
    kCFXMLDataTypeEntityReference = 10,
    kCFXMLDataTypeDocumentType = 11, 
    kCFXMLDataTypeWhitespace = 12,
    kCFXMLDataTypeNotation = 13,
    kCFXMLDataTypeElementTypeDeclaration = 14,
    kCFXMLDataTypeAttributeListDeclaration = 15
} CFXMLDataTypeID;

/* All of these structures are preliminary and expected to change */
typedef struct {
    CFDictionaryRef attributes;
    CFArrayRef attributeOrder;
    Boolean isEmpty;
} CFXMLElementData;

typedef struct {
    CFStringRef dataString;
} CFXMLProcessingInstructionData;

typedef struct {
    CFURLRef sourceURL;
    CFStringEncoding encoding;
} CFXMLDocumentData;

typedef struct {
    CFURLRef systemID;
    CFStringRef publicID;
} CFXMLExternalID;

typedef CFXMLExternalID CFXMLDocumentTypeData;

typedef struct {
    /* This is expected to change */
    CFStringRef contentDescription;
} CFXMLElementTypeDeclarationData;

typedef struct {
    /* This is expected to change */
    CFStringRef attributeName;
    CFStringRef typeString;
    CFStringRef defaultString;
} CFXMLAttributeDeclarationData;

typedef struct {
    CFXMLAttributeDeclarationData *attributes; /* each element is an CFXMLAttributeDeclarationData */
    CFIndex numberOfAttributes;
} CFXMLAttributeListDeclarationData;

typedef enum {
    kCFXMLEntityTypeParameter,       /* Implies parsed, internal */
    kCFXMLEntityTypeParsedInternal,
    kCFXMLEntityTypeParsedExternal,
    kCFXMLEntityTypeUnparsed,
    kCFXMLEntityTypeCharacter
} CFXMLEntityTypeID;

typedef struct {
    CFXMLEntityTypeID entityType;
    CFStringRef replacementText;     /* NULL if entityType is external or unparsed */
    CFXMLExternalID entityID;          /* entityID.systemID will be NULL if entityType is internal */
    CFStringRef notationName;        /* NULL if entityType is parsed */
} CFXMLEntityData;

/* See table below for a mapping from CFXMLDataTypeID type codes to the meaning of dataString and structure of additionalData */
typedef struct {
    CFXMLDataTypeID dataTypeID;
    CFStringRef dataString;
    void *additionalData;
    void *_reserved;
} CFXMLDataTypeDescription;

/*
 dataTypeID                         meaning of dataString                format of additionalData
 ===========                        =====================                ========================
 kCFXMLDataTypeDocument             <currently unused>                   CFXMLDocumentData *
 kCFXMLDataTypeElement              tag name                             CFXMLElementData *
 kCFXMLDataTypeAttribute            <currently unused>                   <currently unused>
 kCFXMLDataTypeProcessInstruction   name of the target                   CFXMLProcessingInstructionData *
 kCFXMLDataTypeComment              text of the comment                  NULL
 kCFXMLDataTypeText                 the text's contents                  NULL
 kCFXMLDataTypeCDATASection         text of the CDATA                    NULL
 kCFXMLDataTypeDocumentFragment     <currently unused>                   <currently unused>
 kCFXMLDataTypeEntity               name of the entity                   CFXMLEntityData *
 kCFXMLDataTypeEntityReference      name of the referenced entity        CFXMLEntityTypeID *
 kCFXMLDataTypeDocumentType         name given as top-level element      CFXMLExternalID *
 kCFXMLDataTypeWhitespace           text of the whitespace               NULL
 kCFXMLDataTypeNotation             notation name                        CFXMLExternalID *
 kCFXMLDataTypeElementTypeDeclaration     tag name                       CFXMLElementTypeDeclarationData *
 kCFXMLDataTypeAttributeListDeclaration   tag name                       CFXMLAttributeListDeclarationData *
*/

/*  These functions are called as a parse progresses.

createXMLStructure -
  called as new XML structures are encountered by the parser.  May return NULL to indicate
  that the given structure should be skipped; if NULL is returned for a given structure,
  only minimal parsing is done for that structure (enough to correctly determine its end,
  and to extract any data necessary for the remainder of the parse, such as Entity definitions).
  createXMLStructure (or indeed, any of the tree-creation callbacks) will not be called for any
  children of the skipped structure.  The only exception is that the top-most element will always
  be reported even if NULL was returned for the document as a whole.  Any values in desc that
  are to be kept should be copied - this includes CFStrings and other CF objects within the
  CFXMLDataTypeDescription structure.  It is NOT sufficient to retain such objects; they must
  be copied; a convenience function (CFXMLCopyDataTypeDescription()) is provided to perform
  the necessary copies.

addChild -
  called as children are parsed and are ready to be added to the tree.  If createXMLStructure
  returns NULL for a given structure, that structure is omitted entirely, and addChild will
  NOT be called for either a NULL child or parent.

endXMLStructure -
  called once a structure (and all its children) are completely parsed.  As elements are encountered,
  createXMLStructure is called for them first, then addChild to add the new structure to its parent,
  then addChild (potentially several times) to add the new structure's children to it, then finally 
  endXMLStructure to show that the structure has been fully parsed.

createXMLStructure, addChild, and endXMLStructure are all REQUIRED TO BE NON-NULL.

resolveExternalEntity -
  called when external entities are referenced (NOT when they are simply defined).  If the function
  pointer is NULL, the parser uses its internal routines to try and resolve the entity.  If the
  function pointer is set, and the function returns NULL, a place holder for the external entity
  is inserted into the tree.  In this manner, the parser's client can prevent any external network 
  or file accesses.

handleError - called as errors/warnings are encountered in the data stream.  At some point, we will
  have an enum of the expected errors, some of which will be fatal, others of which will not.  If
  the function pointer is NULL, the parser will silently attempt to recover.  The
  handleError function may always return FALSE to force the parser to stop; if handleError returns
  TRUE, the parser will attempt to recover (fatal errors will still cause the parse to abort
  immediately).
*/

typedef void *		(*CFXMLParserCreateXMLStructureCallBack)(CFXMLParserRef parser, CFXMLDataTypeDescription *desc, void *context);
typedef void		(*CFXMLParserAddChildCallBack)(CFXMLParserRef parser, void *parent, void *child, void *context);
typedef void		(*CFXMLParserEndXMLStructureCallBack)(CFXMLParserRef parser, void *xmlType, void *context);
typedef CFDataRef	(*CFXMLParserResolveExternalEntityCallBack)(CFXMLParserRef parser, CFStringRef publicID, CFURLRef systemID, void *context);
typedef Boolean		(*CFXMLParserHandleErrorCallBack)(CFXMLParserRef parser, SInt32 error, void *context);
typedef struct {
    CFIndex version;
    CFXMLParserCreateXMLStructureCallBack    createXMLStructure;
    CFXMLParserAddChildCallBack              addChild;
    CFXMLParserEndXMLStructureCallBack       endXMLStructure;
    CFXMLParserResolveExternalEntityCallBack resolveExternalEntity;
    CFXMLParserHandleErrorCallBack           handleError;
} CFXMLParserCallBacks;

/* Convenience to parser clients that duplicates the contents of source in destination; takes care
   of copying all internal CFTypes properly.  NOTE: CFXMLDataTypeDescriptions created this way
   MUST be destroyed using CFXMLDestroyDataTypeDescription(), below, which must be provided with
   the same allocator.  All CFTypes contained within the returned CFXMLDataTypeDescription have
   been retained, and must be released before being changed.  allocator may NOT be NULL.  Client
   must ensure that the allocator is still valid when CFXMLDestroyDataTypeDescription() is called.
*/
CF_EXPORT
CFXMLDataTypeDescription *CFXMLCreateDataTypeDescriptionCopy(CFAllocatorRef allocator, const CFXMLDataTypeDescription *source);

CF_EXPORT
void CFXMLDestroyDataTypeDescription(CFXMLDataTypeDescription *desc);

/* Creates a parser which will parse the given data with the given options.
   dataSource should be the URL from which the data came; it is used to resolve any
   relative references found in xmlData */
CF_EXPORT
CFXMLParserRef CFXMLParserCreate(CFAllocatorRef allocator, CFDataRef xmlData, CFURLRef dataSource, CFOptionFlags options);

CF_EXPORT
void CFXMLParserSetContext(CFXMLParserRef parser, void *context);

CF_EXPORT
void *CFXMLParserGetContext(CFXMLParserRef parser);

CF_EXPORT
void CFXMLParserSetCallBacks(CFXMLParserRef parser, CFXMLParserCallBacks *callBacks);

CF_EXPORT
void CFXMLParserGetCallBacks(CFXMLParserRef parser, CFXMLParserCallBacks *callBacks);

CF_EXPORT
CFURLRef CFXMLParserGetSourceURL(CFXMLParserRef parser);

/* Returns the character index or line number of the current parse location */
CF_EXPORT
CFIndex CFXMLParserGetLocation(CFXMLParserRef parser);

CF_EXPORT
CFIndex CFXMLParserGetLineNumber(CFXMLParserRef parser);

/* Returns the top-most object returned by the createXMLStructure callback */
CF_EXPORT
void *CFXMLParserGetDocument(CFXMLParserRef parser);

/* Get the status code or a user-readable description of the last error that occurred in a parse.
   If no error has occurred, a null description string is returned.  See the enum above for
   possible status returns */
CF_EXPORT
SInt32 CFXMLParserGetStatusCode(CFXMLParserRef parser);

CF_EXPORT
CFStringRef CFXMLParserCopyErrorDescription(CFXMLParserRef parser);

/* Cause any in-progress parse to abort with the given error code and description.  errorCode
   must be positive, and errorDescription may not be NULL.  Cannot be called asynchronously
   (i.e. must be called from within a parser callback) */
CF_EXPORT
void CFXMLParserAbort(CFXMLParserRef parser, SInt32 errorCode, CFStringRef errorDescription);

/* Starts a parse of the data the parser was created with; returns success or failure.
   Upon success, use CFXMLParserGetDocument() to get the product of the parse.  Upon
   failure, use CFXMLParserGetErrorCode() or CFXMLParserCopyErrorDescription() to get
   information about the error.  It is an error to call CFXMLParserParse() while a
   parse is already underway. */
CF_EXPORT
Boolean CFXMLParserParse(CFXMLParserRef parser);

/* CFXMLTree */

/* These functions provide a higher-level interface.  The XML data is parsed to a
   special CFTree (an CFXMLTree) with known contexts and callbacks.  The nodes of
   the tree may be queried either via the basic CFTree interface (to report on the
   structure of the tree itself), or via the functions here (to report on the XML
   contents of the nodes)
*/
typedef CFTreeRef CFXMLTreeRef;

/* Creates a childless node from desc */
CF_EXPORT
CFXMLTreeRef CFXMLTreeCreateFromDescription(CFAllocatorRef allocator, CFXMLDataTypeDescription *desc);

CF_EXPORT
const CFXMLDataTypeDescription *CFXMLTreeGetDescription(CFXMLTreeRef xmlNode);

CF_EXPORT
CFXMLDataTypeID CFXMLTreeGetDataType(CFXMLTreeRef xmlNode);

CF_EXPORT
CFStringRef CFXMLTreeGetString(CFXMLTreeRef xmlNode);

/* Parse to an CFXMLTreeRef.  parseOptions are as above. */
CF_EXPORT
CFXMLTreeRef CFXMLTreeCreateFromData(CFAllocatorRef allocator, CFDataRef xmlData, CFURLRef dataSource, CFOptionFlags parseOptions);

/* Generate the XMLData (ready to be written to whatever permanent storage is to be
   used) from an CFXMLTree.  Will NOT regenerate entity references (except those
   required for syntactic correctness) if they were replaced at the parse time;
   clients that wish this should walk the tree and re-insert any entity references
   that should appear in the final output file. */
CF_EXPORT
CFDataRef CFXMLTreeCreateXMLData(CFAllocatorRef allocator, CFXMLTreeRef xmlTree);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFXMLPARSER__ */


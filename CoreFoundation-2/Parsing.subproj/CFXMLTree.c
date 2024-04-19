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
#include "CFInternal.h"
#include <CoreFoundation/CFXMLParser.h>

/*************/
/* CFXMLTree */
/*************/

static CFStringRef _describeXMLData(const void *info) {
    const CFXMLDataTypeDescription *desc = (const CFXMLDataTypeDescription *)info;
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("CFXMLDataTypeDescription 0x%x>{typeID = %d, string = %@}"), (UInt32)desc, desc->dataTypeID, desc->dataString);
}

CF_INLINE Boolean _nullSafeCFEqual(CFTypeRef cf1, CFTypeRef cf2) {
    if (cf1 && !cf2) return FALSE;
    if (cf2 && !cf1) return FALSE;
    if (cf1) return CFEqual(cf1, cf2);
    return TRUE;
}

static Boolean externalIDEqual(CFXMLExternalID *ext1, CFXMLExternalID *ext2) {
    return _nullSafeCFEqual(ext1->systemID, ext2->systemID) && _nullSafeCFEqual(ext1->publicID, ext2->publicID);
}

static Boolean _XMLDataEqual(const void *info1, const void *info2) {
    const CFXMLDataTypeDescription *desc1 = (const CFXMLDataTypeDescription *)info1, *desc2 = (const CFXMLDataTypeDescription *)info2;
    if (desc1 == desc2) return TRUE;
    if (!desc1 || !desc2) return FALSE;
    if (desc1->dataTypeID != desc2->dataTypeID) return FALSE;
    if ((desc1->dataString && !desc2->dataString) || (!desc1->dataString && desc2->dataString)) return FALSE;
    if (desc1->dataString && !CFEqual(desc1->dataString, desc2->dataString)) return FALSE;
    if ((desc1->additionalData && !desc2->additionalData) || (!desc1->additionalData && desc2->additionalData)) return FALSE;
    if (!desc1->additionalData) return TRUE;
    switch (desc1->dataTypeID) {
        case kCFXMLDataTypeDocument:{
            CFURLRef url1, url2;
            url1 = ((CFXMLDocumentData *)desc1->additionalData)->sourceURL;
            url2 = ((CFXMLDocumentData *)desc2->additionalData)->sourceURL;
            return _nullSafeCFEqual(url1, url2);
        }
        case kCFXMLDataTypeElement: {
            CFXMLElementData *elt1, *elt2;
            elt1 = (CFXMLElementData *)desc1->additionalData;
            elt2 = (CFXMLElementData *)desc2->additionalData;
            if (elt1->isEmpty != elt2->isEmpty) return FALSE;
            if (elt1->attributes == elt2->attributes) return TRUE;
            if (!elt1->attributes) return (CFDictionaryGetCount(elt2->attributes) == 0);
            if (!elt2->attributes) return (CFDictionaryGetCount(elt1->attributes) == 0);
            return CFEqual(elt1->attributes, elt2->attributes);
        }
        case kCFXMLDataTypeProcessingInstruction: {
            CFStringRef str1, str2;
            str1 = ((CFXMLProcessingInstructionData *)desc1->additionalData)->dataString;
            str2 = ((CFXMLProcessingInstructionData *)desc2->additionalData)->dataString;
            return _nullSafeCFEqual(str1, str2);
        }
        case kCFXMLDataTypeEntity: {
            CFXMLEntityData *data1, *data2;
            data1 = (CFXMLEntityData *)desc1->additionalData;
            data2 = (CFXMLEntityData *)desc2->additionalData;
            if (data1->entityType != data2->entityType) return FALSE;
            if (!_nullSafeCFEqual(data1->replacementText, data2->replacementText)) return FALSE;
            if (!_nullSafeCFEqual(data1->notationName, data2->notationName)) return FALSE;
            return externalIDEqual(&data1->entityID, &data2->entityID);
        }
        case kCFXMLDataTypeEntityReference: {
            return *((CFXMLEntityTypeID *)(desc1->additionalData)) == *((CFXMLEntityTypeID *)(desc2->additionalData));
        }
        case kCFXMLDataTypeNotation:
        case kCFXMLDataTypeDocumentType: {
            return externalIDEqual(desc1->additionalData, desc2->additionalData);
        }
        case kCFXMLDataTypeElementTypeDeclaration: {
            CFXMLElementTypeDeclarationData *d1 = (CFXMLElementTypeDeclarationData *)desc1->additionalData;
            CFXMLElementTypeDeclarationData *d2 = (CFXMLElementTypeDeclarationData *)desc2->additionalData;
            return _nullSafeCFEqual(d1->contentDescription, d2->contentDescription);
        }
        case kCFXMLDataTypeAttributeListDeclaration: {
            CFXMLAttributeListDeclarationData *attList1 = (CFXMLAttributeListDeclarationData *)desc1->additionalData;
            CFXMLAttributeListDeclarationData *attList2 = (CFXMLAttributeListDeclarationData *)desc2->additionalData;
            CFIndex index;
            if (attList1->numberOfAttributes != attList2->numberOfAttributes) return FALSE;
            for (index = 0; index < attList1->numberOfAttributes; index ++) {
                CFXMLAttributeDeclarationData *attr1 = &(attList1->attributes[index]);
                CFXMLAttributeDeclarationData *attr2 = &(attList2->attributes[index]);
                if (!_nullSafeCFEqual(attr1->attributeName, attr2->attributeName)) return FALSE;
                if (!_nullSafeCFEqual(attr1->typeString, attr2->typeString)) return FALSE;
                if (!_nullSafeCFEqual(attr1->defaultString, attr2->defaultString)) return FALSE;
            }
            return TRUE;
        }
        default:
            return FALSE;
    }
    return TRUE;
}

// We will probably ultimately want to export this under some public API name
CF_EXPORT Boolean CFXMLTreeEqual(CFXMLTreeRef xmlTree1, CFXMLTreeRef xmlTree2) {
    const CFXMLDataTypeDescription *desc1, *desc2;
    CFXMLTreeRef child1, child2;
    if (CFTreeGetChildCount(xmlTree1) != CFTreeGetChildCount(xmlTree2)) return FALSE;
    desc1 = CFXMLTreeGetDescription(xmlTree1);
    desc2 = CFXMLTreeGetDescription(xmlTree2);
    if (!_XMLDataEqual(desc1, desc2)) return FALSE;
    for (child1 = CFTreeGetFirstChild(xmlTree1), child2 = CFTreeGetFirstChild(xmlTree2); child1 && child2; child1 = CFTreeGetNextSibling(child1), child2 = CFTreeGetNextSibling(child2)) {
        if (!CFXMLTreeEqual(child1, child2)) return FALSE;
    }
    return TRUE;
}

CFXMLTreeRef CFXMLTreeCreateFromDescription(CFAllocatorRef allocator, CFXMLDataTypeDescription *desc) {
    CFTreeContext ctxt;
    CFAssert1(desc != NULL, __kCFLogAssertion, "%s(): NULL data type description not allowed", __PRETTY_FUNCTION__);
#if defined(DEBUG)
    if (allocator) {
        __CFGenericValidateType(allocator, __kCFAllocatorTypeID);
    }
#endif
    ctxt.info = CFXMLCreateDataTypeDescriptionCopy(allocator, desc);
    ctxt.retain = NULL;
    ctxt.release = (void (*)(const void *))CFXMLDestroyDataTypeDescription;
    ctxt.copyDescription = _describeXMLData;
    return CFTreeCreate(allocator, &ctxt);
}

const CFXMLDataTypeDescription *CFXMLTreeGetDescription(CFXMLTreeRef xmlNode) {
    CFTreeContext ctxt;
    __CFGenericValidateType(xmlNode, __kCFTreeTypeID);
    ctxt.version = 0;
    CFTreeGetContext(xmlNode, &ctxt);
    return ctxt.info;
}

CFXMLDataTypeID CFXMLTreeGetDataType(CFXMLTreeRef xmlNode) {
    CFTreeContext ctxt;
    CFXMLDataTypeDescription *desc;
    __CFGenericValidateType(xmlNode, __kCFTreeTypeID);
    CFTreeGetContext(xmlNode, &ctxt);
    desc = ctxt.info;
    return desc->dataTypeID;
}

CFStringRef CFXMLTreeGetString(CFXMLTreeRef xmlNode) {
    CFTreeContext ctxt;
    CFXMLDataTypeDescription *desc;
    __CFGenericValidateType(xmlNode, __kCFTreeTypeID);
    CFTreeGetContext(xmlNode, &ctxt);
    desc = ctxt.info;
    return desc->dataString;
}

static void *_XMLTreeCreateXMLStructure(CFXMLParserRef parser, CFXMLDataTypeDescription *desc, void *context) {
    return CFXMLTreeCreateFromDescription(CFGetAllocator(parser), desc);
}

static void _XMLTreeAddChild(CFXMLParserRef parser, void *parent, void *child, void *context) {
    CFTreeAppendChild((CFTreeRef)parent, (CFTreeRef)child);
}

static void _XMLTreeEndXMLStructure(CFXMLParserRef parser, void *xmlType, void *context) {
    CFXMLTreeRef node = (CFXMLTreeRef)xmlType;
    if (CFTreeGetParent(node))
        CFRelease((CFXMLTreeRef)xmlType);
}

CFXMLTreeRef CFXMLTreeCreateFromData(CFAllocatorRef allocator, CFDataRef xmlData, CFURLRef dataSource, UInt32 parseOptions) {
    CFXMLParserRef parser;
    CFXMLParserCallBacks callbacks;
    CFXMLTreeRef result;

    __CFGenericValidateType(xmlData, __kCFDataTypeID);
    CFAssert1(dataSource == NULL || CFGetTypeID(dataSource) == __kCFURLTypeID, __kCFLogAssertion, "%s(): dataSource is not a valid CFURL", __PRETTY_FUNCTION__);
          
    parser = CFXMLParserCreate(allocator, xmlData, dataSource, parseOptions);
    callbacks.createXMLStructure = _XMLTreeCreateXMLStructure;
    callbacks.addChild = _XMLTreeAddChild;
    callbacks.endXMLStructure = _XMLTreeEndXMLStructure;
    callbacks.resolveExternalEntity = NULL;
    callbacks.handleError = NULL;
    
    CFXMLParserSetContext(parser, NULL);
    CFXMLParserSetCallBacks(parser, &callbacks);
    if (CFXMLParserParse(parser)) {
        result = (CFXMLTreeRef)CFXMLParserGetDocument(parser);
    } else {
        result = (CFXMLTreeRef)CFXMLParserGetDocument(parser);
        if (result) CFRelease(result);
        result = NULL;
    }
    CFRelease(parser);
    return result;
}

static void _CFAppendXML(CFMutableStringRef str, CFXMLTreeRef tree);
static void _CFAppendXMLProlog(CFMutableStringRef str, const CFXMLDataTypeDescription *desc);
static void _CFAppendXMLEpilog(CFMutableStringRef str, const CFXMLDataTypeDescription *desc);

CFDataRef CFXMLTreeCreateXMLData(CFAllocatorRef allocator, CFXMLTreeRef xmlTree) {
    CFMutableStringRef xmlStr; 
    CFDataRef result;
    CFStringEncoding encoding;
    const CFXMLDataTypeDescription *desc;

    __CFGenericValidateType(xmlTree, __kCFTreeTypeID);
    
    xmlStr = CFStringCreateMutable(allocator, 0);
    desc = CFXMLTreeGetDescription(xmlTree);
    _CFAppendXML(xmlStr, xmlTree);
    if (desc->dataTypeID == kCFXMLDataTypeDocument) {
        encoding = ((CFXMLDocumentData *)desc->additionalData)->encoding;
    } else {
        encoding = kCFStringEncodingUTF8;
    }
    result = CFStringCreateExternalRepresentation(allocator, xmlStr, encoding, 0);
    CFRelease(xmlStr);
    return result;
}

static void _CFAppendXML(CFMutableStringRef str, CFXMLTreeRef tree) {
    const CFXMLDataTypeDescription *desc = CFXMLTreeGetDescription(tree);
    CFXMLTreeRef child;
    _CFAppendXMLProlog(str, desc);
    for (child = CFTreeGetFirstChild(tree); child; child = CFTreeGetNextSibling(child)) {
        _CFAppendXML(str, child);
    }
    _CFAppendXMLEpilog(str, desc);
}

void appendQuotedString(CFMutableStringRef str, CFStringRef strToQuote) {
    char quoteChar = CFStringFindWithOptions(strToQuote, CFSTR("\""), CFRangeMake(0, CFStringGetLength(strToQuote)), 0, NULL) ? '\'' : '\"';
    CFStringAppendFormat(str, NULL, CFSTR("%c%@%c"), quoteChar, strToQuote, quoteChar);
}

static void appendExternalID(CFMutableStringRef str, CFXMLExternalID *extID) {
    if (extID->publicID) {
        CFStringAppendCString(str, " PUBLIC ", kCFStringEncodingASCII);
        appendQuotedString(str, extID->publicID);
        if (extID->systemID) {
            // Technically, for externalIDs, systemID must not be NULL.  However, by testing for a NULL systemID, we can use this to emit publicIDs, too.  REW, 2/15/2000
            CFStringAppendCString(str, " ", kCFStringEncodingASCII);
            appendQuotedString(str, CFURLGetString(extID->systemID));
        }
    } else if (extID->systemID) {
        CFStringAppendCString(str, " SYSTEM ", kCFStringEncodingASCII);
        appendQuotedString(str, CFURLGetString(extID->systemID));
    } else {
        // Should never get here
    }
}

static void appendElementProlog(CFMutableStringRef str, const CFXMLDataTypeDescription *desc) {
    CFXMLElementData *data = (CFXMLElementData *)desc->additionalData;
    CFStringAppendFormat(str, NULL, CFSTR("<%@"), desc->dataString);
    if (data->attributeOrder) {
        CFIndex i, c = CFArrayGetCount(data->attributeOrder);
        for (i = 0; i < c; i ++) {
            CFStringRef attr = CFArrayGetValueAtIndex(data->attributeOrder, i);
            CFStringRef value = CFDictionaryGetValue(data->attributes, attr);
            CFStringAppendFormat(str, NULL, CFSTR(" %@="), attr);
            appendQuotedString(str, value);
        }
    }
    if (data->isEmpty) {
        CFStringAppendCString(str, "/>", kCFStringEncodingASCII);
    } else {
        CFStringAppendCString(str, ">", kCFStringEncodingASCII);
    }
}

/* Although named "prolog", for leafs of the tree, this is the only XML generation function called.  This is why Comments, Processing Instructions, etc. generate their XML during this function.  REW, 2/11/2000 */
static void _CFAppendXMLProlog(CFMutableStringRef str, const CFXMLDataTypeDescription *desc) {
    switch (desc->dataTypeID) {
        case kCFXMLDataTypeDocument:
            break;
        case kCFXMLDataTypeElement:
            appendElementProlog(str, desc);
            break;
        case kCFXMLDataTypeAttribute:
            // Should never be encountered
            break;
        case kCFXMLDataTypeProcessingInstruction: {
            CFXMLProcessingInstructionData *data = (CFXMLProcessingInstructionData *)desc->additionalData;
            CFStringAppendFormat(str, NULL, CFSTR("<?%@ %@?>"), desc->dataString, data->dataString);
            break;
        }
        case kCFXMLDataTypeComment:
            CFStringAppendFormat(str, NULL, CFSTR("<!--%@-->"), desc->dataString);
            break;
        case kCFXMLDataTypeText:
            CFStringAppend(str, desc->dataString);
            break;
        case kCFXMLDataTypeCDATASection:
            CFStringAppendFormat(str, NULL, CFSTR("<![CDATA[%@]]>"), desc->dataString);
            break;
        case kCFXMLDataTypeDocumentFragment:
            break;
        case kCFXMLDataTypeEntity: {
            CFXMLEntityData *data = (CFXMLEntityData *)desc->additionalData;
            CFStringAppendCString(str, "<!ENTITY ", kCFStringEncodingASCII);
            if (data->entityType == kCFXMLEntityTypeParameter) {
                CFStringAppend(str, CFSTR("% "));
            }
            CFStringAppend(str, desc->dataString);
            if (data->replacementText) {
                appendQuotedString(str, data->replacementText);
                CFStringAppendCString(str, ">", kCFStringEncodingASCII);
            } else {
                appendExternalID(str, &(data->entityID));
                if (data->notationName) {
                    CFStringAppendFormat(str, NULL, CFSTR(" NDATA %@"), data->notationName);
                }
                CFStringAppendCString(str, ">", kCFStringEncodingASCII);
            }
                break;
        }
        case kCFXMLDataTypeEntityReference:
            if (*((CFXMLEntityTypeID *)desc->additionalData) == kCFXMLEntityTypeParameter) {
                CFStringAppendFormat(str, NULL, CFSTR("%%%@;"), desc->dataString);
            } else {
                CFStringAppendFormat(str, NULL, CFSTR("&%@;"), desc->dataString);
            }
            break;
        case kCFXMLDataTypeDocumentType:
            CFStringAppendCString(str, "<!DOCTYPE ", kCFStringEncodingASCII);
            CFStringAppend(str, desc->dataString);
            if (desc->additionalData) {
                CFXMLExternalID *extID = (CFXMLExternalID *)desc->additionalData;
                appendExternalID(str, extID);
            }
                CFStringAppendCString(str, " [", kCFStringEncodingASCII);
            break;
        case kCFXMLDataTypeWhitespace:
            CFStringAppend(str, desc->dataString);
            break;
        case kCFXMLDataTypeNotation: {
            CFXMLExternalID *extID = (CFXMLExternalID *)(desc->additionalData);
            CFStringAppendFormat(str, NULL, CFSTR("<!NOTATION %@ "), desc->dataString);
            appendExternalID(str, extID);
                CFStringAppendCString(str, ">", kCFStringEncodingASCII);
            break;
        }
        case kCFXMLDataTypeElementTypeDeclaration:
            CFStringAppendFormat(str, NULL, CFSTR("<!ELEMENT %@ %@>"), desc->dataString, ((CFXMLElementTypeDeclarationData *)desc->additionalData)->contentDescription);
            break;
        case kCFXMLDataTypeAttributeListDeclaration: {
            CFXMLAttributeListDeclarationData *attListData = (CFXMLAttributeListDeclarationData *)desc->additionalData;
            CFIndex index;
            CFStringAppendCString(str, "<!ATTLIST ", kCFStringEncodingASCII);
            CFStringAppend(str, desc->dataString);
            for (index = 0; index < attListData->numberOfAttributes; index ++) {
                CFXMLAttributeDeclarationData *attr = &(attListData->attributes[index]);
                CFStringAppendFormat(str, NULL, CFSTR("\n\t%@ %@ %@"), attr->attributeName, attr->typeString, attr->defaultString);
            }
            CFStringAppendCString(str, ">", kCFStringEncodingASCII);
            break;
        }
        default:
            CFAssert1(FALSE, __kCFLogAssertion, "Encountered unexpected XMLDataTypeID %d", desc->dataTypeID);
    }
}

static void _CFAppendXMLEpilog(CFMutableStringRef str, const CFXMLDataTypeDescription *desc) {
    if (desc->dataTypeID == kCFXMLDataTypeElement) {
        if (((CFXMLElementData *)desc->additionalData)->isEmpty) return;
        CFStringAppendFormat(str, NULL, CFSTR("</%@>"), desc->dataString);
    } else if (desc->dataTypeID == kCFXMLDataTypeDocumentType) {
        CFIndex len = CFStringGetLength(str);
        if (CFStringGetCharacterAtIndex(str, len-1) == '[') {
            // There were no in-line DTD elements
            CFStringDelete(str, CFRangeMake(len-1, 1));
        } else {
            CFStringAppendCString(str, "]", kCFStringEncodingASCII);
        }
        CFStringAppendCString(str, ">", kCFStringEncodingASCII);
    }
}

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
/*
 cc -g -lIOKit IOCFSerializeTest.c -o IOCFSerializeTest

 cc -g IOCFSerializeTest.c -framework CoreFoundation BUILD/obj/objects-optimized/IOCFSerialize.o \
 BUILD/obj/objects-optimized/IOCFUnserialize.tab.o -o IOCFSerializeTest

*/

#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

char *testBuffer = "
 <?xml version=\"1.0\" encoding=\"UTF-8\"?>
 <!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">
 <plist version=\"0.9\">
 <dict>

 <key>key true</key>	<true/>
 <key>key false</key>	<false/>

 <key>key d0</key>	<data> </data>
 <key>key d1</key>	<data>AQ==</data>
 <key>key d2</key>	<data>ASM=</data>
 <key>key d3</key>	<data>ASNF</data>
 <key>key d4</key>	<data ID=\"1\">ASNFZw==</data>

 <key>key i0</key>	<integer></integer>
 <key>key i1</key>	<integer>123456789</integer>
 <key>key i2</key>	<integer size=\"32\" ID=\"2\">0x12345678</integer>

 <key>key s0</key>	<string></string>
 <key>key s1</key>	<string>string 1</string>
 <key>key s2</key>	<string ID=\"3\">string 2</string>
 <key>key &lt;&amp;&gt;</key>	<string>&lt;&amp;&gt;</string>

 <key>key D0</key>	<dict ID=\"4\">
                        </dict>

 <key>key a0</key>	<array>
                        </array>

 <key>key a1</key>	<array ID=\"5\">
                            <string>array string 1</string>
                            <string>array string 2</string>
                        </array>

 <key>key S0</key>	<set>
                        </set>
 <key>key S1</key>	<set ID=\"6\">
                             <string>set string 1</string>
                             <string>set string 2</string>
                         </set>

 <key>key r1</key>	<ref IDREF=\"1\"/>
 <key>key r2</key>	<ref IDREF=\"2\"/>
 <key>key r3</key>	<ref IDREF=\"3\"/>
 <key>key r4</key>	<ref IDREF=\"4\"/>
 <key>key r5</key>	<ref IDREF=\"5\"/>
 <key>key r6</key>	<ref IDREF=\"6\"/>

 </dict>
 </plist>
";


int
main(int argc, char **argv)
{
	CFTypeRef	properties1, properties2, properties3;
	CFDataRef	data1, data2;
	CFStringRef  	errorString;

	printf("testing...\n");

	properties1 = IOCFUnserialize(testBuffer, kCFAllocatorDefault, 0, &errorString);
	if (!properties1) {
		CFIndex	len = CFStringGetLength(errorString) + 1;
		char *buffer = malloc(len);
		if (!buffer || !CFStringGetCString(errorString, buffer, len, 
						   kCFStringEncodingASCII)) {
			exit(1);
		}

		printf("prop1 error: %s\n", buffer);
		CFRelease(errorString);
		exit(1);
	}

	data1 = IOCFSerialize( properties1, kNilOptions );
	if (!data1) {
		printf("serialize on prop1 failed\n");
		exit(1);
	}

	properties2 = IOCFUnserialize((char *)CFDataGetBytePtr(data1), kCFAllocatorDefault, 0, &errorString);
	if (!properties2) {
		CFIndex	len = CFStringGetLength(errorString) + 1;
		char *buffer = malloc(len);
		if (!buffer || !CFStringGetCString(errorString, buffer, len, 
						   kCFStringEncodingASCII)) {
			exit(1);
		}

		printf("prop2 error: %s\n", buffer);
		CFRelease(errorString);
		exit(1);
	}

	if (CFEqual(properties1, properties2)) {
		printf("test successful, prop1 == prop2\n");
	} else {
		printf("test failed, prop1 == prop2\n");
//		printf("%s\n", testBuffer);
//		printf("%s\n", (char *)CFDataGetBytePtr(data1));
		exit(1);
	}

	data2 = IOCFSerialize( properties2, kNilOptions );
	if (!data2) {
		printf("serialize on prop2 failed\n");
		exit(1);
	}

	properties3 = IOCFUnserialize((char *)CFDataGetBytePtr(data2), kCFAllocatorDefault, 0, &errorString);
	if (!properties3) {
		CFIndex	len = CFStringGetLength(errorString) + 1;
		char *buffer = malloc(len);
		if (!buffer || !CFStringGetCString(errorString, buffer, len, 
						   kCFStringEncodingASCII)) {
			exit(1);
		}

		printf("prop3 error: %s\n", buffer);
		CFRelease(errorString);
		exit(1);
	}

	if (CFEqual(properties2, properties3)) {
		printf("test successful, prop2 == prop3\n");
	} else {
		printf("test failed, prop2 == prop3\n");
//		printf("%s\n", (char *)CFDataGetBytePtr(data1));
//		printf("%s\n", (char *)CFDataGetBytePtr(data2));
		exit(1);
	}


	CFRelease(data1);
	CFRelease(data2);
	CFRelease(properties1);
	CFRelease(properties2);
	CFRelease(properties3);

	return 0;
}

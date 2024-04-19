#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <mach/mach_init.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>

#include "GetRegistry.h"

#include "DiskArbitrationServerMain.h"

static char * createCStringFromCFString(CFStringRef string);

void GetDisksFromRegistry(io_iterator_t iter)
{
	kern_return_t		kr;
	io_registry_entry_t	entry;

	io_name_t		ioMediaName;
	UInt32			ioBSDUnit;
	int				ioWhole, ioWritable, ioEjectable, ioLeaf;
	unsigned		flags;

	while ( entry = IOIteratorNext( iter ) )
	{
		char *          ioBSDName  = 0; // (needs release)
		char *          ioContent  = 0; // (needs release)

		CFBooleanRef    boolean    = 0; // (don't release)
		CFNumberRef     number     = 0; // (don't release)
		CFDictionaryRef properties = 0; // (needs release)
		CFStringRef     string     = 0; // (don't release)

		// MediaName

		kr = IORegistryEntryGetName(entry, ioMediaName);
		if ( KERN_SUCCESS != kr )
		{
			if ( gDebug ) printf("can't obtain name for media object\n");
			goto Next;
		}

		// Get Properties

		kr = IORegistryEntryCreateCFProperties(entry, &properties, kCFAllocatorDefault, kNilOptions);
		if ( KERN_SUCCESS != kr )
		{
			if ( gDebug ) printf("can't obtain properties for '%s'\n", ioMediaName);
			goto Next;
		}

		assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

		// BSDName
		
		string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDName));
		if ( ! string )
		{
			/* We're only interested in disks accessible via BSD */
			if ( gDebug ) printf("kIOBSDName property missing for '%s'\n", ioMediaName);
			goto Next;
		}

		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioBSDName = createCStringFromCFString(string);
		assert(ioBSDName);

		if ( gDebug ) printf("ioBSDName = '%s'\n", ioBSDName);

		// BSDUnit
		
		number = (CFNumberRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDUnit));
		if ( ! number )
		{
			/* We're only interested in disks accessible via BSD */
			if ( gDebug ) printf("kIOBSDUnit property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(number) == CFNumberGetTypeID());

		if ( ! CFNumberGetValue(number, kCFNumberSInt32Type, &ioBSDUnit) )
		{
			goto Next;
		}

		if ( gDebug ) printf("ioBSDUnit = %ld\n", ioBSDUnit);

		// Content

		string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaContent));
		if ( ! string )
		{
			if ( gDebug ) printf("kIOMediaContent property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(string) == CFStringGetTypeID());

		ioContent = createCStringFromCFString(string);
		assert(ioContent);

		if ( gDebug ) printf("ioContent = '%s'\n", ioContent);
	
		// Leaf

		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaLeaf));
		if ( ! boolean )
		{
			if ( gDebug ) printf("kIOMediaLeaf property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioLeaf = ( kCFBooleanTrue == boolean );

		if ( gDebug ) printf("ioLeaf = %d\n", ioLeaf);
	
		// Whole

		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWhole));
		if ( ! boolean )
		{
			if ( gDebug ) printf("kIOMediaWhole property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWhole = ( kCFBooleanTrue == boolean );

		if ( gDebug ) printf("ioWhole = %d\n", ioWhole);

		// Writable
	
		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaWritable));
		if ( ! boolean )
		{
			if ( gDebug ) printf("kIOMediaWritable property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioWritable = ( kCFBooleanTrue == boolean );

		if ( gDebug ) printf("ioWritable = %d\n", ioWritable);
	
		// Ejectable

		boolean = (CFBooleanRef) CFDictionaryGetValue(properties, CFSTR(kIOMediaEjectable));
		if ( ! boolean )
		{
			if ( gDebug ) printf("kIOMediaEjectable property missing for '%s'\n", ioBSDName);
			goto Next;
		}

		assert(CFGetTypeID(boolean) == CFBooleanGetTypeID());

		ioEjectable = ( kCFBooleanTrue == boolean );

		if ( gDebug ) printf("ioEjectable = %d\n", ioEjectable);

		/* Construct the <flags> word */
		
		flags = 0;

		if ( ! ioWritable ) 
			flags |= kDiskArbitrationDiskAppearedLockedMask;

		if ( ioEjectable ) 
			flags |= kDiskArbitrationDiskAppearedEjectableMask;

		if ( ioWhole ) 
			flags |= kDiskArbitrationDiskAppearedWholeDiskMask;
		
		if ( ! ioLeaf ) 
			flags |= kDiskArbitrationDiskAppearedNonLeafDiskMask;
		
		{
			DiskPtr dp;
			
			/* Is there an existing disk on our list with this IOBSDName? */
		
			dp = LookupDiskByIOBSDName( ioBSDName );
			if ( dp )
			{
				if ( gDebug ) printf("%s: '%s' already exists\n", __FUNCTION__, ioBSDName);

				/* In case it was accidentally unmounted, mark it for remounting */
				if ( dp->mountpoint && 0==strcmp(dp->mountpoint,"") )
				{
					dp->state = kDiskStateNew;
				}
			}
			else
			{
				/* Create a new disk, leaving the <mountpoint> initialized to NULL */
		
				if ( ! NewDisk( ioBSDName, ioBSDUnit, ioContent, kDiskFamily_SCSI, NULL, ioMediaName, flags ) )
				{
					printf("%s: NewDisk() failed!\n", __FUNCTION__);
				}
				
			}
		}
		
	Next:
	
		if ( properties )	CFRelease( properties );
		if ( ioBSDName )	free( ioBSDName );
		if ( ioContent )	free( ioContent );

		IOObjectRelease( entry );
		
	} /* while */

	if ( gDebug ) PrintDisks();

} /* GetDisksFromRegistry */

char * createCStringFromCFString(CFStringRef string)
{
    /*
     * result of createCStringFromCFString should be released with free()
     */

    CFStringEncoding encoding     = CFStringGetSystemEncoding();
    CFIndex          bufferLength = CFStringGetLength(string) + 1;
    char *           buffer       = malloc(bufferLength);

    if (buffer)
    {
        if (CFStringGetCString(string, buffer, bufferLength, encoding) == FALSE)
        {
            free(buffer);
            buffer = NULL;
        }
    }

    return buffer;
} /* createCStringFromCFString */

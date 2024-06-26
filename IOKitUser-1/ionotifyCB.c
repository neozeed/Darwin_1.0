/*
cc ionotifyCB.c -o /tmp/ion -Wall -Wno-four-char-constants -framework IOKit -undefined warning
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include <mach/mach_interface.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFRunLoop.h>

mach_port_t		masterPort;
mach_port_t		notifyPort;
CFRunLoopSourceRef	cfSource;

void ServiceInterestCallback(void * refcon, io_service_t service,
                            natural_t messageType, void * messageArgument )
{
    io_name_t	name;
    
    assert( refcon == (void *) masterPort );
    
    if( KERN_SUCCESS == IORegistryEntryGetName( service, name ))
        printf(name);
    else
        printf("???");
    printf(": messageType %08lx, arg %08lx\n",
                   messageType, messageArgument);
}

void ServiceArrivalCallback( void * refcon, io_iterator_t iter ) 
{
    kern_return_t	kr;
    io_object_t		obj;
    io_name_t		name;
    io_string_t		path;
    mach_port_t		note;

    IONotificationPortRef notify = (IONotificationPortRef) refcon;
    
    while( (obj = IOIteratorNext( iter))) {
        assert( KERN_SUCCESS == (
        kr = IORegistryEntryGetName( obj, name )
        ));
	printf("name:%s(%d)\n", name, obj);
        kr = IORegistryEntryGetPath( obj, kIOServicePlane, path );
        if( KERN_SUCCESS == kr) {
            
	    // if the object is detached, getPath will fail
            printf("path:%s\n", path);

            // as will IOServiceAddInterestNotification
            if( KERN_SUCCESS != (
                    kr = IOServiceAddInterestNotification(
                            notify,
                            obj, kIOGeneralInterest,
                            &ServiceInterestCallback, (void *) masterPort,
                            &note )
            )) printf("IOServiceAddInterestNotification(%lx)\n", kr);
            // can compare two kernel objects with IOObjectIsEqualTo() if we keep the object,
            // otherwise, release
            IOObjectRelease( obj );
        }
    }
}

void notifyTest( char * arg )
{
    kern_return_t	kr;
    io_iterator_t	note1, note2;
    io_service_t	obj;
    const char *	type;
    IONotificationPortRef notify;
    
    assert( 0 != (
    notify = IONotificationPortCreate( masterPort )
    ));
    
    type = kIOMatchedNotification;// or kIOPublishNotification;
    assert( KERN_SUCCESS == (
    kr = IOServiceAddMatchingNotification(
                        notify,
                        type,
                        IOServiceMatching( arg ),
//                      IOOpenFirmwarePathMatching( masterPort, 0, arg ),
//                      IOBSDNameMatching( masterPort, 0, arg ),
                        &ServiceArrivalCallback, (void *) notify,
                        &note1 )
    ));
    printf("IOServiceAddNotification: %s: ", type );
    // dumping the iterator gives us the current list
    // and arms the notification for changes
    ServiceArrivalCallback ( (void *) notify, note1 );
    printf("\n");

    type = kIOBusyInterest;
    obj = IORegistryEntryFromPath( masterPort, kIOServicePlane ":/");
    assert( obj );
    assert( KERN_SUCCESS == (
    kr = IOServiceAddInterestNotification(
            notify,
            obj, type,
            &ServiceInterestCallback, (void *) masterPort,
            &note2 )
    ));
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(notify),
                       kCFDefaultRunLoopMode);
    printf("waiting...\n");
    CFRunLoopRun();	// Start silly state machine

    IOObjectRelease( note1 );
    IOObjectRelease( note2 );
}

int
main(int argc, char **argv)
{
	kern_return_t		kr;

	/*
	 * Get master device port
	 */
	assert( KERN_SUCCESS == (
	kr = IOMasterPort(   bootstrap_port,
			     &masterPort)
	));

	if( argc > 1)
            notifyTest( argv[1] );
	else
	    printf("%s className\n", argv[0]);

	printf("Exit\n");
}


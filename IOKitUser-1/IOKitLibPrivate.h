
enum {
    kIOAsyncReservedIndex 	= 0,
    
    kIOAsyncCalloutFuncIndex 	= 1,
    kIOAsyncCalloutRefconIndex,
    kIOAsyncCalloutCount,

    kIOMatchingCalloutFuncIndex	= 1,
    kIOMatchingCalloutRefconIndex,
    kIOMatchingCalloutCount,
    
    kIOInterestCalloutFuncIndex	= 1,
    kIOInterestCalloutRefconIndex,
    kIOInterestCalloutServiceIndex,
    kIOInterestCalloutCount
};

struct IONotificationPort
{
    mach_port_t		masterPort;
    mach_port_t		wakePort;
    CFRunLoopSourceRef	source;
};
typedef struct IONotificationPort IONotificationPort;

CFMutableDictionaryRef
IOMakeMatching(
	mach_port_t	masterPort,
	unsigned int	type,
	unsigned int	options,
	void *		args,
	unsigned int	argsSize );

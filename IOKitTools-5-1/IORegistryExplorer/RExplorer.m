#import "RExplorer.h"

mach_port_t gMasterPort;

@implementation RExplorer

static void addChildrenOfPlistToMapsRecursively(id plist, NSMapTable *_parentMap, NSMapTable *_keyMap) {
    // Adds all the children of the given plist to the map-tables.  Does not add the plist itself.
    if ([plist isKindOfClass:[NSDictionary class]]) {
        NSEnumerator *keyEnum = [plist keyEnumerator];
        NSString *curKey;
        id curChild;


        while ((curKey = [keyEnum nextObject]) != nil) {
            curChild = [plist objectForKey:curKey];

            NSMapInsert(_parentMap, curChild, plist);
            NSMapInsert(_keyMap, curChild, curKey);
            addChildrenOfPlistToMapsRecursively(curChild, _parentMap, _keyMap);
        }
    } else if ([plist isKindOfClass:[NSArray class]]) {
        unsigned i, c = [plist count];
        id curChild;

        for (i=0; i<c; i++) {
            curChild = [plist objectAtIndex:i];
            NSMapInsert(_parentMap, curChild, plist);
            NSMapInsert(_keyMap, curChild, [NSString stringWithFormat:@"%u", i] );
            addChildrenOfPlistToMapsRecursively(curChild, _parentMap, _keyMap);
        }
    }
}

void dumpIter( io_iterator_t iter )
{
    kern_return_t	kr;
    io_object_t		obj;

    while( (obj = IOIteratorNext( iter))) {
#ifdef DEBUG
        io_name_t	name;
        io_string_t	path;

        kr = IORegistryEntryGetName( obj, name );
        printf("name:%s(%d)\n", (kr == KERN_SUCCESS) ? name : "<error>", obj);

        // if the object is detached, getPath is expected to fail
        kr = IORegistryEntryGetPath( obj, kIOServicePlane, path );
        printf("path:%s\n", (kr == KERN_SUCCESS) ? path : "<error>", obj);
#endif DEBUG
        IOObjectRelease( obj );
    }
}

- (id)init
{
    kern_return_t       status;
    kern_return_t	kr;
    io_iterator_t	iter1, iter2;
    const char *	type;


    self = [super init];

    // Obtain the I/O Kit communication handle.
    status = IOMasterPort(bootstrap_port, &gMasterPort);
    assert(status == KERN_SUCCESS);

    assert( KERN_SUCCESS == (
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port)
    ));

    type = kIOFirstMatchNotification;
    assert( KERN_SUCCESS == (
                             kr = IOServiceAddNotification( gMasterPort, type,
                                                            IOServiceMatching( "IOService" ),
                                    port, (unsigned int) type, &iter2 )
    ));
    dumpIter( iter2 ); // dumping the iterator arms the notification for changes


    type = kIOTerminatedNotification;
    assert( KERN_SUCCESS == (
                             kr = IOServiceAddNotification( gMasterPort, type,
                                    IOServiceMatching( "IOService" ),
                                    port, (unsigned int) type, &iter1 )
    ));
    dumpIter( iter1 ); // dumping the iterator arms the notification for changes


    // create a timer to check for hardware additions/removals, etc.
    updateTimer = [[NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(checkForUpdate:) userInfo:nil repeats:YES] retain];

    return self;
}

- (void)awakeFromNib
{
    [self initializeRegistryDictionaryWithPlane:kIOServicePlane];
    [splitView setVertical:NO];
    [propertiesOutlineView setDelegate:self];
    [propertiesOutlineView setDataSource:self];
    [browser setDelegate:self];
    [planeBrowser setDelegate:self];
    [window setDelegate:self];

    keyColumn = [[propertiesOutlineView tableColumns] objectAtIndex:0];
    valueColumn = [[propertiesOutlineView tableColumns] objectAtIndex:1];
    
    [window setFrameAutosaveName:@"MainWindow"];
    [window setFrameUsingName:@"MainWindow"];
    [planeWindow setFrameAutosaveName:@"PlaneWindow"];
    [planeWindow setFrameUsingName:@"PlaneWindow"];
    [inspectorWindow setFrameAutosaveName:@"InspectorWindow"];
    [inspectorWindow setFrameUsingName:@"InspectorWindow"];

    [browser setPathSeparator:@":"];
        
    return;
}

- (void)dealloc
{
    [currentSelectedItemDict release];
    [updateTimer release];
    [super dealloc];
    return;
}

- (NSDictionary *)dictForIterated:(io_registry_entry_t)passedEntry
{
    kern_return_t       status;
    io_name_t		name;

    io_registry_entry_t iterated;
    io_iterator_t       regIterator;
    NSMutableDictionary 	*localDict = [NSMutableDictionary dictionary];
    NSMutableArray 		*localArray = [NSMutableArray array];
    NSDictionary	*propertiesDict = nil;
    CFDictionaryRef	dictionary; ///ok

    status = IORegistryEntryGetChildIterator(passedEntry, currentPlane, &regIterator);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryGetName(passedEntry, name);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryCreateCFProperties(passedEntry,
                                    &dictionary,
                                    kCFAllocatorDefault, kNilOptions);
    assert( KERN_SUCCESS == status );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(dictionary));


    propertiesDict = dictionary;

    while ( (iterated = IOIteratorNext(regIterator)) != NULL ) {
        id insideDict = [self dictForIterated:iterated];
        [localArray addObject:insideDict];
    }

    
    [localDict setObject:localArray forKey:@"children"];
    [localDict setObject:[NSString stringWithCString:name] forKey:@"name"];
    [localDict setObject:propertiesDict forKey:@"properties"];


    return [NSDictionary dictionaryWithDictionary:localDict];
}

- (void)initializeRegistryDictionaryWithPlane:(const char *)plane
{

    kern_return_t       status;

    io_registry_entry_t iterated;
    io_iterator_t       regIterator;

    io_name_t		rootName;

    io_registry_entry_t rootEntry;

    NSMutableDictionary *localDict = [NSMutableDictionary dictionary];
    NSMutableArray 		*localArray = [NSMutableArray array];

    NSDictionary	*propertiesDict = nil;
    CFDictionaryRef	dictionary; ///ok

    if (registryDict) {
        [registryDict release];
    }
    registryDict = [[NSMutableDictionary alloc] init];

    currentPlane = plane;


    rootEntry = IORegistryGetRootEntry(gMasterPort);
    status = IORegistryEntryGetName(rootEntry, rootName);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryGetChildIterator(rootEntry, currentPlane, &regIterator);
    assert(status == KERN_SUCCESS);

    status = IORegistryEntryCreateCFProperties(rootEntry,
                                    &dictionary,
                                    kCFAllocatorDefault, kNilOptions);
    assert( KERN_SUCCESS == status );
    assert( CFDictionaryGetTypeID() == CFGetTypeID(dictionary));


    propertiesDict = dictionary;
        
    while ( (iterated = IOIteratorNext(regIterator)) != NULL ) {

        [localArray addObject:[self dictForIterated:iterated]];

    }
        
    // create dictionaries for the properties in the root
    {
        NSEnumerator *propertiesEnum = [[propertiesDict allKeys] objectEnumerator];
        id aProp = nil;
        
        while (aProp = [propertiesEnum nextObject]) {
            NSMutableDictionary *propDict = [NSMutableDictionary dictionary];
            
            [propDict setObject:aProp forKey:@"name"];
            
            if ([aProp isEqual:@"IOCatalogue"]) {
                [propDict setObject:[propertiesDict objectForKey:aProp] forKey:@"children"];
                [propDict setObject:[NSDictionary dictionary] forKey:@"properties"];
            } else if ([aProp isEqual:@"IOKitBuildVersion"]) {
                [propDict setObject:[NSArray array] forKey:@"children"];
                [propDict setObject:[NSDictionary dictionaryWithObjectsAndKeys:[propertiesDict objectForKey:aProp],@"name", nil] forKey:@"properties"];
            } else {
                [propDict setObject:[NSArray array] forKey:@"children"];
                [propDict setObject:[propertiesDict objectForKey:aProp] forKey:@"properties"];
            }

            [localArray addObject:propDict];
        }
    }
            
            
    [localDict setObject:localArray forKey:@"children"];
    [localDict setObject:[NSString stringWithCString:rootName] forKey:@"name"];
    [localDict setObject:propertiesDict forKey:@"properties"];


    [registryDict setObject:localDict forKey:@"Root"];
}

- (void)changeLevel:(id)sender
{
    id object = nil;
    int column = [sender selectedColumn];
    int row = [sender selectedRowInColumn:column];

    if (column < 0 || row < 0)
        return;

    if (column == 0) {
        NSArray *objArray = [[registryDict objectForKey:@"Root"] objectForKey:@"children"];
        object = [objArray objectAtIndex:row];
    } else {
        object = [[self childArrayToIndex:column] objectAtIndex:row];
    }

    if ([object objectForKey:@"properties"]) {
        [inspectorText setString:[[object objectForKey:@"properties"] description]];
        [inspectorText display];

        //[informationView setString:[[object objectForKey:@"properties"] description]];
    } else {
        [inspectorText setString:[object description]];
        [inspectorText display];

        //[informationView setString:[object description]];
    }

    [currentSelectedItemDict release];
    currentSelectedItemDict = [[object objectForKey:@"properties"] retain];
    
    if (!currentSelectedItemDict) {
        currentSelectedItemDict = [object retain];
    }

    // go through and create a uniqued dictionary where all the values are uniqued and all the keys are uniqued
    if ([currentSelectedItemDict count]) {
        int count = [currentSelectedItemDict count];
        int i;
        NSMutableDictionary *newDict = [NSMutableDictionary dictionary];

        NSArray *keyArray = [currentSelectedItemDict allKeys];
        NSArray *valueArray = [currentSelectedItemDict allValues];

        for (i = 0; i < count ; i++) {

            if (CFGetTypeID([currentSelectedItemDict objectForKey:[keyArray objectAtIndex:i]]) == CFBooleanGetTypeID()) {
                if ([[[currentSelectedItemDict objectForKey:[keyArray objectAtIndex:i]] description] isEqualToString:@"0"]) {
                    [newDict setObject:[[[RBool alloc] initWithBool:NO] autorelease] forKey:[keyArray objectAtIndex:i]];
                } else {
                    [newDict setObject:[[[RBool alloc] initWithBool:YES] autorelease] forKey:[keyArray objectAtIndex:i]];
                }

            } else {
                [newDict setObject:[[[valueArray objectAtIndex:i] copy] autorelease] forKey:[keyArray objectAtIndex:i]];
            }
            
        }
        [currentSelectedItemDict release];
        currentSelectedItemDict = [newDict retain];
             
    }

    [self initializeMapsForDictionary:currentSelectedItemDict];


    [propertiesOutlineView reloadData];
    
    return;
}

- (void)dumpDictionaryToOutput:(id)sender
{

    NSLog(@"%@", registryDict);
    return;
}

- (NSArray *)childArrayToIndex:(int)column
{
    int i = 0;
    id lastDict = [registryDict objectForKey:@"Root"];


    for (i = 1; (i <= column); i++) {
        NSArray *localArray = [lastDict objectForKey:@"children"];

        lastDict = [localArray objectAtIndex:[browser selectedRowInColumn:i - 1]];
    }

    return [lastDict objectForKey:@"children"];
}

- (void)displayAboutWindow:(id)sender
{
    [NSApp orderFrontStandardAboutPanelWithOptions:nil];
    return;
}

- (void)displayPlaneWindow:(id)sender
{
    //NSLog(@"%@, %@", planeBrowser, planeWindow);
    [planeBrowser loadColumnZero];
    [planeWindow makeKeyAndOrderFront:self];
    return;
}


- (void)switchRootPlane:(id)sender
{
    [self initializeRegistryDictionaryWithPlane:[[[sender selectedCell] stringValue] cString]];
    [browser loadColumnZero];
    [self changeLevel:browser];
    [currentSelectedItemDict release];
    currentSelectedItemDict = nil;
    [propertiesOutlineView reloadData];
    return;

}

- (void)forceUpdate:(id)sender
{
    NSString *currentPath = [browser path];
    [self initializeRegistryDictionaryWithPlane:currentPlane];
    [browser loadColumnZero];
    [browser setPath:currentPath];
    [self changeLevel:browser];

    return;
}

- (void)registryHasChanged
{
    if (NSAlertDefaultReturn == NSRunInformationalAlertPanel(@"Registry Change", @"The IO Registry has been updated by the addition or termination of a service.\nDo you wish to update your display or skip this update?", @"Update", @"Skip", NULL)) {
        // reload
        [self forceUpdate:self];
    } else {
        // don't reload
    }
}

// Window delegation
- (void)windowWillClose:(id)sender
{
    [NSApp stop:nil];
    return;
}


- (void)initializeMapsForDictionary:(NSDictionary *)dict
{

    if (_parentMap) {
        NSFreeMapTable(_parentMap);
    }
    if (_keyMap) {
        NSFreeMapTable(_keyMap);
    }

    
    _parentMap = NSCreateMapTableWithZone(NSIntMapKeyCallBacks, NSNonRetainedObjectMapValueCallBacks, 100, [self zone]);
    _keyMap = NSCreateMapTableWithZone(NSIntMapKeyCallBacks, NSObjectMapValueCallBacks, 100, [self zone]);


    addChildrenOfPlistToMapsRecursively(dict, _parentMap, _keyMap);

    //NSLog(@"%@", NSStringFromMapTable(_keyMap));

    return;
}

- (void)checkForUpdate:(NSTimer *)timer
{
    kern_return_t	kr;
    unsigned long int  	notifyType;
    unsigned long int  	ref;
    struct {
        mach_msg_header_t	msgHdr;
        OSNotificationHeader	notifyHeader;
        mach_msg_trailer_t	trailer;
    } msg;


    //assert( KERN_SUCCESS == (
    kr = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                  0, sizeof(msg), port, 0, MACH_PORT_NULL);
    //));

    if ( KERN_SUCCESS == kr ) { // if it was not a timeout or error
        assert( KERN_SUCCESS == (
        kr = OSGetNotificationFromMessage( &msg.msgHdr, 0, &notifyType, &ref,
                        0, 0 )
        ));

#ifdef DEBUG
        // we passed a string for the refcon
        printf("got notification, type=%d(%s), local=%d, remote=%d\n", notifyType, ref,
                msg.msgHdr.msgh_local_port, msg.msgHdr.msgh_remote_port );
#endif DEBUG

        [self registryHasChanged];

        // remote port is the notification (an iterator_t) that fired;
        // dumping the iterator re-arms the notification for more changes
        dumpIter( msg.msgHdr.msgh_remote_port );
    }

    return;
}

// search the dictionaries
- (NSArray *)searchResultsForText:(NSString *)text searchKeys:(BOOL)keys searchValues:(BOOL)values
{
    if (!keys && !values) {
        return [NSArray array];
    }

    if (keys) {
        NSMutableArray *array = [NSMutableArray arrayWithArray:[self searchKeysResultsInDictionary:[registryDict objectForKey:@"Root"] forText:text passedPath:@""]];

        // do the root directory stuff here ...
        NSEnumerator *rootEnum = [[[[registryDict objectForKey:@"Root"] objectForKey:@"properties"] allKeys] objectEnumerator];
        id aRootEntry = nil;

        while (aRootEntry = [rootEnum nextObject]) {
            //if ([aRootEntry rangeOfString:text].length > 0) {
            //    [array addObject:[NSString stringWithFormat:@"/Root/%@", aRootEntry]];
            //}
            if ([aRootEntry isEqualToString:@"IOCatalogue"]) {
                // special case
            }
        }

        return [NSArray arrayWithArray:array];
        //NSLog(@"%@", array);
    }
    return [NSArray array];
}

- (NSArray *)searchKeysResultsInDictionary:(NSDictionary *)dict forText:(NSString *)text passedPath:(NSString *)path
{
    NSArray *children = [dict objectForKey:@"children"];
    NSEnumerator *kidEnum = [children objectEnumerator];
    id aKid = nil;
    NSMutableArray *array = [NSMutableArray array];

    if ([[dict objectForKey:@"name"] length]) {
        if ([[[dict objectForKey:@"name"] uppercaseString] rangeOfString:[text uppercaseString]].length > 0) {
            [array addObject:[NSString stringWithFormat:@"%@:%@", path, [dict objectForKey:@"name"]]];
        }
    }

    while (aKid = [kidEnum nextObject]) {
        [array addObjectsFromArray:[self searchKeysResultsInDictionary:aKid forText:text passedPath:[NSString stringWithFormat:@"%@:%@", path, [dict objectForKey:@"name"]]]];
    }

    return [[array copy] autorelease];
}

- (void)goToPath:(NSString *)path
{
    int count = [[path componentsSeparatedByString:@":"] count];

    if (count > 2) {
        count -= 2;
    }
    
    [browser setPath:path];
    [self changeLevel:browser];
    [browser scrollColumnToVisible:count];

    return;
}


@end

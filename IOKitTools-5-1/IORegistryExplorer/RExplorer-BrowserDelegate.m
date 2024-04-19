/* RExplorer-BrowserDelegate.m created by epeyton on Tue 11-Jan-2000 */

#import "RExplorer-BrowserDelegate.h"

@implementation RExplorer (BrowserDelegate)

// browser delegation

- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column
{
    if (sender == browser) {
        if (column == 0 ) {
            int childCount = [[[registryDict objectForKey:@"Root"] objectForKey:@"children"] count];
            return childCount;
        } else {
            return [[self childArrayToIndex:column] count];
        }

    } else if (sender == planeBrowser) {
        return [[[[registryDict objectForKey:@"Root"] objectForKey:@"properties"] objectForKey:@"IORegistryPlanes"] count];
    }
    return 0;
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column
{
    if (sender == browser) {
        id object = nil;

        id name = nil;

        if (column == 0) {
            NSArray *objArray = [[registryDict objectForKey:@"Root"] objectForKey:@"children"];
            object = [objArray objectAtIndex:row];
        } else {
            object = [[self childArrayToIndex:column] objectAtIndex:row];
        }

        name = [object objectForKey:@"name"];

        if (!name) {
            name = [object objectForKey:@"IOClass Names"];
        }

        if (!name) {
            name = [[object allKeys] objectAtIndex:0];
        }

        if (!name) {
            name = object;
        }

        [cell setStringValue:name];
        [cell setLeaf:(![[object objectForKey:@"children"] count])];
    } else if (sender == planeBrowser) {
        id planesDict = [[[registryDict objectForKey:@"Root"] objectForKey:@"properties"] objectForKey:@"IORegistryPlanes"];

        [cell setStringValue:[[planesDict allKeys] objectAtIndex:row]];
        //[cell setStringValue:@"1"];
        [cell setLeaf:YES];
    }
    return;
}

- (BOOL)browser:(NSBrowser *)sender selectRow:(int)row inColumn:(int)column
{

    return YES;
}



@end

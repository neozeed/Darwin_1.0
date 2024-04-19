
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFSerialize.h>
#include <mach/mach_types.h>

#include <IOKit/kext/KEXTManager.h>

static Boolean sVerbose = false;
static Boolean sInteractive = false;
static Boolean sAuthenticateAll = false;

static KEXTManagerRef manager = NULL;
static const char * sArgv0;

#define kModuleKey         "Module"
#define kModulesKey        "Modules"
#define kModuleFileKey     "File"
#define kPersonalityKey	   "Personality"
#define kPersonalitiesKey  "Personalities"
#define kNameKey           "Name"
#define kVendorKey         "Vendor"
#define kVersionKey        "Version"
#define kRequiresKey       "Requires"
#define kModuleAliasesKey  "Aliases"

#define kDefaultSearchPath "/System/Library/Extensions"
#define kInfoMacOS         "Info-macos"
#define kInfoMacOSType     "xml"

#define defaultPath CFSTR(kDefaultSearchPath)


static void usage(Boolean help)
{
    printf("Usage: %s [-ivh] kextpath\n", sArgv0);
    if ( help ) {
        printf("\t-i               Interactive mode.\n");
        printf("\t-v               Verbose mode.\n");
        printf("\t-h               Help (this menu).\n");
    }
    exit(-1);
}

static Boolean Prompt(CFStringRef message, Boolean defaultValue)
{
    Boolean ret;
    CFIndex len;
    char * buf;
    char * dp;
    char c;
    char dv;

    ret = false;
    len = CFStringGetLength(message) + 1;
    buf = (char *)malloc(sizeof(char) * len);
    if ( !CFStringGetCString(message, buf, len, kCFStringEncodingASCII) ) {
        free(buf);
        return false;
    }
    
    dv = defaultValue?'y':'n';
    dp = defaultValue?" [Y/n]":" [y/N]";
    
    while ( 1 ) {
        printf(buf);
        printf(dp);
        printf("? ");
        fflush(stdout);
        fscanf(stdin, "%c", &c);
        if ( c != 10 ) while ( fgetc(stdin) != 10 );
        if ( (c == 10) || (tolower(c) == dv) ) {
            ret = defaultValue;
            break;
        }
        else if ( tolower(c) == 'y' ) {
            ret = true;
            break;
        }
        else if ( tolower(c) == 'n' ) {
            ret = false;
            break;
        }
    }
    free(buf);

    return ret;
}

// Override the default authentication scheme so we can load
// kext not owned by root into the kernel.
static KEXTReturn authenticate(CFURLRef url, void * context)
{
    if ( !sAuthenticateAll ) {
        KEXTReturn ret;
        
        ret = KEXTManagerAuthenticateURL(url);
        if ( ret != kKEXTReturnSuccess ) {
            if ( sVerbose ) {
                CFURLRef absUrl;
                CFStringRef message;
                CFStringRef path;

                absUrl = CFURLCopyAbsoluteURL(url);
                path = CFURLGetString(absUrl);

                message = CFStringCreateWithFormat(
                                            kCFAllocatorDefault,
                                            NULL,
                                            CFSTR("Error (%) Authentication failed: %@"),
                                            ret,
                                            path);

                CFShow(message);
                CFRelease(message);
                CFRelease(absUrl);
            }
            return ret;
        }
    }

    return kKEXTReturnSuccess;
}

// Print out a message when a module is about to load.
static Boolean mWillLoad(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    CFStringRef name;
    CFStringRef message;

    if ( !sVerbose ) {
        return true;
    }
    
    name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kNameKey));
    message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Loading module: %@."), name);
    CFShow(message);
    CFRelease(message);
    
    return true;
}

// Print out a message when the module was successfully loaded.
static void mWasLoaded(KEXTManagerRef manager, KEXTModuleRef module, void * context)
{
    CFStringRef name;
    CFStringRef message;

    if ( !sVerbose ) {
        return;
    }

    name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kNameKey));
    message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Loaded module: %@."), name);
    CFShow(message);
    CFRelease(message);
}

// Print an error message if there was an error loading the module.
static KEXTReturn mLoadError(KEXTManagerRef manager, KEXTModuleRef module, KEXTReturn error, void * context)
{
    if ( sVerbose ) {
        CFStringRef name;
        CFStringRef message;

        name = (CFStringRef)KEXTModuleGetProperty(module, CFSTR(kNameKey));
        switch ( error ) {
            // If the module has already been loaded, then just continue on.
            case kKEXTReturnModuleAlreadyLoaded:
                message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Module '%@' already loaded."), name);
                CFShow(message);
                CFRelease(message);
                error = kKEXTReturnSuccess;
                break;

            default:
                message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Error loading module '%@'"), name);
                KEXTError(error, message);
                CFRelease(message);
                break;
        }
    }

    return error;
}

// Initialize the KEXTManager.
static Boolean InitManager(void)
{
    CFURLRef url;
    KEXTReturn error;
    KEXTManagerBundleLoadingCallbacks bCallback = {
        0, authenticate, NULL, NULL, NULL, NULL, NULL,
    };
    KEXTManagerModuleLoadingCallbacks mCallbacks = {
        0, mWillLoad, mWasLoaded, mLoadError, NULL, NULL,
    };

    // Give the manager the default path to the Extensions folder.
    // This is needed for dependency matching.
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, defaultPath, kCFURLPOSIXPathStyle, true);

    // Create the manager database.
    manager = KEXTManagerCreate(&bCallback, &mCallbacks, NULL, NULL, NULL, &error);
    if ( !manager ) {
        printf("Manager not created.\n");
        return false;
    }

    // Now scan in all the bundles in the extensions directory.
    if ( (error = KEXTManagerScanPath(manager, url)) != kKEXTReturnSuccess ) {
        if ( sVerbose ) {
            KEXTError(error, CFSTR("Error scanning path"));
        }
        return false;
    };

    return true;
}

static void PromptForLoading(void * val, void * context)
{
    KEXTPersonalityRef person;
    CFMutableArrayRef toLoad;
    Boolean boolval;

    person = val;
    toLoad = context;

    boolval = true;
    if ( sInteractive ) {
        CFStringRef name;

        name = KEXTPersonalityGetProperty(person, CFSTR(kNameKey));
        if ( name ) {
            CFStringRef message;
            
            message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Load personality '%@'"), name);
            boolval = Prompt(message, true);
            CFRelease(message);
        }
    }

    if ( boolval ) {
        CFArrayAppendValue(toLoad, person);
    }
}

static void ArrayGetModuleList(void * val, void * context[])
{
    KEXTPersonalityRef person;
    KEXTModuleRef module;
    KEXTReturn error;
    CFMutableArrayRef modules;
    CFStringRef name;
    CFStringRef modName;
    CFStringRef message;

    person = val;
    modules = context[0];
    error = *(KEXTReturn *)context[1];

    if ( error != kKEXTReturnSuccess ) {
        return;
    }

    // Once we have the personality entity, we can
    // attempt to load it into the kernel...
    modName = KEXTPersonalityGetProperty(person, CFSTR(kModuleKey));
    if ( !modName ) {
        name = KEXTPersonalityGetProperty(person, CFSTR(kNameKey));
        message = CFStringCreateWithFormat(kCFAllocatorDefault,
                                            NULL,
                                            CFSTR("Error: '%@' has no Module key."),
                                            name);
        CFShow(message);
        CFRelease(message);

        if ( sInteractive ) {
            if ( !Prompt(CFSTR("Continue"), true) ) {
                *(KEXTReturn *)(context[1]) = error;
                return;
            }
        }
        return;
    }

    module = KEXTManagerGetModule(manager, modName);
    if ( module ) {
        CFRange range;

        range = CFRangeMake(0, CFArrayGetCount(modules));
        if ( !CFArrayContainsValue(modules, range, module) ) {
            CFArrayAppendValue(modules, module);
        }
    }
}

static void ArrayLoadMods(void * val, void * context)
{
    KEXTModuleRef mod;
    KEXTReturn error;
    Boolean boolval;

    error = *(KEXTReturn *)context;
    if ( error != kKEXTReturnSuccess ) {
        return;
    }

    mod = val;
    
    boolval = true;
    if ( sInteractive ) {
        CFStringRef name;

        name = KEXTModuleGetProperty(mod, CFSTR(kNameKey));
        if ( name ) {
            CFStringRef message;

            message = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("Load module '%@'"), name);
            boolval = Prompt(message, true);
            CFRelease(message);
        }
    }

    if ( boolval ) {
        error = KEXTManagerLoadModule(manager, mod);
        if ( error != kKEXTReturnSuccess ) {
            KEXTError(error, CFSTR("Error loading module"));
            *(KEXTReturn *)context = error;
        }
    }
}

static KEXTReturn LoadAllModules(KEXTBundleRef bundle)
{
    CFArrayRef modules;
    CFRange range;
    KEXTReturn error;

    if ( !bundle ) {
        return kKEXTReturnBadArgument;
    }
    
    modules = KEXTManagerCopyModulesForBundle(manager, bundle);
    if ( !modules ) {
        return kKEXTReturnModuleNotFound;
    }

    error = kKEXTReturnSuccess;
    range = CFRangeMake(0, CFArrayGetCount(modules));
    CFArrayApplyFunction(modules, range, (CFArrayApplierFunction)ArrayLoadMods, &error);

    CFRelease(modules);

    return error;
}

static KEXTReturn LoadAllPersonalities(KEXTBundleRef bundle)
{
    CFArrayRef array;
    CFArrayRef configs;
    CFMutableArrayRef toLoad;
    CFMutableArrayRef modules;
    CFRange range;
    KEXTReturn error;
    void * context[2];

    error = kKEXTReturnSuccess;

    // Get the configurations associated with this bundle.
    configs = KEXTManagerCopyConfigsForBundle(manager, bundle);

    // Get the personality entities associated with
    // this particular bundle.  We use these keys to aquire\
    // personality entities from the database.
    array = KEXTManagerCopyPersonalitiesForBundle(manager, bundle);

    if ( !array && !configs ) {
        return kKEXTReturnPersonalityNotFound;
    }

    // This is the list of personalities and configurations to load.
    toLoad = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    modules = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    if ( configs ) {
        range = CFRangeMake(0, CFArrayGetCount(configs));
        CFArrayApplyFunction(configs, range, (CFArrayApplierFunction)PromptForLoading, toLoad);
        CFRelease(configs);
    }

    if ( array ) {
        range = CFRangeMake(0, CFArrayGetCount(array));
        CFArrayApplyFunction(array, range, (CFArrayApplierFunction)PromptForLoading, toLoad);
        CFRelease(array);
    }

    context[0] = modules;
    context[1] = &error;

    range = CFRangeMake(0, CFArrayGetCount(toLoad));
    CFArrayApplyFunction(toLoad, range, (CFArrayApplierFunction)ArrayGetModuleList, context);

    if ( error != kKEXTReturnSuccess ) {
        CFRelease(toLoad);
        CFRelease(modules);
        return error;
    }

    // Load all the modules.
    sInteractive = false;
    range = CFRangeMake(0, CFArrayGetCount(modules));
    CFArrayApplyFunction(modules, range, (CFArrayApplierFunction)ArrayLoadMods, &error);
    sInteractive = true;

    if ( error != kKEXTReturnSuccess ) {
        CFRelease(toLoad);
        CFRelease(modules);
        return error;
    }
    
    // We need to send all personalities together.
    error = KEXTManagerLoadPersonalities(manager, toLoad);
    CFRelease(toLoad);
    CFRelease(modules);

    return error;
}

int main (int argc, const char *argv[])
{
    int c;

    CFURLRef url;
    CFURLRef abs;
    CFStringRef path;
    CFStringRef name;
    KEXTBundleRef bundle;
    KEXTReturn error;

    name  = NULL;
    sArgv0 = argv[0];

    while ( (c = getopt(argc, (char **)argv, "vihp:")) != -1 ) {
        switch ( c ) {

            case 'v':
                sVerbose = true;
                break;

            case 'i':
                sInteractive = true;
                break;

            case 'h':
                usage(true);

            case 'p':
                if ( !optarg )
                    usage(false);
                else
                    name = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingASCII);
                break;

            default:
                usage(false);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1)
        usage(false);

    path = CFStringCreateWithCString(kCFAllocatorDefault, argv[argc - 1], kCFStringEncodingASCII);
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, true);
    CFRelease(path);
    
    abs = CFURLCopyAbsoluteURL(url);
    CFRelease(url);

    if ( !abs ) {
        printf("Invalid path: %s.\n", argv[argc - 1]);
        CFRelease(abs);
        exit(-1);
    }

    if ( sVerbose ) {
        printf("Examining: %s\n", argv[argc - 1]);
    }

    sAuthenticateAll = false;
    if ( !InitManager() ) {
        printf("Error initializing KEXT Manager.\n");
        exit(-1);
    }

    // Don't authenticate the target bundle, this
    // is just a convenience for developers.
    sAuthenticateAll = true;
    
    // Add the bundle to the database.
    error = KEXTManagerAddBundle(manager, abs);
    if ( error != kKEXTReturnSuccess ) {
        printf("Error (%d) adding bundle to database.\n", error);
        exit(1);
    }
    // Re-enable the authentication.
    sAuthenticateAll = false;
    // Now, get the bundle entity from the database,
    // this is the handle we use for accessing bundle resources.
    bundle = KEXTManagerGetBundleWithURL(manager, abs);
    if ( !bundle ) {
        printf("Bundle not found in database.\n");
        exit(1);
    }

    // If not name was given, then assume all personalities should be loaded.
    error = LoadAllPersonalities(bundle);
    if ( error != kKEXTReturnSuccess ) {
        // No personalities are found, then this is probably
        // a kmod bundle.  Try loading just the modules.
        if ( error == kKEXTReturnPersonalityNotFound ) {
            // XXX -- Attempt to load modules.
            LoadAllModules(bundle);
        }
    }

    if ( manager ) {
        KEXTManagerRelease(manager);
    }

    printf("done.\n");

    return 0;      // ...and make main fit the ANSI spec.
}

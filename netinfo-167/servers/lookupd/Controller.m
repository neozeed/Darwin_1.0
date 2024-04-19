/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Controller.m
 *
 * Controller for lookupd
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <config.h>
#import <project_version.h>
#import "Controller.h"
#import "Thread.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import "Syslog.h"
#import "LUGlobal.h"
#import "MachRPC.h"
#import "LUServer.h"
#import "CacheAgent.h"
#import "NIAgent.h"
#import "LUNIDomain.h"
#import "DNSAgent.h"
#import "FFAgent.h"
#import "YPAgent.h"
#import "LDAPAgent.h"
#import "NILAgent.h"
#import "stringops.h"
#import "sys.h"
#import <sys/types.h>
#import <sys/param.h>
#import <unistd.h>
#import <string.h>
#import <libc.h>

#define forever for(;;)
#define LookupConfigDict "/locations"
#define LookupConfigDirPath "/etc"

extern BOOL restartLookupd;
extern int gethostname(char *, int);
extern sys_port_type server_port;
extern sys_port_type _lookupd_port(sys_port_type);
#ifdef _SHADOW_
extern sys_port_type _lookupd_port1(sys_port_type);
extern sys_port_type server_port_privileged;
extern sys_port_type server_port_unprivileged;
#endif

static int _cache_initialized = 0;

@implementation Controller

- (void)serviceRequest:(lookup_request_msg *)request
{
	BOOL status;

	/*
 	 * Update idle thread counter
	 */
	[serverLock lock];
	idleThreadCount--;

	/* make sure there's at lease one thread in msg_receive */
	while ((idleThreadCount < 1) && (threadCount < maxThreads))
	{
		[self startServerThread];
	}
	[serverLock unlock];

	/*
	 * Deal with the client's request
	 */
	status = [machRPC process:request];

	/*
	 * This thread should exit if there are too many idle threads
	 */
	[serverLock lock];
	if (idleThreadCount > maxIdleThreads)
	{
		threadCount--;
		[[Thread currentThread] shouldTerminate:YES];
	}
	else
	{
		idleThreadCount++;
	}
	[serverLock unlock];

}

- (sys_port_type)server_port
{
	return server_port;
}

/*
 * Server threads run in this loop to answer requests
 */
- (void)serverLoop
{
	kern_return_t status;
	lookup_request_msg request;
	Thread *t;
	char str[256];

	t = [Thread currentThread];

	forever
	{
		/* Receive and service a request */
		[t setState:ThreadStateIdle];
		status = sys_receive_message(&request.head, sizeof(lookup_request_msg), server_port, 0);
		[t setState:ThreadStateActive];

		if (status != KERN_SUCCESS)
		{
			if (restartLookupd) [t terminateSelf];
			sprintf(str, "Server status = %s (%d)", sys_strerror(status), status);
			[lookupLog syslogError:str];
			continue;
		}
		
		[self serviceRequest:&request];
		if (restartLookupd || [t shouldTerminate]) [t terminateSelf];
	}
}

/*
 * Cache initialization thread
 */
- (void)initCache
{
	char hname[1024];
	LUServer *s;
	LUDictionary *item;
	Thread *t;

	t = [Thread currentThread];
	[t setState:ThreadStateActive];

	/*
	 * get the shared cache agent
	 */
	cacheAgent = [[CacheAgent alloc] init];

	gethostname(hname, 1024);

	s = [self checkOutServer];
	item = [s userWithName:"root"];
	if (item != nil)
	{
		[cacheAgent addObject:item];
		[item setTimeToLive:(time_t)-1];
		[item release];
	}

	item = [s hostWithName:"localhost"];
	if (item != nil)
	{
		[cacheAgent addObject:item];
		[item setTimeToLive:(time_t)-1];
		[item release];
	}

	item = [s hostWithName:hname];
	if (item != nil)
	{
		[cacheAgent addObject:item];
		[item setTimeToLive:(time_t)-1];
		[item release];
	}

	[self checkInServer:s];

	_cache_initialized = 1;

	[t terminateSelf];
}

- (int)config:(LUDictionary *)dict int:(char *)key default:(int)def
{
	char *s;
	int n, i;

	if (dict == nil) return def;
	s = [dict valueForKey:key];
	if (s == NULL) return def;
	n = sscanf(s, "%d", &i);
	if (n <= 0) return def;
	return i;
}

- (char *)config:(LUDictionary *)dict string:(char *)key default:(char *)def
{
	char *s, t[256];
	int n;

	if (dict == nil) return def;
	s = [dict valueForKey:key];
	if (s == NULL) return def;
	n = sscanf(s, "%s", t);
	if (n <= 0) return def;
	return s;
}

- (BOOL)config:(LUDictionary *)dict bool:(char *)key default:(BOOL)def
{
	char *s;

	if (dict == nil) return def;
	s = [dict valueForKey:key];
	if (s == NULL) return def;
	if (s[0] == 'Y') return YES;
	if (s[0] == 'y') return YES;
	if (s[0] == 'T') return YES;
	if (s[0] == 't') return YES;
	if (!strcmp(s, "1")) return YES;
	if (s[0] == 'N') return NO;
	if (s[0] == 'n') return NO;
	if (s[0] == 'F') return NO;
	if (s[0] == 'f') return NO;
	if (!strcmp(s, "0")) return NO;

	return def;
}

- (void)newAgent:(id)agent name:(char *)name
{
	if (agentCount == 0) agents = (id *)malloc(sizeof(id));
	else agents = (id *)realloc(agents, (agentCount + 1) * sizeof(id));
	agents[agentCount] = agent;
	agentNames = appendString(name, agentNames);
	agentCount++;
}

- (id)agentNamed:(char *)name
{
	int i;
	char cname[256];

	i = listIndex(name, agentNames);
	if (i != IndexNull) return agents[i];

	sprintf(cname, "%sAgent", name);
	i = listIndex(cname, agentNames);
	if (i != IndexNull) return agents[i];

	return nil;
}

- (void)initLookup:(LUCategory)cat
{
	char **order;
	int i;
	id agent;
	BOOL validation;
	unsigned int max, freq;
	time_t ttl, delta;

	if (configDict[(int)cat] == nil) return;

	order = [configDict[(int)cat] valuesForKey:"LookupOrder"];
	if (order != NULL)
	{
		[lookupOrder[(int)cat] releaseObjects];
	
		for (i = 0; order[i] != NULL; i++)
		{
			agent = [self agentNamed:order[i]];
			if (agent == nil) continue;

			[lookupOrder[(int)cat] addObject:agent];		
		}
	}

	validation = [self config:configDict[(int)cat] bool:"ValidateCache" default:YES];
	[cacheAgent setCacheIsValidated:validation forCategory:cat];

	max = [self config:globalDict int:"CacheCapacity" default:0];
	if (max == 0) max = (unsigned int)-1;
	ttl = (time_t)[self config:globalDict int:"TimeToLive" default:43200];
	delta = (time_t)[self config:globalDict int:"TimeToLiveDelta" default:0];
	freq = [self config:globalDict int:"TimeToLiveFreq" default:0];

	[cacheAgent setCapacity:max forCategory:cat];
	[cacheAgent setTimeToLive:ttl forCategory:cat];
	[cacheAgent addTimeToLive:delta afterCacheHits:freq forCategory:cat];
}

- (void)initGlobalLookup
{
	char **order;
	int i, n, len;
	id agent;
	BOOL validation;
	char *logFileName;
	char *logFacilityName;
	unsigned int max, freq;
	time_t now, ttl, delta;
	char str[64];

	logFileName = [self config:globalDict string:"LogFile" default:NULL];
 	[lookupLog setLogFile:logFileName];

	logFacilityName = [self config:globalDict
		string:"LogFacility" default:"LOG_NETINFO"];
 	[lookupLog setLogFacility:logFacilityName];

	now = time(0);
	sprintf(str, "lookupd (version %s) starting - %s",
		_PROJECT_VERSION_, ctime(&now));

	/* remove ctime trailing newline */
	str[strlen(str) - 1] = '\0';
	[lookupLog syslogDebug:str];

	maxThreads = [self config:globalDict int:"MaxThreads" default:16];
	maxIdleThreads = [self config:globalDict int:"MaxIdleThreads" default:16];
	maxIdleServers = [self config:globalDict int:"MaxIdleServers" default:16];

	validation = [self config:globalDict bool:"ValidateCache" default:YES];
	max = [self config:globalDict int:"CacheCapacity" default:-1];
	if (max == 0) max = (unsigned int)-1;
	ttl = (time_t)[self config:globalDict int:"TimeToLive" default:43200];
	delta = (time_t)[self config:globalDict int:"TimeToLiveDelta" default:0];
	freq = [self config:globalDict int:"TimeToLiveFreq" default:0];

	for (i = 0; i < NCATEGORIES; i++)
	{
		[cacheAgent setCacheIsValidated:validation forCategory:(LUCategory)i];
		[cacheAgent setCapacity:max forCategory:(LUCategory)i];
		[cacheAgent setTimeToLive:ttl forCategory:(LUCategory)i];
		[cacheAgent addTimeToLive:delta afterCacheHits:freq forCategory:(LUCategory)i];
	}

	[self newAgent:[CacheAgent class] name:"CacheAgent"];
	[self newAgent:[DNSAgent class] name:"DNSAgent"];
	[self newAgent:[NIAgent class] name:"NIAgent"];
	[self newAgent:[FFAgent class] name:"FFAgent"];
	[self newAgent:[YPAgent class] name:"YPAgent"];
	[self newAgent:[LDAPAgent class] name:"LDAPAgent"];
	[self newAgent:[NILAgent class] name:"NILAgent"];

	order = [globalDict valuesForKey:"LookupOrder"];
	if (order == NULL) len = 0;
	else len = listLength(order);

	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:order[i]];
		if (agent == nil) continue;

		for (n = 0; n < NCATEGORIES; n++) [lookupOrder[n] addObject:agent];		
	}

	if (len == 0)
	{
		for (n = 0; n < NCATEGORIES; n++)
		{
			[lookupOrder[n] addObject:[CacheAgent class]];
			if ((n == (int)LUCategoryHost) || (n == (int)LUCategoryNetwork))
				[lookupOrder[n] addObject:[DNSAgent class]];
			[lookupOrder[n] addObject:[NIAgent class]];
		}
	}
}

- (char *)getLineFromFile:(FILE *)fp
{
	char s[1024];
	char *out;
	int len;

    s[0] = '\0';

    fgets(s, 1024, fp);
    if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] == '#')
	{
		out = copyString("#");
		return out;
	}

	len = strlen(s) - 1;
	s[len] = '\0';

	out = copyString(s);
	return out;
}

- (LUDictionary *)configurationForFilePath:(char *)p
{
	LUDictionary *dict;
	char path[256];
	FILE *fp;
	char *line;
	char **tokens;
	FFParser *parser;

	sprintf(path, "%s/%s/%s", LookupConfigDirPath, portName, p);

	fp = fopen(path, "r");
	if ((fp == NULL) && (strcmp(portName, DefaultName)))
	{
		sprintf(path, "%s/%s/%s", LookupConfigDirPath, DefaultName, p);
		fp = fopen(path, "r");
	}
	if (fp == NULL) return nil;

	dict = [[LUDictionary alloc] init];
	parser = [[FFParser alloc] init];

	while (NULL != (line = [self getLineFromFile:fp]))
	{
		if (line[0] == '#') 
		{
			freeString(line);
			line = NULL;
			continue;
		}

		tokens = [parser tokensFromLine:line separator:" \t"];
		if (tokens == NULL) continue;

		[dict setValues:(tokens+1) forKey:tokens[0]];
		freeList(tokens);
		tokens = NULL;
	}

	[parser release];
	return dict;
}

- (LUDictionary *)configurationForNIPath:(char *)p
{
	LUDictionary *dict = nil;
	LUNIDomain *d;
	char path[256];

	sprintf(path, "%s/%s", LookupConfigDict, portName);
	if (p != NULL)
	{
		strcat(path, "/");
		strcat(path, p);
	}

	for (d = [controlNIAgent domainAtIndex:0]; d != nil; d = [d parent])
	{
		dict = [d readDirectoryName:path selectedKeys:NULL];
		if (dict != nil) break;
	}

	if ((dict == nil) && (strcmp(portName, DefaultName)))
	{
		sprintf(path, "%s/%s", LookupConfigDict, DefaultName);
		if (p != NULL)
		{
			strcat(path, "/");
			strcat(path, p);
		}

		for (d = [controlNIAgent domainAtIndex:0]; d != nil; d = [d parent])
		{
			dict = [d readDirectoryName:path selectedKeys:NULL];
			if (dict != nil) break;
		}
	}

	if (dict == nil) return nil;

	return dict;
}

- (LUDictionary *)configurationForCategory:(LUCategory)cat
{
	LUDictionary *dict;
	char *catPath;
	char path[256];

	catPath = (char *)[controlNIAgent categoryPathname:cat];
	dict = [self configurationForNIPath:catPath];

	if (catPath == NULL) catPath = "global";
	if (dict == nil) dict = [self configurationForFilePath:catPath];

	if (dict == nil) return nil;

	if (cat == LUCategoryNull)
		sprintf(path, "Controller global configuration");
	else
		sprintf(path, "Controller configuration for category %s",
			[controlNIAgent categoryName:cat]);
	[dict setBanner:path];

	return dict;
}

- (LUDictionary *)configurationForAgent:(char *)agent category:(LUCategory)cat
{
	LUDictionary *dict;
	char *catPath;
	char path[256];

	if (agent == NULL) return [globalDict retain];

	catPath = (char *)[controlNIAgent categoryPathname:cat];

	if (catPath == NULL)
	{
		sprintf(path, "agents/%s", agent);
		dict = [self configurationForNIPath:path];
	}
	else
	{
		sprintf(path, "agents/%s/%s", agent, catPath);
		dict = [self configurationForNIPath:path];
	}

	if (dict == nil)
	{
		if (catPath == NULL) strcat(path, "/global");
		dict = [self configurationForFilePath:path];
	}

	if (dict == nil) return nil;

	if (cat == LUCategoryNull)
		sprintf(path, "%s configuration", agent);
	else
		sprintf(path, "%s configuration for category %s", agent, catPath);
	[dict setBanner:path];

	return dict;
}

- (LUDictionary *)configurationForAgent:(char *)agent
{
	return [self configurationForAgent:agent category:LUCategoryNull];
}

- (id)init
{
	return [self initWithName:NULL];
}

- (id)initWithName:(char *)name
{
	int i;
	char str[128];
	Thread *t;
	DNSAgent *dns;

	[super init];

	dnsSearchList = NULL;

	controller = self;

 	if (name == NULL) portName = copyString((char *)DefaultName);
	else portName = copyString((char *)name);

	lookupLog = nil;

	[[Syslog alloc] initWithIdent:portName
		facility:LOG_NETINFO
		options:(LOG_NOWAIT | LOG_PID)
		logFile:NULL];

	/* XXX sleeps to allow log thread to start up */
	while (lookupLog == nil) sleep(1);

	if (![self registerPort:portName]) return nil;

	serverLock = [[Lock alloc] initThreadLock];
	serverList = [[LUArray alloc] init];
	[serverList setBanner:"Controller server list"];

	rpcLock = [[Lock alloc] init];
	controlStats = [[LUDictionary alloc] init];
	[controlStats setBanner:"Controller statistics"];

	[controlStats setValue:_PROJECT_VERSION_ forKey:"version"];

	threadCount = 0;
	idleThreadCount = 0;

	cacheAgent = [[CacheAgent alloc] init];
	for (i = 0; i < NCATEGORIES; i++)
	{
		lookupOrder[i] = [[LUArray alloc] init];
		sprintf(str, "Controller lookup order for category %s",
			[cacheAgent categoryName:i]);
		[lookupOrder[i] setBanner:str];
	}

	controlNIAgent = [[NIAgent reallyAlloc] initWithLocalHierarchy];
	globalDict = [self configurationForCategory:LUCategoryNull];

	for (i = 0; i < NCATEGORIES; i++)
		configDict[i] = [self configurationForCategory:(LUCategory)i];

	[self initGlobalLookup];
	[self initLookup:LUCategoryUser];
	[self initLookup:LUCategoryHost];
	[self initLookup:LUCategoryNetwork];
	[self initLookup:LUCategoryService];
	[self initLookup:LUCategoryProtocol];
	[self initLookup:LUCategoryRpc];
	[self initLookup:LUCategoryMount];
	[self initLookup:LUCategoryPrinter];
	[self initLookup:LUCategoryBootparam];
	[self initLookup:LUCategoryBootp];
	[self initLookup:LUCategoryAlias];
	[self initLookup:LUCategoryNetgroup];

	[self startServerThread];

	_cache_initialized = 0;
	t = [[Thread alloc] init];
	[t setName:"Cache Init"];
	[t run:@selector(initCache) context:self];

	loginUser = nil;

	machRPC = [[MachRPC alloc] init:self];

	dns = [[DNSAgent alloc] init];
	if (dns != nil)
	{
		dnsSearchList = [dns searchList];
		[dns release];
	}

	t = [Thread currentThread];
	[t yield];

	while (_cache_initialized == 0)
	{
		[t usleep:100];
		[t yield];
	}

	return self;
}

- (char *)portName
{
	return portName;
}

- (char **)dnsSearchList
{
	return dnsSearchList;
}

- (void)dealloc
{
	int i;

	if (dnsSearchList != NULL) freeList(dnsSearchList);
	dnsSearchList = NULL;

	sys_destroy_service(portName);
	freeString(portName);
	portName = NULL;
	sys_port_free(server_port);

	if (loginUser != nil) [loginUser release];
	if (cacheAgent != nil) [cacheAgent release];
	freeString(configDir);
	configDir = NULL;
	if (serverLock != nil) [serverLock free];
	if (rpcLock != nil) [rpcLock free];
	if (globalDict != nil) [globalDict release];

	for (i = 0; i < NCATEGORIES; i++)
	{
		if (lookupOrder[i] != nil) [lookupOrder[i] release];
		if (configDict[i] != nil) [configDict[i] release];
	}

	if (serverList != nil)
	{
		[serverList releaseObjects];
		[serverList release];
	}

	if (controlNIAgent != nil) [controlNIAgent release];
	if (controlStats != nil) [controlStats release];
	freeList(agentNames);
	agentNames = NULL;
	free(agents);

	[lookupLog syslogDebug:"lookupd exiting"];
	[[Thread currentThread] yield];

	[lookupLog release];
	[super dealloc];
}

- (BOOL)registerPort:(char *)name
{
	kern_return_t status;
	char str[256];

	sprintf(str, "Registering service \"%s\"", portName);
	[lookupLog syslogDebug:str];

	if (!strcmp(name, DefaultName))
	{
		/*
		 * If server_port is already set, this is a restart.
		 */
		if (server_port != SYS_PORT_NULL) return YES;

#ifdef _SHADOW_
		status = sys_port_alloc(&server_port_unprivileged);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't allocate unprivileged server port!"];
			return NO;
		}

		status = sys_port_alloc(&server_port_privileged);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't allocate privileged server port!"];
			return NO;
		}

		status = port_set_allocate(task_self(), &server_port);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't allocate server port set!"];
			return NO;
		}

		status = port_set_add(task_self(), server_port, server_port_unprivileged);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't add unprivileged port to port set!"];
			return NO;
		}

		status = port_set_add(task_self(), server_port, server_port_privileged);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't add privileged port to port set!"];
			return NO;
		}
#else
		if (sys_port_alloc(&server_port) != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't allocate server port!"];
			return NO;
		}
#endif

#ifdef _OS_VERSION_MACOS_X_
		status = mach_port_insert_right(mach_task_self(), server_port, server_port, MACH_MSG_TYPE_MAKE_SEND);
		if (status != KERN_SUCCESS)
		{
			[lookupLog syslogError:"Can't insert send right for server port!"];
			return NO;
		}
#endif

#ifdef _SHADOW_
		/*
		 * _lookupd_port(p) registers the unprivileged lookupd port p.
		 * _lookupd_port1(q) registers the privileged lookupd port q.
		 * Clients get ports with _lookup_port(0) and _lookup_port1(0).
		 */

		if (_lookupd_port(server_port_unprivileged) != server_port_unprivileged)
		{
			[lookupLog syslogError:"Can't check in unprivileged server port!"];
			return NO;
		}

		if (_lookupd_port1(server_port_privileged) != server_port_privileged)
		{
			[lookupLog syslogError:"Can't check in privileged server port!"];
		}
#else
		/* _lookupd_port(p) registers the lookupd port p. */
		if (_lookupd_port(server_port) != server_port)
		{
			[lookupLog syslogError:"Can't check in server port!"];
			return NO;
		}
#endif
		return YES;
	}

	status = sys_create_service(name, &server_port);
	if (status == KERN_SUCCESS) return YES;

	sprintf(str, "Can't create service! (error %d)", status);
	[lookupLog syslogError:str];
	return NO;
}

- (void)startServerThread
{
	Thread *t;
	char tname[128];

	[serverLock lock];

	/*
	 * Create the thread
	 */
	t = [[Thread alloc] init];
	sprintf(tname, "IPC Server %d", threadCount);
	[t setName:tname];
	
	sprintf(tname, "Started IPC Server %d", threadCount);
	[lookupLog syslogDebug:tname];

	[t run:@selector(serverLoop) context:self];

	/*
	 * Update counters
	 */
	threadCount++;
	idleThreadCount++;

	[serverLock unlock];
}

/*
 * Get an idle server from the server list
 */
- (LUServer *)checkOutServer
{
	LUServer *server;
	int i, len;

	[serverLock lock];
	server = nil;

	len = [serverList count];

	for (i = 0; i < len; i++)
	{
		if ([[serverList objectAtIndex:i] isIdle])
		{
			server = [serverList objectAtIndex:i];
			[server setIsIdle:NO];
			break;
		}
	}

	if (server == nil)
	{
		/*
		 * No servers available - create a new server 
		 */
		server = [[LUServer alloc] init];
		[server setIsIdle:NO];

		for (i = 0; i < NCATEGORIES; i++)
			[server setLookupOrder:lookupOrder[i] forCategory:(LUCategory)i];

		[serverList addObject:server];
	}

	[serverLock unlock];
	return server;
}

- (void)checkInServer:(LUServer *)server
{
	int i, len, idleServerCount;

	[serverLock lock];

	[server setIsIdle:YES];

	idleServerCount = 0;
	len = [serverList count];
	for (i = 0; i < len; i++)
	{
		if ([[serverList objectAtIndex:i] isIdle]) idleServerCount ++;
	}

	if (idleServerCount > maxIdleServers)
	{
		[serverList removeObject:server];
		[server release];
	}

	[serverLock unlock];
}

- (void)setLoginUser:(int)uid
{
	LUServer *s;
	char scratch[256];

	if (loginUser != nil)
	{
		[cacheAgent removeObject:loginUser];
		[loginUser release];
		loginUser = nil;
	}

	s = [self checkOutServer];
	loginUser = [s userWithNumber:&uid];
	[self checkInServer:s];

	if (loginUser != nil)
	{
		[cacheAgent addObject:loginUser];
		[loginUser setTimeToLive:(time_t)-1];
		sprintf(scratch, "%s (console user)", [loginUser banner]);
		[loginUser setBanner:scratch];
	}
}

- (void)flushCache
{
	[cacheAgent flushCache];
}

- (void)suspend
{
	/* XXX suspend */
}

/*
 * Custom lookups 
 *
 * Data lookup done here!
 */
- (BOOL)isSecurityEnabledForOption:(char *)option
{
	if ([controlNIAgent isSecurityEnabledForOption:option]) return YES;
	return NO;
}

/*
 * Data lookup done here!
 */
- (BOOL)isNetwareEnabled
{
	if ([controlNIAgent isNetwareEnabled]) return YES;
	return NO;
}

- (LUArray *)allStatistics
{
	LUArray *allStats;
	LUArray *agentStats;
	LUDictionary *cacheStats;
	LUDictionary *memoryStats;
	LUDictionary *dict;
	LUServer *server;
	char *name;
	int i, nservers, j, nstats;

	[serverLock lock];

	cacheStats = nil;
	allStats = [[LUArray alloc] init];
	[allStats setBanner:"Controller allStatistics"];

	[allStats addObject:controlStats];
	[allStats addObject:globalDict];
	for (i = 0; i < NCATEGORIES; i++)
	{
		if (configDict[i] != nil)
			[allStats addObject:configDict[i]];
	}

	nservers = [serverList count];
	for (i = 0; i < nservers; i++)
	{
		server = [serverList objectAtIndex:i];
		[allStats addObject:[server statistics]];

		agentStats = [server agentStatistics];
		nstats = [agentStats count];
		for (j = 0; j < nstats; j++)
		{
			dict = [agentStats objectAtIndex:j];
			name = [dict valueForKey:"information_system"];
			if (name == NULL)
				[allStats addObject:dict];
			else if (strcmp(name, "Cache"))
				[allStats addObject:dict];
			else
				cacheStats = dict;
		}
		[agentStats release];
	}

	if (cacheStats != nil) [allStats addObject:cacheStats];
	memoryStats = [rover statistics];
	if (memoryStats != nil) [allStats addObject:memoryStats];

	[serverLock unlock];
	return allStats;
}

- (void)rpcLock
{
	[rpcLock lock];
}

- (void)rpcUnlock
{
	[rpcLock unlock];
}

@end

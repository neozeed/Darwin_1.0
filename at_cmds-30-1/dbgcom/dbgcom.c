/* dbgcom.c

   This file/library will no longer be needed when sysctlbyname() 
   is moved to libc.

*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>

/* *** copied from network_cmds/netstat.tproj/main.c 
	(originally from FreeBSD)
   *** */
int
sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
	     size_t newlen)
{
	int name2oid_oid[2];
	int real_oid[CTL_MAXNAME+2];
	int error;
	size_t oidlen;

	name2oid_oid[0] = 0;	/* This is magic & undocumented! */
	name2oid_oid[1] = 3;

	oidlen = sizeof(real_oid);
	error = sysctl(name2oid_oid, 2, real_oid, &oidlen, (void *)name,
		       strlen(name));
	if (error < 0) 
		return error;
	oidlen /= sizeof (int);

	error = sysctl(real_oid, oidlen, oldp, oldlenp, newp, newlen);

#ifdef APPLETALK_DEBUG
	printf("oid = %d.%d.%d, oldlen = %d, oldbits = 0x%x.%d, newbits = 0x%x.%d\n",
	       real_oid[0], real_oid[1], real_oid[2], (int)oidlen, 
	       (oldp)?*(unsigned *)oldp:0, (oldlenp)?(int)*oldlenp:0, 
	       (newp)?*(unsigned *)newp:0, newlen);
#endif
	return(error);
}
/* *** end copied from network_cmds/netstat.tproj/main.c *** */

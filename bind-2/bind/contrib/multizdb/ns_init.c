#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_init.c	4.38 (Berkeley) 3/21/91";
static char rcsid[] = "$Id:";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1990
 * -
 * Copyright (c) 1986, 1990
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <resolv.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "named.h"

#undef nsaddr

static void		zoneinit __P((struct zoneinfo *)),
			get_forwarders __P((FILE *)),
			boot_read __P((char *)),
#ifdef DEBUG
			content_zone __P((int)),
#endif
#ifdef MULTIZDB
			process_mzone_line __P((FILE *)),
#endif
			free_forwarders __P((void));

static struct zoneinfo	*find_zone __P((char *, int, int));

static char *multizone_file;  /* these don't nest */

/*
 * Read boot file for configuration info.
 */
void
ns_init(bootfile)
	char *bootfile;
{
	register struct zoneinfo *zp;
	static int loads = 0;			/* number of times loaded */

	dprintf(1, (ddt, "\nns_init(%s)\n", bootfile));
	gettime(&tt);

        if (loads == 0) {
		if ((zones =
			(struct zoneinfo *)calloc(64, sizeof(struct zoneinfo)))
					    == NULL) {
		    syslog(LOG_ERR,
			"Not enough memory to allocate initial zones array");
		    exit(1);
		}
		nzones = 1;		/* zone zero is cache data */
		/* allocate cache hash table, formerly the root hash table. */
		hashtab = savehash((struct hashbuf *)NULL);

		/* allocate root-hints/file-cache hash table */
		fcachetab = savehash((struct hashbuf *)NULL);
		/* init zone data */
		zones[0].z_type = Z_CACHE;
        } else {
		/* Mark previous zones as not yet found in boot file. */
		for (zp = &zones[1]; zp < &zones[nzones]; zp++)
			zp->z_flags &= ~Z_FOUND;
#ifdef LOCALDOM
		if (localdomain) {
			free(localdomain);
			localdomain = NULL;
		}
#endif
		free_forwarders();
		free_netlist(enettab);
#ifdef XFRNETS
		free_netlist(&xfrnets);
#endif
#ifdef BOGUSNS
		free_netlist(&boglist);
#endif
		forward_only = 0;
	}

	dprintf(3, (ddt, "\n content of zones before loading \n"));
#ifdef DEBUG
        if (debug >= 3) {
        	content_zone(nzones - 1);
        }
#endif
	boot_read(bootfile);

        /* erase all old zones that were not found */
        for (zp = &zones[1]; zp < &zones[nzones]; zp++) {
            if (zp->z_type && (zp->z_flags & Z_FOUND) == 0) {
#ifdef CLEANCACHE
		remove_zone(hashtab, zp - zones, 1);
#else
		remove_zone(hashtab, zp - zones);
#endif
#ifdef SECURE_ZONES
		free_netlist(&zp->secure_nets);
#endif
		syslog(LOG_NOTICE, "Zone \"%s\" was removed", zp->z_origin);
		free(zp->z_origin);
		free(zp->z_source);
		bzero((char *) zp, sizeof(*zp));
	    }
        }
	dprintf(2, (ddt,"\n content of zones after loading\n"));

#ifdef DEBUG
	if (debug >= 2) {
        	content_zone(nzones-1);
        }
#endif

	/*
	 * Schedule calls to ns_maint().
	 */
	if (!needmaint)
		sched_maint();
	dprintf(1, (ddt, "exit ns_init()%s\n",
		    needmaint ? ", need maintenance immediately" : ""));
	loads++;
}

/*
 * Read the actual boot file.
 * Set up to recurse.
 */
static void
boot_read(bootfile)
	char *bootfile;
{
	char buf[BUFSIZ];
	FILE *fp, *mzfp;
	int slineno;			/* Saved global line number. */

	if ((fp = fopen(bootfile, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", bootfile);
		exit(1);
	}

	slineno = lineno;
	lineno = 0;

	while (!feof(fp) && !ferror(fp)) {
		if (!getword(buf, sizeof(buf), fp))
			continue;
		/* read named.boot keyword and process args */
		if (strcasecmp(buf, "directory") == 0) {
			(void) getword(buf, sizeof(buf), fp);
			if (chdir(buf) < 0) {
				syslog(LOG_CRIT, "directory %s: %m\n",
					buf);
				exit(1);
			}
			continue;
		} else if (strcasecmp(buf, "sortlist") == 0) {
			get_netlist(fp, enettab, ALLOW_NETS, buf);
			continue;
		} else if (strcasecmp(buf, "max-fetch") == 0) {
			max_xfers_running = getnum(fp, bootfile, 0);
			continue;
		} else if (strcasecmp(buf, "forwarders") == 0) {
			get_forwarders(fp);
			continue;
		} else if (strcasecmp(buf, "slave") == 0) {
			forward_only++;
			endline(fp);
			continue;
#ifdef BOGUSNS
		} else if (strcasecmp(buf, "bogusns") == 0) {
			get_netlist(fp, &boglist, ALLOW_HOSTS, buf);
			continue;
#endif
#ifdef XFRNETS
		} else if ((strcasecmp(buf, "tcplist") == 0) ||
			   (strcasecmp(buf, "xfrnets") == 0)) {
			get_netlist(fp, &xfrnets, ALLOW_NETS, buf);
			continue;
#endif
#ifdef LOCALDOM
		} else if (strcasecmp(buf, "domain") == 0) {
			if (getword(buf, sizeof(buf), fp))
				localdomain = savestr(buf);
			endline(fp);
			continue;
#endif
		} else if (strcasecmp(buf, "include") == 0) {
			if (getword(buf, sizeof(buf), fp))
				boot_read(buf);
			endline(fp);
			continue;
		} else if (strncasecmp(buf, "cache", 5) == 0) {
			process_zone_line(Z_CACHE, buf, fp, 0);
		} else if (strncasecmp(buf, "primary", 7) == 0) {
			process_zone_line(Z_PRIMARY, buf, fp, 0);
		} else if (strncasecmp(buf, "secondary", 9) == 0) {
			process_zone_line(Z_SECONDARY, buf, fp, 0);
#ifdef STUBS
		} else if (strncasecmp(buf, "stub", 4) == 0) {
			process_zone_line(Z_STUB, buf, fp, 0);
#endif
#ifdef MULTIZDB
		} else if (strncasecmp(buf, "multizone", 9) == 0) {
			process_mzone_line(fp);
			continue;
#endif /* MULTIZDB */
		} else {
			syslog(LOG_ERR, "%s: line %d: unknown field '%s'\n",
				bootfile, lineno+1, buf);
			endline(fp);
			continue;
		}
	}
	(void) my_fclose(fp);
	lineno = slineno;
}

static void
zoneinit(zp)
	register struct zoneinfo *zp;
{
	struct stat sb;

	/*
	 * Try to load zone from backup file,
	 * if one was specified and it exists.
	 * If not, or if the data are out of date,
	 * we will refresh the zone from a primary
	 * immediately.
	 */
	if (!zp->z_source)
		return;
	if (stat(zp->z_source, &sb) == -1 ||
#ifdef MULTIZDB
	    db_load(zp->z_source, zp->z_origin, zp, 0, 0) != 0) {
#else
	    db_load(zp->z_source, zp->z_origin, zp, 0) != 0) {
#endif
		/*
		 * Set zone to be refreshed immediately.
		 */
		zp->z_refresh = INIT_REFRESH;
		zp->z_retry = INIT_REFRESH;
		zp->z_time = tt.tv_sec;
		needmaint = 1;
	} else {
		zp->z_flags |= Z_AUTH;
	}
}

#ifdef ALLOW_UPDATES
/*
 * Look for the authoritative zone with the longest matching RHS of dname
 * and return its zone # or zero if not found.
 */
int
findzone(dname, class)
	char *dname;
	int class;
{
	char *dZoneName, *zoneName;
	int dZoneNameLen, zoneNameLen;
	int maxMatchLen = 0;
	int maxMatchZoneNum = 0;
	int zoneNum;

	dprintf(4, (ddt, "findzone(dname=%s, class=%d)\n", dname, class));
#ifdef DEBUG
	if (debug >= 5) {
		fprintf(ddt, "zone dump:\n");
		for (zoneNum = 1; zoneNum < nzones; zoneNum++)
			printzoneinfo(zoneNum);
	}
#endif

	dZoneName = strchr(dname, '.');
	if (dZoneName == NULL)
		dZoneName = "";	/* root */
	else
		dZoneName++;	/* There is a '.' in dname, so use remainder of
				   string as the zone name */
	dZoneNameLen = strlen(dZoneName);
	for (zoneNum = 1; zoneNum < nzones; zoneNum++) {
		zoneName = (zones[zoneNum]).z_origin;
		zoneNameLen = strlen(zoneName);
		/* The zone name may or may not end with a '.' */
		if (zoneName[zoneNameLen - 1] == '.')
			zoneNameLen--;
		if (dZoneNameLen != zoneNameLen)
			continue;
		dprintf(5, (ddt, "about to strncasecmp('%s', '%s', %d)\n",
			    dZoneName, zoneName, dZoneNameLen));
		if (strncasecmp(dZoneName, zoneName, dZoneNameLen) == 0) {
			dprintf(5, (ddt, "match\n"));
			/*
			 * See if this is as long a match as any so far.
			 * Check if "<=" instead of just "<" so that if
			 * root domain (whose name length is 0) matches,
			 * we use it's zone number instead of just 0
			 */
			if (maxMatchLen <= zoneNameLen) {
				maxMatchZoneNum = zoneNum;
				maxMatchLen = zoneNameLen;
			}
		} else {
			dprintf(5, (ddt, "no match\n"));
		}
	}
	dprintf(4, (ddt, "findzone: returning %d\n", maxMatchZoneNum));
	return (maxMatchZoneNum);
}
#endif /* ALLOW_UPDATES */

static void
get_forwarders(fp)
	FILE *fp;
{
	char buf[BUFSIZ];
	register struct fwdinfo *fip = NULL, *ftp = NULL;

#ifdef SLAVE_FORWARD
	int forward_count = 0;
#endif

	dprintf(1, (ddt, "forwarders "));

	/* on mulitple forwarder lines, move to end of the list */
#ifdef SLAVE_FORWARD
	if (fwdtab != NULL){
		forward_count++;
		for (fip = fwdtab; fip->next != NULL; fip = fip->next)
			forward_count++;
	}
#else
	if (fwdtab != NULL) {
		for (fip = fwdtab; fip->next != NULL; fip = fip->next) {
			;
		}
	}
#endif /* SLAVE_FORWARD */

	while (getword(buf, sizeof(buf), fp)) {
		if (strlen(buf) == 0)
			break;
		dprintf(1, (ddt," %s",buf));
		if (ftp == NULL)
			ftp = (struct fwdinfo *)malloc(sizeof(struct fwdinfo));
		if (inet_aton(buf, &ftp->fwdaddr.sin_addr)) {
			ftp->fwdaddr.sin_port = ns_port;
			ftp->fwdaddr.sin_family = AF_INET;
		} else {
			syslog(LOG_ERR, "'%s' (ignored, NOT dotted quad)",
			       buf);
			dprintf(1, (ddt, " (ignored, NOT dotted quad)"));
			continue;	
		}
#ifdef FWD_LOOP
		if (aIsUs(ftp->fwdaddr.sin_addr)) {
			syslog(LOG_ERR,
			       "Forwarder '%s' ignored, my address",
			       buf);
			dprintf(1, (ddt, " (ignored, my address)"));
			continue;
		}
#endif /* FWD_LOOP */
		ftp->next = NULL;
		if (fwdtab == NULL)
			fwdtab = ftp;	/* First time only */
		else
			fip->next = ftp;
		fip = ftp;
		ftp = NULL;
#ifdef SLAVE_FORWARD
		forward_count++;
#endif /* SLAVE_FORWARD */
	}
	if (ftp)
		free((char *)ftp);
	
#ifdef SLAVE_FORWARD
	/*
	** Set the slave retry time to 60 seconds total divided
	** between each forwarder
	*/
	if (forward_count != 0) {
		slave_retry = (int) (60 / forward_count);
		if(slave_retry <= 0)
			slave_retry = 1;
	}
#endif

	dprintf(1, (ddt, "\n"));
#ifdef DEBUG
	if (debug > 2) {
		for (ftp = fwdtab; ftp != NULL; ftp = ftp->next) {
			fprintf(ddt,"ftp x%x [%s] next x%x\n",
				ftp,
				inet_ntoa(ftp->fwdaddr.sin_addr),
				ftp->next);
		}
	}
#endif
}

static void
free_forwarders()
{
	register struct fwdinfo *ftp, *fnext;

	for (ftp = fwdtab; ftp != NULL; ftp = fnext) {
		fnext = ftp->next;
		free((char *)ftp);
	}
	fwdtab = NULL;
}

static struct zoneinfo *
find_zone(name, type, class) 
	char *name;
	int type, class;
{
	register struct zoneinfo *zp;

        for (zp = &zones[1]; zp < &zones[nzones]; zp++) {
		if (zp->z_type == type && zp->z_class == class &&
		    strcasecmp(name, zp->z_origin) == 0) {
			dprintf(2, (ddt, ", old zone (%d)", zp - zones));
			return (zp);
		}
	}
	dprintf(2, (ddt, ", new zone"));
	return NULL;
}

#ifdef DEBUG
/* prints out the content of zones */
static void
content_zone(end) 
	int end;
{
	int i;

	for (i = 1;  i <= end;  i++) {
		printzoneinfo(i);
	}
}
#endif

struct zoneinfo *
zone_list_pointer(obuf, type, class)
char *obuf;
int type, class;
{
	struct zoneinfo *zp;
	if (type == Z_CACHE)
	{
		return (&zones[0]);
	}
	zp = find_zone(obuf, type, class);
	if (zp) return (zp);
	/*
	** Fill a new zone slot, growing the zone slot list if necessary.
	*/
	for (zp = &zones[1]; zp < &zones[nzones]; ++zp)
	{
		if (zp->z_type == Z_NIL)  return (zp);
	}
	/*
	** This code assumes that nzones never decreases.
	*/
	if (nzones%64 == 0)
	{
		struct zoneinfo *nz;
		dprintf(1, (ddt, "Reallocating zones structure\n"));
		/*
		** realloc() not used since it might damage zones if an error occurs
		*/
		nz = (struct zoneinfo *)malloc((64 + nzones) * sizeof(struct zoneinfo));
		if (nz == 0)
		{
			syslog(LOG_ERR, "no memory for more zones\n");
			dprintf(1, (ddt, "Out of memory for new zones\n"));
			return (0);
		}
		bcopy((char *)zones, (char *)nz, nzones * sizeof(struct zoneinfo));
		bzero((char *)&nz[nzones], 64 * sizeof(struct zoneinfo));
		free(zones);
		zones = nz;
	}
	zp = &zones[nzones++];
	return (zp);
}

static void
zone_reload_bookkeeping(zp, source, mzfp)
struct zoneinfo *zp;
char *source;
FILE *mzfp;
{
#ifdef STUBS
	if (zp->z_type == Z_SECONDARY  ||  zp->z_type == Z_STUB)
#else
	if (zp->z_type == Z_SECONDARY)
#endif
	{
		if (source)
		{
			if (zp->z_source)
			{
				free(source);
			}
			else
			{
				zp->z_source = source;
			}
		}
		if (!(zp->z_flags & Z_AUTH))
		{
			zoneinit(zp);
		}
#ifdef FORCED_RELOAD
		else
		{
			/*
			** Force secondary to try transfer right away after SIGHUP
			*/
			if (reloading)
			{
				zp->z_time = tt.tv_sec;
				needmaint = 1;
			}
		}
#endif /* FORCED_RELOAD */
	}
	/*
	** Misc bookkeeping for all zones
	*/
	zp->z_flags |= Z_FOUND;
#ifdef MULTIZDB
	if (mzfp)
	{
		if (zp->z_msource)
		{
			free(zp->z_msource);
		}
		zp->z_msource = savestr(multizone_file);
	}
#endif
	dprintf(1, (ddt, "zone[%d] type %d: '%s'", zp-zones, zp->z_type, *(zp->z_origin) ? zp->z_origin : "."));
	if (zp->z_refresh && zp->z_time == 0)
	{
		zp->z_time = zp->z_refresh + tt.tv_sec;
	}
	if (zp->z_time <= tt.tv_sec)
	{
		needmaint = 1;
	}
	dprintf(1, (ddt, " z_time %d, z_refresh %d\n", zp->z_time, zp->z_refresh));
}

#ifdef MULTIZDB
void
process_mzone_line(fp)
FILE *fp;
{
	int need_reread;
	FILE *mzfp;
	struct zoneinfo *zp, *przp;
	struct stat mf_time;
	char buf[BUFSIZ];
	if (!getword(buf, sizeof(buf), fp))
	{
		syslog(LOG_ERR, "%s: line %d: missing multizone filename\n", bootfile, lineno+1);
		endline(fp);
		return;
	}
	endline(fp);
	if (multizone_file) free(multizone_file);
	multizone_file = savestr(buf);
	dprintf(1, (ddt, "processing multizone file %s\n", multizone_file));
	mzfp = fopen(multizone_file, "r");
	if (mzfp == NULL)
	{
		syslog(LOG_ERR, "%s: line %d: can't open %s\n", bootfile, lineno+1, multizone_file);
		return;
	}
	/*
	** Strategy to check the timestamp on zones from a multizone file is
	** to look in the list of zones for a primary that was loaded from
	** there since it's z_ftime will be that of the multizone file.  That
	** means that if there are no primary zones in a multizone file
	** (e.g., just a list of secondaries and/or stubs), we'll process it
	** anyhow.  Since the secondaries/stubs have backup files which get
	** checked as if they were named in the boot file, it's no big deal.
	*/
	przp = 0;
	for (zp = &zones[1]; zp < &zones[nzones]; ++zp)
	{
		if (zp->z_msource  &&  zp->z_type == Z_PRIMARY
		&&  strcmp(multizone_file, zp->z_msource) == 0)
		{
			przp = zp;
			break;
		}
	}
	dprintf(2, (ddt, "using primary %s as multizone timestamp\n", przp?przp->z_origin:"--none--"));
	need_reread = 1;
	if (!przp
	||  fstat(mzfp, &mf_time) == -1
	||  przp->z_ftime != mf_time.st_mtime)
	{
		(void)db_load(0, 0, 0, 0, mzfp);
	}
	else
	{
		/*
		** Cycle through all zones with this as the multizone source file
		** and mark them as found.
		*/
		for (zp = &zones[0]; zp < &zones[nzones]; ++zp)
		{
			if (zp->z_msource
			&&  strcmp(multizone_file, zp->z_msource) == 0)
			{
				zone_reload_bookkeeping(zp, 0, mzfp);
			}
		}
	}
}
#endif /* MULTIZDB */

void
process_zone_line(type, type_buf, fp, mzfp)
int type;
char *type_buf;
FILE *fp;
FILE *mzfp;
{
	static int tmpnum;
	struct stat f_time;
	int class, olen;
	char *class_p, *source, obuf[BUFSIZ], buf[BUFSIZ];
	struct zoneinfo *zp;
	class = C_IN;
#ifdef GEN_AXFR
	if (class_p = strchr(type_buf, '/')) class = get_class(class_p+1);
#endif
	/*
	** Read zone origin
	*/
	if (!getword(obuf, sizeof(obuf), fp))
	{
		syslog(LOG_ERR, "%s: line %d: missing origin\n", bootfile, lineno);
		endline(fp);
		return;
	}
	olen = strlen(obuf);
	if ((obuf[olen-1] == '.') && (olen != 1))
	{
		syslog(LOG_ERR, "%s: line %d: zone \"%s\" has trailing dot\n", bootfile, lineno, obuf);
	}
	while ((--olen >= 0) && (obuf[olen] == '.'))
	{
		obuf[olen] = '\0';
	}
	dprintf(1, (ddt, "zone origin %s", obuf[0]?obuf:"."));
	source = 0;
	zp = zone_list_pointer(obuf, type, class);
	if (zp == 0)
	{
		endline(fp);
		return;
	}
#ifdef MULTIZDB
	if (mzfp)
	{
		if (zp->z_msource)
		{
			free(zp->z_msource);
		}
		zp->z_msource = savestr(multizone_file);
		if (type == Z_PRIMARY  ||  type == Z_CACHE)
		{
			/*
			** A source filename isn't used in $primary and $cache
			** directives in a multizone DB file, so fake one to simplify
			** the getword() stuff below.
			*/
			ungetc('!', fp);
		}
	}
#endif /* MULTIZDB */
	zp->z_addrcnt = 0;
	zp->z_type = type;
	zp->z_class = class;
	if (type == Z_CACHE)
	{
		zp->z_origin = "";
	}
	else
	{
		zp->z_origin = savestr(obuf);
	}
	while (getword(buf, sizeof(buf), fp))
	{
		if (
#ifdef STUBS
			(type == Z_SECONDARY  ||  type ==  Z_STUB)
#else
			(type == Z_SECONDARY)
#endif
		&& inet_aton(buf, &zp->z_addr[zp->z_addrcnt]))
		{
			dprintf(1, (ddt, ", addr %s", buf));
			if ((int)++zp->z_addrcnt > NSMAX - 1)
			{
				zp->z_addrcnt = NSMAX - 1;
				dprintf(1, (ddt, "\nns_defs.h NSMAX reached\n"));
			}
		}
		else
		{
			source = savestr(buf);
			break;
		}
	}
	switch (type)
	{
	case Z_PRIMARY:
	case Z_CACHE:
#ifdef MULTIZDB
		if (!source  &&  !mzfp)
#else
		if (!source)
#endif /* MULTIZDB */
		{
			syslog(LOG_ERR, "%s: line %d: zone %s missing source file\n", bootfile, lineno, obuf);
			endline(fp);
			return;
		}
#ifdef ALLOW_UPDATES
		if (type == Z_PRIMARY  &&  getword(buf, sizeof(buf), fp))
		{
			char *flag, *cp;
			flag = buf;
			while (flag)
			{
				cp = strchr(flag, ',');
				if (cp)  *cp++ = 0;
				if (strcasecmp(flag, "dynamic") == 0)
				{
					zp->z_flags |= Z_DYNAMIC;
				}
				else if (strcasecmp(flag, "addonly") == 0)
				{
					zp->z_flags |= Z_DYNADDONLY;
				}
				else
				{
					syslog(LOG_ERR, "%s: line %d: bad flag '%s'\n", bootfile, lineno, flag);
				}
				flag = cp;
			}
		}
#endif /* ALLOW_UPDATES */
		zp->z_refresh = 0;
		if (type == Z_CACHE  &&  getword(buf, sizeof(buf), fp))
		{
#ifdef notyet
			zp->z_refresh = atoi(buf);
			if (zp->z_refresh <= 0)
			{
				syslog(LOG_ERR, "%s: line %d: bad refresh time '%s', ignored\n", bootfile, lineno, buf);
				zp->z_refresh = 0;
			}
			else if (cache_file == 0)
			{
				cache_file = source;
			}
#else
			syslog(LOG_WARNING, "%s: line %d: cache refresh ignored\n", bootfile, lineno);
#endif /* notyet */
		}
		break;
	case Z_SECONDARY:
#ifdef STUBS
	case Z_STUB:	
#endif		
		if (zp->z_addrcnt <= 0)
		{
			syslog(LOG_ERR, "%s: line %d: zone %s missing host address\n", bootfile, lineno, obuf);
			endline(fp);
			return;
		}
		if (source)
		{
			zp->z_flags &= ~Z_TMP_FILE;
		}
		else
		{
			/*
			** We will always transfer this zone again after a reload.
			*/
			zp->z_flags |= Z_TMP_FILE;
			sprintf(buf, "/%s/NsTmp%d.%d", _PATH_TMPDIR, getpid(), tmpnum++);
			source = savestr(buf);
		}
		break;
	}
	endline(fp);
	dprintf(1, (ddt, ", source = %s\n", source));

	/*
	** Check source file timestamps, etc, to see if reload is necessary.
	** If we've loaded this file, and the file has not been modified and
	** contains no $include, then there's no need to reload.  (The
	** $include check is extraneous for secondary zones but does no
	** harm.)
	*/
	if (!zp->z_source ||
		strcmp(source, zp->z_source) || 
		zp->z_flags & Z_INCLUDE ||
		stat(zp->z_source, &f_time) == -1  ||
		zp->z_ftime != f_time.st_mtime)
	{
		/*
		** Need reload or something.
		*/
		dprintf(1, (ddt, "zone %s backup or source file out of date\n", zp->z_origin[0]?zp->z_origin:"."));
		if (zp->z_source)
		{
			free(zp->z_source);
#ifdef CLEANCACHE
			remove_zone((type == Z_CACHE ? fcachetab : hashtab), zp - zones, 1);
#else
			remove_zone((type == Z_CACHE ? fcachetab : hashtab), zp - zones);
#endif /* CLEANCACHE */
			zp->z_flags &= ~Z_AUTH;
		}
		switch (type)
		{
		case Z_PRIMARY:
			zp->z_source = source;
#ifdef MULTIZDB
			if (db_load(zp->z_source, zp->z_origin, zp, 0, mzfp) == 0)
#else
			if (db_load(zp->z_source, zp->z_origin, zp, 0) == 0)
#endif
			{
				zp->z_flags |= Z_AUTH;
			}
#ifdef ALLOW_UPDATES
			/*
			 ** Guarantee calls to ns_maint()
			 */
			zp->z_refresh = maint_interval;
#else 
			zp->z_refresh = 0;  /* no maintenance needed */
			zp->z_time = 0;
#endif /* ALLOW_UPDATES */
			break;
		case Z_CACHE:
			zp->z_source = source;
#ifdef MULTIZDB
			(void)db_load(zp->z_source, zp->z_origin, zp, 0, mzfp);
#else
			(void)db_load(zp->z_source, zp->z_origin, zp, 0);
#endif
			break;
		case Z_SECONDARY:
		case Z_STUB:
			zp->z_source = 0;
			zp->z_flags &= ~Z_AUTH;
			zp->z_serial = 0;
			break;
		}
	}
	else
	{
		if (cache_file != source) free(source);
		dprintf(1, (ddt, "zone %s is up to date\n", zp->z_origin[0]?zp->z_origin:"."));
	}
	zone_reload_bookkeeping(zp, source, mzfp);
	return;
}

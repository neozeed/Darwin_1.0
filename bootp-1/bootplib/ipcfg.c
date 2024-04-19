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
 * ipcfg.c
 * - get the interface configuration information
 *   (iftab for now)
 */

/*
 * Modification History
 * 5/28/99	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#import <stdlib.h>
#import <unistd.h>
#import <stdio.h>
#import <sys/ioctl.h>
#import <strings.h>
#import <netdb.h>
#import <netdb.h>
#import <net/if_types.h>
#import <mach/boolean.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <ctype.h>
#import "ipcfg.h"

static boolean_t
comment_or_blank(char * line)
{
    char * scan;
    
    for (scan = line; *scan; scan++) {
	char ch = *scan;
	switch (ch) {
	case ' ':
	case '\n':
	case '\t':
	    break;
	case '#':
	    return (TRUE);
	default:
	    return (FALSE);
	}
    }
    return (TRUE);
}

static char *
non_blank(char * str, char * buf)
{
    char ch;

    for (ch = *str; ch == '\n' || ch == ' ' || ch == '\t'; ) {
	str++;
	ch = *str;
    }
    if (ch == '\0')
	return NULL;
    for (ch = *str; ch != '\0' && ch != ' ' && ch != '\t' && ch != '\n'; ) {
	*buf++ = ch;
	str++;
	ch = *str;
    }
    *buf = '\0';
    return (str);
}

static int
parse_inet_cfg(char * str, ipcfg_t * cfg, char * msg, boolean_t * is_inet)
{
    char 	buf[64];
    char * 	scan;
    
    *is_inet = TRUE;
    bzero(cfg, sizeof(*cfg));
    cfg->mode = ipcfg_mode_none_e;

    scan = non_blank(str, cfg->ifname);
    if (scan == NULL) {
	sprintf(msg, "no interface name");
	return -1;
    }

    scan = non_blank(scan, buf);
    if (scan == NULL) {
	sprintf(msg, "after '%s'", cfg->ifname);
	return -1;
    }

    if (strcmp(buf, "inet")) {
	*is_inet = FALSE;
	return (0);
    }

    scan = non_blank(scan, buf);
    if (scan == NULL) {
	sprintf(msg, "after inet");
	return -1;
    }
    if (strcmp(buf, "-BOOTP-") == 0 || strcmp(buf, "-AUTOMATIC-") == 0) {
	cfg->mode = ipcfg_mode_bootp_e;
	scan = NULL;
    }
    else if (strcmp(buf, "-DHCP-") == 0) {
	cfg->mode = ipcfg_mode_dhcp_e;
	scan = NULL;
    }
    else if (strcmp(buf, "-INFORM-") == 0) {
	cfg->mode = ipcfg_mode_inform_e;
	scan = non_blank(scan, buf);
	if (scan == NULL) {
	    sprintf(msg, "after -INFORM-");
	    return (-1);
	}
    }
    if (scan != NULL) {
	if (inet_aton(buf, &cfg->addr) == 1) {
	    boolean_t got_netmask = FALSE;
	    boolean_t got_broadcast = FALSE;
	    boolean_t got_up_down = FALSE;

	    if (cfg->mode == ipcfg_mode_none_e)
		cfg->mode = ipcfg_mode_manual_e;
	    cfg->up = TRUE;
	    while (1) {
		scan = non_blank(scan, buf);
		if (scan == NULL)
		    break;

		if (strcmp(buf, "netmask") == 0) {
		    if (got_netmask) {
			sprintf(msg, "duplicate netmask");
			return -1;
		    }
		    scan = non_blank(scan, buf);
		    if (scan == NULL) {
			sprintf(msg, "netmask value missing");
			return -1;
		    }
		    if (inet_aton(buf, &cfg->mask) != 1) {
			sprintf(msg, "invalid netmask '%s'", buf);
			return -1;
		    }
		    got_netmask = TRUE;
		}
		else if (strcmp(buf, "broadcast") == 0) {
		    if (got_broadcast) {
			sprintf(msg, "duplicate broadcast");
			return -1;
		    }
		    scan = non_blank(scan, buf);
		    if (scan == NULL) {
			sprintf(msg, "broadcast value missing");
			return -1;
		    }
		    if (inet_aton(buf, &cfg->broadcast) != 1) {
			sprintf(msg, "invalid broadcast '%s'", buf);
			return -1;
		    }
		    got_broadcast = TRUE;
		}
		else if (strcmp(buf, "up") == 0
			 || strcmp(buf, "down") == 0) {
		    if (got_up_down) {
			sprintf(msg, "up/down specified multiple times");
			return -1;
		    }
		    cfg->up = (strcmp(buf, "up") == 0);
		    got_up_down = TRUE;
		}
	    }
	}
	else {
	    do {
		if (strcmp(buf, "down") == 0)
		    cfg->mode = ipcfg_mode_off_e;
		scan = non_blank(scan, buf);
	    } while (scan);
	}
    }
    return (0);
}

static __inline__ ipcfg_t *
parse_iftab(FILE * f, char * msg, int * how_many)
{
    char 	line[1024];
    int		status = 0;
    ipcfg_t *	entries = NULL;
    int		count = 0;
    int		size = 10;

    *how_many = 0;
    sprintf(msg, "malloc/realloc failed\n");
    entries = malloc(size * sizeof(*entries));
    if (entries == NULL)
	return (NULL);

    while (1) {
	if (fgets(line, sizeof(line), f) == NULL) {
	    if (feof(f) == 0)
		status = -1;
	    break;
	}
	if (comment_or_blank(line) == FALSE) {
	    ipcfg_t *	entry;
	    boolean_t 	is_inet;

	    if (count >= size) {
		size *= 2;
		entries = realloc(entries, sizeof(*entries) * size);
		if (entries == NULL)
		    return (NULL);
	    }
	    entry = entries + count;
	    if (parse_inet_cfg(line, entry, msg, &is_inet) < 0) {
		status = -1;
		break;
	    }
	    if (is_inet) /* save it */
		count++;
	}
    }
    if (status < 0) {
	free(entries);
	return (NULL);
    }
    *how_many = count;
    if (count == 0)
	count = 1; /* for realloc below, don't want it to be 0 */
    return realloc(entries, sizeof(*entries) * count);
}

void
ipcfg_free(ipcfg_table_t * * t)
{
    if (t && *t) {
	if ((*t)->table)
	    free((*t)->table);
	free (*t);
	*t = NULL;
    }
    return;
}

ipcfg_table_t *
ipcfg_from_iftab(FILE * f, char *msg)
{
    int			count;
    ipcfg_t * 		entries;
    ipcfg_table_t * 	t;

    entries = parse_iftab(f, msg, &count);
    if (entries == NULL)
	return (NULL);
    t = malloc(sizeof(*t));
    if (t == NULL) {
	free(entries);
	return NULL;
    }
    t->table = entries;
    t->count = count;
    return (t);
}


void
ipcfg_entry_print(ipcfg_t * entry)
{
    printf("%s %s", entry->ifname, 
	   ipcfg_mode_string(entry->mode));
    if (entry->mode == ipcfg_mode_manual_e) {
	printf(" %s", inet_ntoa(entry->addr));
	printf(" %s", inet_ntoa(entry->mask));
	if (entry->up)
	    printf(" up");
    }
    else if (entry->mode == ipcfg_mode_inform_e) {
	printf(" %s", inet_ntoa(entry->addr));
    }
    printf("\n");
}

void
ipcfg_print(ipcfg_table_t * table) 
{
    int i;

    printf("%d entries\n", table->count);
    for (i = 0; i < table->count; i++)
	ipcfg_entry_print(table->table + i);
    return;
}

#define ANYCHAR		'*'

static boolean_t
ifname_match(char * pattern, char * ifname)
{
    char * pscan;
    char * iscan;

    for (pscan = pattern, iscan = ifname; *pscan && *iscan; 
	 pscan++, iscan++) {
	if (*pscan == ANYCHAR)
	    return (TRUE);
	if (*pscan != *iscan)
	    return (FALSE);
    }
    if (*pscan || *iscan)
	return (FALSE);
    return (TRUE);
}

ipcfg_t *
ipcfg_lookup(ipcfg_table_t * table, char * ifname)
{
    int i;

    for (i = 0; i < table->count; i++) {
	ipcfg_t * entry = table->table + i;

	if (ifname_match(entry->ifname, ifname))
	    return (entry);
    }
    return (NULL);
}

#ifdef TESTING
#import "interfaces.h"

int
main(int argc, char * argv[])
{
    FILE * 		f;
    char *		filename;
    char 		msg[1024];
    ipcfg_table_t *	table = NULL;
    interface_list_t * 	list_p = if_init();
    

    if (argc == 1)
	filename = "/etc/iftab";
    else
	filename = argv[1];
    f = fopen(filename, "r");

    if (f == NULL) {
	fprintf(stderr, "couldn't open %s\n", filename);
	exit (1);
    }
	
    table = ipcfg_from_iftab(f, msg);
    if (table == NULL) {
	printf("parse failed: %s\n", msg);
	exit(1);
    }
    ipcfg_print(table);

    if (list_p) {
	int i;
	for (i = 0; i < if_count(list_p); i++) {
	    interface_t * 	if_p = if_at_index(list_p, i);
	    ipcfg_t *		ipcfg = ipcfg_lookup(table, if_name(if_p));
	    
	    if (ipcfg) {
		printf("Found match for %s:\n", if_name(if_p));
		ipcfg_entry_print(ipcfg);
		printf("\n");
	    }
	}
    }
    ipcfg_free(&table);
    exit(0);
}
#endif TESTING

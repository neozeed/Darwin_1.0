/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)print.c	8.6 (Berkeley) 4/16/94";
#endif
static const char rcsid[] =
	"$FreeBSD: print.c,v 1.33 1998/11/25 09:34:00 dfr Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/ucred.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <sys/cdefs.h>

#if FIXME
#include <vm/vm.h>
#endif /* FIXME */
#include <err.h>
#include <math.h>
#include <nlist.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vis.h>
#include <pwd.h>

#include "ps.h"

extern int mflg, print_all_thread, print_thread_num;
void
printheader()
{
	VAR *v;
	struct varent *vent;

	for (vent = vhead; vent; vent = vent->next) {
		v = vent->var;
		if (v->flag & LJUST) {
			if (vent->next == NULL)	/* last one */
				(void)printf("%s", v->header);
			else
				(void)printf("%-*s", v->width, v->header);
		} else
			(void)printf("%*s", v->width, v->header);
		if (vent->next != NULL)
			(void)putchar(' ');
	}
	(void)putchar('\n');
}

void
command(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	int left;
	char *cp, *vis_env, *vis_args;

	v = ve->var;

	if(!mflg || (print_all_thread && (print_thread_num== 0))) {
	if (cflag) {
		if (ve->next == NULL)	/* last field, don't pad */
			(void)printf("%s", KI_PROC(k)->p_comm);
		else
			(void)printf("%-*s", v->width, KI_PROC(k)->p_comm);
		return;
	}

	if ((vis_args = malloc(strlen(k->ki_args) * 4 + 1)) == NULL)
		err(1, NULL);
	strvis(vis_args, k->ki_args, VIS_TAB | VIS_NL | VIS_NOSLASH);
	if (k->ki_env) {
		if ((vis_env = malloc(strlen(k->ki_env) * 4 + 1)) == NULL)
			err(1, NULL);
		strvis(vis_env, k->ki_env, VIS_TAB | VIS_NL | VIS_NOSLASH);
	} else
		vis_env = NULL;

	if (ve->next == NULL) {
		/* last field */
		if (termwidth == UNLIMITED) {
			if (vis_env)
				(void)printf("%s ", vis_env);
			(void)printf("%s", vis_args);
		} else {
			left = termwidth - (totwidth - v->width);
			if (left < 1) /* already wrapped, just use std width */
				left = v->width;
			if ((cp = vis_env) != NULL) {
				while (--left >= 0 && *cp)
					(void)putchar(*cp++);
				if (--left >= 0)
					putchar(' ');
			}
			for (cp = vis_args; --left >= 0 && *cp != '\0';)
				(void)putchar(*cp++);
		}
	} else
		/* XXX env? */
		(void)printf("%-*.*s", v->width, v->width, vis_args);
	free(vis_args);
	if (vis_env != NULL)
		free(vis_env);
	} else {
		(void)printf("%-*s", v->width, " ");
	}
}

void
ucomm(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s", v->width, KI_PROC(k)->p_comm);
}

char *getname(uid)
	uid_t	uid;
{
	register struct passwd *pw;
	struct passwd *getpwuid();

	pw = getpwuid((short)uid);
	if (pw == NULL) {
		return( "UNKNOWN" );
	}
	return( pw->pw_name );
}

void
logname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	char *s;

	v = ve->var;
	(void)printf("%-*s", v->width, (getname(KI_EPROC(k)->e_ucred.cr_uid)));
}

extern int mach_state_order();
void
state(k, ve)
	KINFO *k;
	VARENT *ve;
{
	struct extern_proc *p;
	int flag,j;
	char *cp;
	VAR *v;
	char buf[16];
	extern char mach_state_table[];

	v = ve->var;
	p = KI_PROC(k);
	flag = p->p_flag;
	cp = buf;

	if(!mflg ) {
	switch (p->p_stat) {
	case SZOMB:
		*cp = 'Z';
		break;

	default:
		*cp = mach_state_table[k->state];
	}
	cp++;
	if (p->p_nice < NZERO)
		*cp++ = '<';
	else if (p->p_nice > NZERO)
		*cp++ = 'N';
	if (flag & P_TRACED)
		*cp++ = 'X';
	if (flag & P_WEXIT && p->p_stat != SZOMB)
		*cp++ = 'E';
	if (flag & P_PPWAIT)
		*cp++ = 'V';
	if (flag & (P_SYSTEM | P_NOSWAP | P_PHYSIO))
		*cp++ = 'L';
	if (KI_EPROC(k)->e_flag & EPROC_SLEADER)
		*cp++ = 's';
	if ((flag & P_CONTROLT) && KI_EPROC(k)->e_pgid == KI_EPROC(k)->e_tpgid)
		*cp++ = '+';
	*cp = '\0';
	(void)printf("%-*s", v->width, buf);
	} else if (print_all_thread) {
		j =  mach_state_order(k->thval[print_thread_num].tb.run_state,
			k->thval[print_thread_num].tb.sleep_time);
		*cp++ = mach_state_table[j];
		*cp++='\0'; 
		(void)printf("%-*s", v->width, buf);
	} else {
		(void)printf("%-*s", v->width, " ");
	}
	
}

void
pri(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	int j=0;

	v = ve->var;
	if (!mflg ) {
		(void)printf("%*d", v->width, k->curpri);
	} else if (print_all_thread) {
		switch(k->thval[print_thread_num].tb.policy) {
			case POLICY_TIMESHARE : 
		j = k->thval[print_thread_num].schedinfo.tshare.cur_priority;
			break;
			case POLICY_FIFO : 
		j = k->thval[print_thread_num].schedinfo.fifo.base_priority;
			break;
			case POLICY_RR : 
		j = k->thval[print_thread_num].schedinfo.rr.base_priority;
			break;
			default :
				j = 0;		
		}
		(void)printf("%*d", v->width, j);
	}else {
		j=0;
		(void)printf("%*d", v->width, j);
		
	}
}

void
uname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if(!mflg || (print_all_thread && (print_thread_num== 0)))
		(void)printf("%-*s",
	  	  (int)v->width, 
			user_from_uid(KI_EPROC(k)->e_ucred.cr_uid, 0));
	else 
		(void)printf("%-*s", (int)v->width, " ");
}

int
s_uname(k)
	KINFO *k;
{
	    return (strlen(user_from_uid(KI_EPROC(k)->e_ucred.cr_uid, 0)));
}

void
runame(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%-*s",
	    (int)v->width, user_from_uid(KI_EPROC(k)->e_pcred.p_ruid, 0));
}

int
s_runame(k)
	KINFO *k;
{
	    return (strlen(user_from_uid(KI_EPROC(k)->e_pcred.p_ruid, 0)));
}

void
tdev(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char buff[16];

	v = ve->var;
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV)
		(void)printf("%*s", v->width, "??");
	else {
		(void)snprintf(buff, sizeof(buff),
		    "%d/%d", major(dev), minor(dev));
		(void)printf("%*s", v->width, buff);
	}
}

void
tname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;

	if(!mflg || (print_all_thread && (print_thread_num== 0))) {
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%*s ", v->width-1, "??");
	else {
		if (strncmp(ttname, "tty", 3) == 0 ||
		    strncmp(ttname, "cua", 3) == 0)
			ttname += 3;
		(void)printf("%*.*s%c", v->width-1, v->width-1, ttname,
			KI_EPROC(k)->e_flag & EPROC_CTTY ? ' ' : '-');
	}
	}
	else {
		(void)printf("%*s ", v->width-1, " ");
	}
}

void
longtname(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	dev_t dev;
	char *ttname;

	v = ve->var;
	dev = KI_EPROC(k)->e_tdev;
	if (dev == NODEV || (ttname = devname(dev, S_IFCHR)) == NULL)
		(void)printf("%-*s", v->width, "??");
	else
		(void)printf("%-*s", v->width, ttname);
}

void
started(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	static time_t now;
	time_t then;
	struct tm *tp;
	char buf[100];

	v = ve->var;
	if (!k->ki_u.u_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}

	then = k->ki_u.u_start.tv_sec;
	tp = localtime(&then);
	if (!now)
		(void)time(&now);
	if (now - k->ki_u.u_start.tv_sec < 24 * 3600) {
		static char fmt[] = "%l:%M%p";
		(void)strftime(buf, sizeof(buf) - 1, fmt, tp);
	} else if (now - k->ki_u.u_start.tv_sec < 7 * 86400) {
		static char fmt[] = "%a%I%p";
		(void)strftime(buf, sizeof(buf) - 1, fmt, tp);
	} else
		(void)strftime(buf, sizeof(buf) - 1, "%e%b%y", tp);
	(void)printf("%-*s", v->width, buf);
}

void
lstarted(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	time_t then;
	char buf[100];

	v = ve->var;
	if (!k->ki_u.u_valid) {
		(void)printf("%-*s", v->width, "-");
		return;
	}
	then = k->ki_u.u_start.tv_sec;
	(void)strftime(buf, sizeof(buf) -1, "%c", localtime(&then));
	(void)printf("%-*s", v->width, buf);
}

void
wchan(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (KI_PROC(k)->p_wchan) {
		if (KI_PROC(k)->p_wmesg)
			(void)printf("%-*.*s", v->width, v->width,
				      KI_EPROC(k)->e_wmesg);
		else
#if FIXME
			(void)printf("%-*lx", v->width,
			    (long)KI_PROC(k)->p_wchan &~ KERNBASE);
#else /* FIXME */
			(void)printf("%-*lx", v->width,
			    (long)KI_PROC(k)->p_wchan);
#endif /* FIXME */
	} else
		(void)printf("%-*s", v->width, "-");
}

#define pgtok(a)        (((a)*getpagesize())/1024)

void
vsize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
#if FIXME
	(void)printf("%*d", v->width,
	    (KI_EPROC(k)->e_vm.vm_map.size/1024));
#else /* FIXME */
	(void)printf("%*d", v->width,
	    (u_long)((k)->tasks_info.virtual_size)/1024);
#endif /* FIXME */
}

void
rssize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	/* XXX don't have info about shared */
	(void)printf("%*lu", v->width,
	    (u_long)((k)->tasks_info.resident_size)/1024);
}

void
p_rssize(k, ve)		/* doesn't account for text */
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
/* FIXME LATER */
	v = ve->var;
	/* (void)printf("%*ld", v->width, "-"); */
	(void)printf("%*lu", v->width,
	    (u_long)((k)->tasks_info.resident_size)/1024);
}

void
cputime(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];
#if 1
	int i=0;
	time_value_t total_time, system_time;
        struct thread_basic_info *tinfo;
#endif
	v = ve->var;
#if FIXME
	if (KI_PROC(k)->p_stat == SZOMB || !k->ki_u.u_valid) {
		secs = 0;
		psecs = 0;
	} else {
		/*
		 * This counts time spent handling interrupts.  We could
		 * fix this, but it is not 100% trivial (and interrupt
		 * time fractions only work on the sparc anyway).	XXX
		 */
#if FIXME
		secs = KI_PROC(k)->p_runtime / 1000000;
		psecs = KI_PROC(k)->p_runtime % 1000000;
#endif /* FIXME */
		if (sumrusage) {
			secs += k->ki_u.u_cru.ru_utime.tv_sec +
				k->ki_u.u_cru.ru_stime.tv_sec;
			psecs += k->ki_u.u_cru.ru_utime.tv_usec +
				k->ki_u.u_cru.ru_stime.tv_usec;
		}
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;
	}
#else /* FIXME */
	total_time = k->tasks_info.user_time;
	system_time = k->tasks_info.system_time;

	time_value_add(&total_time, &k->times.user_time);
	time_value_add(&system_time, &k->times.system_time);
	time_value_add(&total_time, &system_time);

	secs = total_time.seconds;
	psecs = total_time.microseconds;
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;
#endif /* FIXME */
	(void)snprintf(obuff, sizeof(obuff),
	    "%3ld:%02ld.%02ld", secs/60, secs%60, psecs);
	(void)printf("%*s", v->width, obuff);
}

void
putime(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];
	int i=0;
	time_value_t user_time;
        struct thread_basic_info *tinfo;


	v = ve->var;
	if (!mflg) {
		user_time = k->tasks_info.user_time;
		time_value_add(&user_time, &k->times.user_time);
	} else if (print_all_thread) {
		user_time = k->thval[print_thread_num].tb.user_time;
	} else {
		user_time.seconds =0;
		user_time.microseconds =0;
	}

	secs = user_time.seconds;
	psecs = user_time.microseconds;
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;

	(void)snprintf(obuff, sizeof(obuff),
	    "%3ld:%02ld.%02ld", secs/60, secs%60, psecs);
	(void)printf("%*s", v->width, obuff);
}

void
pstime(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	long secs;
	long psecs;	/* "parts" of a second. first micro, then centi */
	char obuff[128];
	int i=0;
	time_value_t total_time, system_time;
        struct thread_basic_info *tinfo;

	v = ve->var;
	if (!mflg) {
		system_time = k->tasks_info.system_time;
		time_value_add(&system_time, &k->times.system_time);
	} else if (print_all_thread) {
		system_time = k->thval[print_thread_num].tb.system_time;
	} else {
		system_time.seconds =0;
		system_time.microseconds =0;
	}
	secs = system_time.seconds;
	psecs = system_time.microseconds;
		/*
		 * round and scale to 100's
		 */
		psecs = (psecs + 5000) / 10000;
		secs += psecs / 100;
		psecs = psecs % 100;

	(void)snprintf(obuff, sizeof(obuff),
	    "%3ld:%02ld.%02ld", secs/60, secs%60, psecs);
	(void)printf("%*s", v->width, obuff);

}

int
getpcpu(k)
	KINFO *k;
{
#if FIXME
	struct proc *p;
	static int failure;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);
	p = KI_PROC(k);
#define	fxtofl(fixpt)	((double)(fixpt) / fscale)

	/* XXX - I don't like this */
	if (p->p_swtime == 0 || (p->p_flag & P_INMEM) == 0)
		return (0.0);
	if (rawcpu)
		return (100.0 * fxtofl(p->p_pctcpu));
	return (100.0 * fxtofl(p->p_pctcpu) /
		(1.0 - exp(p->p_swtime * log(fxtofl(ccpu)))));
#else
	return (k->cpu_usage);
#endif /* FIXME */
}

void
pcpu(k, ve)
	KINFO *k;
	VARENT *ve;
{
#ifndef	TH_USAGE_SCALE
#define	TH_USAGE_SCALE	1000
#endif	TH_USAGE_SCALE
#define usage_to_percent(u)	((u*100)/TH_USAGE_SCALE)
#define usage_to_tenths(u)	(((u*1000)/TH_USAGE_SCALE) % 10)

	if (!mflg ) {
		int cp = getpcpu(k);

		printf(" %2d.%01d",usage_to_percent(cp),
			usage_to_tenths(cp));
	} else if (print_all_thread)  {
		int cp = k->thval[print_thread_num].tb.cpu_usage;
		printf(" %2d.%01d",usage_to_percent(cp),
			usage_to_tenths(cp));
	} else {
		int cp = 0;
		printf(" %2d.%01d",usage_to_percent(cp),
			usage_to_tenths(cp));
	}
		
}

double
getpmem(k)
	KINFO *k;
{
	static int failure;
	struct proc *p;
	struct eproc *e;
	double fracmem;
	int szptudot;

	if (!nlistread)
		failure = donlist();
	if (failure)
		return (0.0);
#if FIXME
	p = KI_PROC(k);
	e = KI_EPROC(k);
	if ((p->p_flag & P_INMEM) == 0)
		return (0.0);
	/* XXX want pmap ptpages, segtab, etc. (per architecture) */
	szptudot = UPAGES;
	/* XXX don't have info about shared */
	fracmem = ((float)e->e_vm.vm_rssize + szptudot)/mempages;
	return (100.0 * fracmem);
#else /* FIXME */
	fracmem = ((float)k->tasks_info.resident_size)/mempages;
	return (100.0 * fracmem);
#endif /* FIXME */
}

void
pmem(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*.1f", v->width, getpmem(k));
}

void
pagein(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	(void)printf("%*ld", v->width,
	    k->ki_u.u_valid ? k->ki_u.u_ru.ru_majflt : 0);
}

void
maxrss(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	/* XXX not yet */
	(void)printf("%*s", v->width, "-");
}

void
tsize(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;
	int dummy=0;

	v = ve->var;
#if 0
	(void)printf("%*ld", v->width, (long)pgtok(KI_EPROC(k)->e_vm.vm_tsize));
#else
	(void)printf("%*ld", v->width, dummy);
#endif
}

void
rtprior(k, ve)
	KINFO *k;
	VARENT *ve;
{
#if FIXME

	VAR *v;
	struct rtprio *prtp;
	char str[8];
	unsigned prio, type;
 
	v = ve->var;
	prtp = (struct rtprio *) ((char *)KI_PROC(k) + v->off);
	prio = prtp->prio;
	type = prtp->type;
	switch (type) {
	case RTP_PRIO_REALTIME:
		snprintf(str, sizeof(str), "real:%u", prio);
		break;
	case RTP_PRIO_NORMAL:
		strncpy(str, "normal", sizeof(str));
		break;
	case RTP_PRIO_IDLE:
		snprintf(str, sizeof(str), "idle:%u", prio);
		break;
	default:
		snprintf(str, sizeof(str), "%u:%u", type, prio);
		break;
	}
	str[sizeof(str) - 1] = '\0';
	(void)printf("%*s", v->width, str);
#endif /* FIXME */
}

/*
 * Generic output routines.  Print fields from various prototype
 * structures.
 */
static void
printval(bp, v)
	char *bp;
	VAR *v;
{
	static char ofmt[32] = "%";
	char *fcp, *cp;

	cp = ofmt + 1;
	fcp = v->fmt;
	if (v->flag & LJUST)
		*cp++ = '-';
	*cp++ = '*';
	while ((*cp++ = *fcp++));

	switch (v->type) {
	case CHAR:
		(void)printf(ofmt, v->width, *(char *)bp);
		break;
	case UCHAR:
		(void)printf(ofmt, v->width, *(u_char *)bp);
		break;
	case SHORT:
		(void)printf(ofmt, v->width, *(short *)bp);
		break;
	case USHORT:
		(void)printf(ofmt, v->width, *(u_short *)bp);
		break;
	case INT:
		(void)printf(ofmt, v->width, *(int *)bp);
		break;
	case UINT:
		(void)printf(ofmt, v->width, *(u_int *)bp);
		break;
	case LONG:
		(void)printf(ofmt, v->width, *(long *)bp);
		break;
	case ULONG:
		(void)printf(ofmt, v->width, *(u_long *)bp);
		break;
	case KPTR:
#if FIXME
		(void)printf(ofmt, v->width, *(u_long *)bp &~ KERNBASE);
#else /* FIXME */
		(void)printf(ofmt, v->width, *(u_long *)bp);
#endif /* FIXME */
		break;
	default:
		errx(1, "unknown type %d", v->type);
	}
}

void
pvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	printval((char *)((char *)KI_PROC(k) + v->off), v);
}

void
evar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	printval((char *)((char *)KI_EPROC(k) + v->off), v);
}

void
uvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (k->ki_u.u_valid)
		printval((char *)((char *)&k->ki_u + v->off), v);
	else
		(void)printf("%*s", v->width, "-");
}

void
rvar(k, ve)
	KINFO *k;
	VARENT *ve;
{
	VAR *v;

	v = ve->var;
	if (k->ki_u.u_valid)
		printval((char *)((char *)(&k->ki_u.u_ru) + v->off), v);
	else
		(void)printf("%*s", v->width, "-");
}

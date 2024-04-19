/*
 *  Top users display for Berkeley Unix
 *  Version 1.8
 *
 *  This program may be freely redistributed to other Unix sites, but this
 *  entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, William LeFebvre, Rice University
 *
 *  This program is designed to run on either Berkeley 4.1 or 4.2 Unix.
 *  Compile with the preprocessor constant "FOUR_ONE" set to get an
 *  executable that will run on Berkeley 4.1 Unix.
 *
 *  The Sun kernel uses scaled integers instead of floating point so compile
 *  with the preprocessor variable "SUN" to get an executable that will run
 *  on Sun Unix version 1.1 or later.
 *
 *  Fixes and enhancements since version 1.5:
 *
 *  Jonathon Feiber at sun:
 *	added "#ifdef SUN" code to make top work on a Sun,
 *	fixed race bug in getkval for getting user structure,
 *	efficiency improvements:  added register variables and
 *	removed the function hashit
 *
 *	added real and virtual memory status line
 *
 *	added second "key" to the qsort comparisn function "proc_compar"
 *	which sorts by on cpu ticks if percentage is equal
 *
 **********************************************************************
 * HISTORY
 * 22-Apr-99  Avadis Tevanian (avie) at Apple
 *	Another rewrite for Mach 3.0
 *
 * 21-Apr-90  Avadis Tevanian (avie) at NeXT
 *	Completely rewritten again for processor sets.
 *
 *  6-May-88  David Golub (dbg) at Carnegie-Mellon University
 *	Completely rewritten for MACH.  This version will NOT run on any
 *	other version of BSD.
 *
 */

#define	Default_TOPN	16	/* default number of lines */
#define	Default_DELAY	1	/* default delay interval */

#include <mach/mach.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <mach/host_info.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/vm_region.h>
#include <mach/vm_task.h>
#include <mach/vm_types.h>

#undef  DEVICE_TYPES_H
#define IOKIT 1
#include <device/device_types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IODrive.h>

#include <kvm.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>

#include <libc.h>
#include <termios.h>
#include <bsd/curses.h>

/* Number of lines of header information on the standard screen */
#define	HEADER_LINES	7

#define sec_to_minutes(t)       ((t) / 60)
#define sec_to_seconds(t)       ((t) % 60)
#define usec_to_100ths(t)       ((t) / 10000)

#ifndef	TH_USAGE_SCALE
#define	TH_USAGE_SCALE	1000
#endif	TH_USAGE_SCALE
#define usage_to_percent(u)	((u*100)/TH_USAGE_SCALE)
#define usage_to_tenths(u)	(((u*1000)/TH_USAGE_SCALE) % 10)


#define time_value_sub(tvp, uvp, vvp)                                   \
  do {                                                                  \
     (vvp)->seconds = (tvp)->seconds - (uvp)->seconds;                  \
     (vvp)->microseconds = (tvp)->microseconds - (uvp)->microseconds;   \
     if ((vvp)->microseconds < 0) {                                     \
         (vvp)->seconds--;                                              \
         (vvp)->microseconds += 1000000;                                \
     }                                                                  \
  } while (0)
     


/* From libcurses */
int wclear(WINDOW *win);
int wmove(WINDOW *win, int y, int x);
int wrefresh(WINDOW *win);
int endwin(void);
int werase(WINDOW *win);

int total_threads;

struct timeval cur_tod;
struct timeval start_tod;
struct timeval last_tod;
struct timeval elapsed_tod;
int    elapsed_milliseconds;
int    newLINES = 0;
int    Header_lines = HEADER_LINES;

int do_proc0_vm;
int sort_by_usage;
int wide_output;
int events_only;
int events_delta;
int events_accumulate;
long start_time = 0;


struct io_stats {
        int    io_accum;
        int    io_prev;
        int    io;
        int    kbytes_accum;
        int    kbytes_prev;
        int    kbytes;
};

struct io_stats i_net, o_net;
struct io_stats i_dsk, o_dsk;
struct io_stats i_vm,  o_vm;


io_iterator_t       drivelist  = 0;  /* needs release */
mach_port_t         masterPort = 0;


struct	proc_info {
	uid_t			uid;
	short			pid;
	short			ppid;
	short			pgrp;
	int			status;
	int			flag;

	int			state;
	int			pri;
	int			base_pri;
	boolean_t		all_swapped;
        boolean_t               has_idle_thread;
	time_value_t		total_time;
	time_value_t		idle_time;
	time_value_t		beg_total_time;
	time_value_t		beg_idle_time;

	vm_size_t		virtual_size;
	vm_size_t		resident_size;
	vm_size_t		orig_virtual_size;
	vm_offset_t		drsize, dvsize;
        unsigned int            shared;
        unsigned int            private;
        unsigned int            vprivate;
        unsigned int            aliased;
        int                     obj_count;
	int			cpu_usage;
	int			cpu_idle;

	char			command[20];

	int			num_ports;
        int                     orig_num_ports;
        int                     dnum_ports;
	int			num_threads;
	thread_basic_info_t	threads;	/* array */
        task_events_info_data_t tei;
        task_events_info_data_t deltatei;
        task_events_info_data_t accumtei;
};

typedef	struct proc_info	*proc_info_t;

mach_port_t host_priv_port, host_port;

struct object_info {
        int 	            id;
        int                 pid;
        int                 share_type;
        int                 resident_page_count;
        int                 ref_count;
        int                 task_ref_count;
        int                 size;
        struct object_info  *next;
};

#define OBJECT_TABLE_SIZE	537
#define OT_HASH(object) (((unsigned)object)%OBJECT_TABLE_SIZE)

struct object_info      *shared_hash_table[OBJECT_TABLE_SIZE];

struct object_info *of_free_list = 0;


/*
 *	Translate thread state to a number in an ordered scale.
 *	When collapsing all the threads' states to one for the
 *	entire task, the lower-numbered state dominates.
 */
#define	STATE_MAX	7

int
mach_state_order(s, sleep_time)
        int s;
        long sleep_time;
 {
	switch (s) {
	case TH_STATE_RUNNING:		return(1);
	case TH_STATE_UNINTERRUPTIBLE:
					return(2);
	case TH_STATE_WAITING:		return((sleep_time > 20) ? 4 : 3);
	case TH_STATE_STOPPED:		return(5);
	case TH_STATE_HALTED:		return(6);
	default:			return(7);
	}
}
			    /*01234567 */
char	mach_state_table[] = "ZRUSITH?";

char *	state_name[] = {
		"zombie",
		"running",
		"stuck",
		"sleeping",
		"idle",
		"stopped",
		"halted",
		"unknown",
};
int		state_breakdown[STATE_MAX+1];


char *state_to_string(pi)
	proc_info_t	pi;
{
	static char	s[5];		/* STATIC! */

	s[0] = mach_state_table[pi->state];
	s[1] = (pi->all_swapped) ? 'W' : ' ';
	s[2] = (pi->base_pri > 50) ? 'N' :
			(pi->base_pri < 40) ? '<' : ' ';
	s[3] = ' ';
	s[4] = '\0';
	return(s);
}

void print_time(char *p, time_value_t t)
{
	long	seconds, useconds, minutes, hours;

	seconds = t.seconds;
	useconds = t.microseconds;
	minutes = seconds / 60;
	hours = minutes / 60;

	if (minutes < 100) { // up to 100 minutes
		sprintf(p, "%2ld:%02ld.%02ld", minutes, seconds % 60,
			usec_to_100ths(useconds));
	}
	else if (hours < 100) { // up to 100 hours
		sprintf(p, "%2ld:%02ld:%02ld", hours, minutes % 60,
				seconds % 60);
	}
	else {
		sprintf(p, "%4ld hrs", hours);
	}
}


void
print_usage(char *p, int cpu_usage)
{
        int left_of_decimal;
        int right_of_decimal; 

	if (elapsed_milliseconds) {
	        left_of_decimal = (cpu_usage * 100) / elapsed_milliseconds;
	
		right_of_decimal = (((cpu_usage * 100) - (left_of_decimal * elapsed_milliseconds)) * 10) /
		                   elapsed_milliseconds;
	} else {
	        left_of_decimal = 0;
		right_of_decimal = 0;
	}
	sprintf(p, "%3d.%01d%%%%", left_of_decimal, right_of_decimal);	/* %cpu */
}



char *
digits(n)
	float	n;
{
	static char	tmp[10];	/* STATIC! */

	if ((n > 0) && (n < 10))
		sprintf(tmp, "%4.2f", n);
	else if ((n > 0) && (n < 100))
		sprintf(tmp, "%4.1f", n);
	else if ((n < 0) && (n > -10))
		sprintf(tmp, "%4.1f", n);
	else
		sprintf(tmp, "%4.0f", n);
	return(tmp);
}

char *
mem_to_string(n)
	unsigned	int	n;
{
	static char	s[10];		/* STATIC! */

	/* convert to kilobytes */
	n /= 1024;

	if (n > 1024*1024)
		sprintf(s, "%sG", digits(((float)n)/(1024.0*1024.0)));
	else if (n > 1024)
		sprintf(s, "%sM", digits((float)n/(1024.0)));
	else
		sprintf(s, "%dK", n);

	return(s);
}

char *
offset_to_string(n)
	int		n;
{
	static char	s[10];		/* STATIC! */
	int		an;

	/* convert to kilobytes */
	n /= 1024;
	an = abs(n);

	if (an > 1024*1024)
		sprintf(s, "%sG", digits(((float)n)/(1024.0*1024.0)));
	else if (an > 1024)
		sprintf(s, "%sM", digits((float)n/(1024.0)));
	else
		sprintf(s, "%dK", n);

	return(s);
}

mach_port_t get_host_priv()
{
	kern_return_t	error;
	mach_port_t	host_control, device, wledger, pledger, security;

	// following mechanism for getting privileged port will change...
	error = bootstrap_ports(mach_task_self(), &host_control, &device,
			&wledger, &pledger, &security);
	if (error != KERN_SUCCESS) {
	        mach_error("bootstrap_ports", error);
	        exit(EXIT_FAILURE);
	}
	return(host_control);
}

mach_port_t get_host_port()
{
	return(mach_host_self());
}

void
shared_hash_enter(int obj_id, int share_type, int resident_page_count, int ref_count, int size, int pid)
{
        register struct object_info **bucket;
        register struct object_info *of;

	of = shared_hash_table[OT_HASH(obj_id/OBJECT_TABLE_SIZE)];
	while (of) {
	        if (of->id == obj_id) {
		        of->size += size;
		        of->task_ref_count++;
			of->pid = pid;
			return;
		}
		of = of->next;
	}
	bucket = &shared_hash_table[OT_HASH(obj_id/OBJECT_TABLE_SIZE)];

	if (of = of_free_list)
	        of_free_list = of->next;
	else
	        of = (struct object_info *) malloc(sizeof(*of));

	of->resident_page_count = resident_page_count;
	of->id = obj_id;
	of->share_type = share_type;
	of->ref_count = ref_count;
	of->task_ref_count = 1;
	of->pid = pid;
	of->size = size;

	of->next = *bucket;
	*bucket = of;
}

void
pmem_doit(task_port_t task, int pid, int *shared, int *private, int *aliased, int *obj_count, int *vprivate)
{
	vm_address_t	address = 0;
	kern_return_t	err = 0;
	register int    i;

	*obj_count = *aliased = *shared = *private = *vprivate = 0;

	while (1) {
		mach_port_t		object_name;
		vm_region_top_info_data_t info;
		mach_msg_type_number_t  count;
	        vm_size_t		size;

		count = VM_REGION_TOP_INFO_COUNT;

		if (err = vm_region(task, &address, &size, VM_REGION_TOP_INFO, (vm_region_info_t)&info,
				    &count, &object_name))
		        break;

		*obj_count += 1;

		switch (info.share_mode) {

		case SM_PRIVATE:
		        *private  += info.private_pages_resident * vm_page_size;
		        *vprivate += size;
		        break;

		case SM_EMPTY:
		        break;

		case SM_COW:
			if (pid)
                                shared_hash_enter(info.obj_id, SM_COW, info.shared_pages_resident,
						  info.ref_count, size, pid);
		        *private  += info.private_pages_resident * vm_page_size;
		        *vprivate += info.private_pages_resident * vm_page_size;
			break;

		case SM_SHARED:
			if (pid)
                                shared_hash_enter(info.obj_id, SM_SHARED, info.shared_pages_resident,
						  info.ref_count, size, pid);
		        break;
		}
		address += size;
        }
        for (i = 0; i < OBJECT_TABLE_SIZE; i++) {
	        register struct object_info *sl;

	        sl = shared_hash_table[i];
		
	        while (sl) {
		        if (sl->pid == pid) {
			        if (sl->ref_count == sl->task_ref_count && sl->share_type == SM_SHARED) {
			                sl->share_type = SM_PRIVATE_ALIASED;
				
					*aliased  += sl->resident_page_count * vm_page_size;
				        *vprivate += sl->size;
				} else
				        *shared   += sl->resident_page_count * vm_page_size;
			}
			sl->task_ref_count = 0;

			sl = sl->next;
		}
	}
}

void
pmem_shared_resident(int *total, int *number)
{       register int i;
        register int total_size;
	register int total_num;
	register struct object_info *sl, *next;
  
	total_size = total_num = 0;

        for (i = 0; i < OBJECT_TABLE_SIZE; i++) {
	        sl = shared_hash_table[i];
		shared_hash_table[i] = 0;
		
	        while (sl) {
		        if (sl->share_type != SM_PRIVATE_ALIASED) {
			        total_size += sl->resident_page_count;
				total_num++;
			}
			next = sl->next;

			sl->next = of_free_list;
			of_free_list = sl;

			sl = next;
		}
	}
	*number = total_num;
	*total  = total_size * vm_page_size;
}

/* All of this should come out of the process manager... */

void get_proc_info(kpb, pi)
	struct kinfo_proc	*kpb;
	struct proc_info	*pi;
{
	task_port_t	task;
	mach_port_array_t	names, types;
	unsigned int	ncnt, tcnt;

	pi->uid	= kpb->kp_eproc.e_ucred.cr_uid;
	pi->pid	= kpb->kp_proc.p_pid;
	pi->ppid	= kpb->kp_eproc.e_ppid;
	pi->pgrp	= kpb->kp_eproc.e_pgid;
	pi->status	= kpb->kp_proc.p_stat;
	pi->flag	= kpb->kp_proc.p_flag;

	/*
	 *	Find the other stuff
	 */
	if (task_for_pid(mach_task_self(), pi->pid, &task) != KERN_SUCCESS) {
		pi->status = SZOMB;
	}

	else {
		task_basic_info_data_t	ti;
		unsigned int		count;
		thread_array_t		thread_table;
		unsigned int		table_size;
		thread_basic_info_t	thi;
		thread_basic_info_data_t thi_data;
		int			i, t_state;

		count = TASK_BASIC_INFO_COUNT;
		if (task_info(task, TASK_BASIC_INFO, (task_info_t)&ti,
				&count) != KERN_SUCCESS) {
			pi->status = SZOMB;
		} else {
		        pi->orig_virtual_size = ti.virtual_size;
			pi->virtual_size = ti.virtual_size;

			pi->resident_size = ti.resident_size;

			if ((pi->pid || do_proc0_vm) && (!events_only))
			        pmem_doit(task, pi->pid, &pi->shared, &pi->private, &pi->aliased, &pi->obj_count, &pi->vprivate);
			else {
			        pi->shared    = 0;
			        pi->private   = 0;
				pi->vprivate  = 0;
			        pi->aliased   = 0;
			        pi->obj_count = 0;
			}
			pi->total_time = ti.user_time;
			time_value_add(&pi->total_time, &ti.system_time);
			
			pi->idle_time.seconds = 0;
			pi->idle_time.microseconds = 0;

			if (task_threads(task, &thread_table, &table_size) != KERN_SUCCESS)
			        pi->status = SZOMB;
			else {
			        pi->state = STATE_MAX;
				pi->pri = 255;
				pi->base_pri = 255;
				pi->all_swapped = TRUE;
				pi->has_idle_thread = FALSE;

				thi = &thi_data;

				pi->num_threads = table_size;
				total_threads += table_size;

				for (i = 0; i < table_size; i++) {
				        count = THREAD_BASIC_INFO_COUNT;
					if (thread_info(thread_table[i], THREAD_BASIC_INFO,
							(thread_info_t)thi, &count) == KERN_SUCCESS) {

					        if (thi->flags & TH_FLAGS_IDLE) {
						        pi->has_idle_thread = TRUE;
						    
							time_value_add(&pi->idle_time, 
								       &thi->user_time);
							time_value_add(&pi->idle_time,
								       &thi->system_time);
						} else {
						        time_value_add(&pi->total_time, 
								       &thi->user_time);
							time_value_add(&pi->total_time,
								       &thi->system_time);
						}
						t_state = mach_state_order(thi->run_state,
									   thi->sleep_time);
						if (t_state < pi->state)
						        pi->state = t_state;
// update priority info based on schedule policy
//					        if (thi->cur_priority < pi->pri)
//						        pi->pri = thi->cur_priority;
//					        if (thi->base_priority < pi->base_pri)
//						        pi->base_pri = thi->base_priority;
						if ((thi->flags & TH_FLAGS_SWAPPED) == 0)
						        pi->all_swapped = FALSE;

					}
					if (task != mach_task_self()) {
					        mach_port_deallocate(mach_task_self(),
								     thread_table[i]);
					}
				}
				(void) vm_deallocate(mach_task_self(), (vm_offset_t)thread_table,
						     table_size * sizeof(*thread_table));

				if (mach_port_names(task, &names, &ncnt,
						    &types, &tcnt) == KERN_SUCCESS) {
				        pi->num_ports = ncnt;
					pi->orig_num_ports = ncnt;
					(void) vm_deallocate(mach_task_self(),
							     (vm_offset_t) names,
							     ncnt * sizeof(*names));
					(void) vm_deallocate(mach_task_self(),
							     (vm_offset_t) types,
							     tcnt * sizeof(*types));
				} else {
				        pi->num_ports = -1;
				}
			
				if (events_only) {
				        task_events_info_data_t	tei;

					count = TASK_EVENTS_INFO_COUNT;
					if (task_info(task, TASK_EVENTS_INFO, (task_info_t)&tei,
						      &count) != KERN_SUCCESS) {
					        pi->status = SZOMB;
					} else {
					        pi->tei = tei;
						
					}
				}
			}
		}
		if (task != mach_task_self()) {
			mach_port_deallocate(mach_task_self(), task);
		}
	}

	(void) strncpy(pi->command, kpb->kp_proc.p_comm,
				sizeof(kpb->kp_proc.p_comm)-1);
	pi->command[sizeof(kpb->kp_proc.p_comm)-1] = '\0';
}


/*
 *  signal handlers
 */

void leave()			/* exit under normal conditions -- INT handler */
{
	move(LINES - 1, 0);
	refresh();
	endwin();
	exit(0);
}

void quit(status)		/* exit under duress */
	int status;
{
	endwin();
	exit(status);
}

void sigwinch()
{
        newLINES = 1;
}


/*
 *  comparison function for "qsort"
 *  Do first order sort based on cpu percentage computed by kernel and
 *  second order sort based on total time for the process.
 */
 
int proc_compar(p1, p2)
	register struct proc_info **p1;
	register struct proc_info **p2;
{
	if (sort_by_usage) {
		if ((*p1)->cpu_usage < (*p2)->cpu_usage)
			return(1);
		else if ((*p1)->cpu_usage > (*p2)->cpu_usage)
			return(-1);
		else {
			if ((*p1)->total_time.seconds < (*p2)->total_time.seconds)
				return(1);
			else
				return(-1);
		}
	}
	else {
		if ((*p1)->pid < (*p2)->pid)
			return(1);
		else
			return(-1);
	}
}


int			nproc, total_procs, old_procs;
struct kinfo_proc	*kbase, *kpb;
struct proc_info	*proc,  *pp, *oldproc;
struct proc_info	**pref, **prefp;

int		topn  = 0;
int             wanted_topn = 0;
vm_size_t	pagesize;



void grab_task(task)
	task_t	task;
{
	int			pid;
	size_t			size;
	kern_return_t		ret;
	struct kinfo_proc	ki;
	int			mib[4];

	ret = pid_for_task(task, &pid);
	if (ret != KERN_SUCCESS)
		return;
	size = sizeof(ki);
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	if (sysctl(mib, 4, &ki, &size, NULL, 0) < 0) {
	        perror("failure calling sysctl");
		exit(1);
	}
	if (ki.kp_proc.p_stat == 0) {
	        state_breakdown[0]++;
		return;
	}
	if (total_procs == nproc) {
		nproc *= 2;
		kbase = (struct kinfo_proc *) realloc(kbase,
						      nproc*sizeof(struct kinfo_proc));
		bzero(&kbase[total_procs], total_procs*sizeof(struct kinfo_proc));
		proc  = (struct proc_info *) realloc(proc,
						     nproc*sizeof(struct proc_info));
		bzero(&proc[total_procs], total_procs*sizeof(struct proc_info));
		oldproc  = (struct proc_info *) realloc(oldproc,
							nproc*sizeof(struct proc_info));
		bzero(&oldproc[total_procs], total_procs*sizeof(struct proc_info));
		pref  = (struct proc_info **) realloc(pref,
						      nproc*sizeof(struct proc_info *));
		bzero(&pref[total_procs], total_procs*sizeof(struct proc_info *));
	}
	kbase[total_procs] = ki;
	total_procs++;
}

void update_histdata()
{
	struct proc_info	*pp, *oldp;
	int			i, j, pid;
	time_value_t		elapsed_time;

	i = 0;
	pp = proc;

	// XXX use linear search since list is usually small

	while (i < total_procs) {
		j = 0;
		oldp = oldproc;
		pid = pp->pid;

		while (j < old_procs) {
			if (oldp->pid == pid) {
				pp->drsize = pp->resident_size -
						oldp->resident_size;
				pp->dvsize = pp->virtual_size -
						oldp->orig_virtual_size;
				pp->orig_virtual_size = 
						oldp->orig_virtual_size;

				pp->dnum_ports = pp->num_ports -
				                oldp->orig_num_ports;
				pp->orig_num_ports =
				                oldp->orig_num_ports;

				if (pp->has_idle_thread == TRUE) {
				        if (events_accumulate) {
						time_value_sub(&pp->idle_time, &oldp->beg_idle_time, &elapsed_time);
						pp->beg_idle_time = oldp->beg_idle_time;
						pp->idle_time = elapsed_time;
					} else
					        time_value_sub(&pp->idle_time, &oldp->idle_time, &elapsed_time);

					pp->cpu_idle = (elapsed_time.seconds * 1000) + (elapsed_time.microseconds / 1000);
				}
				if (events_accumulate) {
				        time_value_sub(&pp->total_time, &oldp->beg_total_time, &elapsed_time);
					pp->beg_total_time = oldp->beg_total_time;
					pp->total_time = elapsed_time;
				} else
				        time_value_sub(&pp->total_time, &oldp->total_time, &elapsed_time);

				pp->cpu_usage = (elapsed_time.seconds * 1000) + (elapsed_time.microseconds / 1000);

				if (events_delta)
				{
				    pp->deltatei.pageins = pp->tei.pageins - oldp->tei.pageins;
				    pp->deltatei.faults = pp->tei.faults - oldp->tei.faults;
				    pp->deltatei.cow_faults = pp->tei.cow_faults - oldp->tei.cow_faults;
				    pp->deltatei.messages_sent = pp->tei.messages_sent - oldp->tei.messages_sent;
				    pp->deltatei.messages_received = pp->tei.messages_received - oldp->tei.messages_received;
				    pp->deltatei.syscalls_unix = pp->tei.syscalls_unix - oldp->tei.syscalls_unix;
				    pp->deltatei.syscalls_mach = pp->tei.syscalls_mach - oldp->tei.syscalls_mach;
				    pp->deltatei.csw = pp->tei.csw - oldp->tei.csw;
				}
				if (events_accumulate)
				{
				    pp->deltatei.pageins = pp->tei.pageins - oldp->accumtei.pageins;
				    pp->deltatei.faults = pp->tei.faults - oldp->accumtei.faults;
				    pp->deltatei.cow_faults = pp->tei.cow_faults - oldp->accumtei.cow_faults;
				    pp->deltatei.messages_sent = pp->tei.messages_sent - oldp->accumtei.messages_sent;
				    pp->deltatei.messages_received = pp->tei.messages_received - oldp->accumtei.messages_received;
				    pp->deltatei.syscalls_unix = pp->tei.syscalls_unix - oldp->accumtei.syscalls_unix;
				    pp->deltatei.syscalls_mach = pp->tei.syscalls_mach - oldp->accumtei.syscalls_mach;
				    pp->deltatei.csw = pp->tei.csw - oldp->accumtei.csw;

				    pp->accumtei = oldp->accumtei;
				}
				break;
			}
			j++;
			oldp++;
		}
		if (j >= old_procs) {
		        if (events_accumulate) {
			        pp->accumtei = pp->tei;
				pp->beg_total_time = pp->total_time;
				pp->beg_idle_time = pp->idle_time;

				pp->idle_time.seconds = 0;
				pp->idle_time.microseconds = 0;
				pp->total_time.seconds = 0;
				pp->total_time.microseconds = 0;
			}
			bzero(&pp->deltatei, sizeof (task_events_info_data_t));

			pp->drsize = 0;
			pp->dvsize = 0;
			pp->dnum_ports = 0;
			pp->cpu_usage = 0;
			pp->cpu_idle = 0;
		}
		i++;
		pp++;
	}
	bcopy(proc, oldproc, total_procs*sizeof(struct proc_info));
	old_procs = total_procs;
}

void read_proc_table()
{
	mach_port_t	host;
	processor_set_t	*psets;
	task_t		*tasks;
	unsigned int	pcount, tcount;
	kern_return_t	ret;
	processor_set_t	p;
	int		i, j;

	total_procs = 0;
	total_threads = 0;

	host = host_priv_port;

	if (host == MACH_PORT_NULL) {
		printf("Insufficient privileges.\n");
		exit(0);
	}
	ret = host_processor_sets(host, &psets, &pcount);
	if (ret != KERN_SUCCESS) {
		mach_error("host_processor_sets", ret);
		exit(0);
	}
	for (i = 0; i < pcount; i++) {
		ret = host_processor_set_priv(host, psets[i], &p);
		if (ret != KERN_SUCCESS) {
			mach_error("host_processor_set_priv", ret);
			exit(0);       
		}
		
		ret = processor_set_tasks(p, &tasks, &tcount);
		if (ret != KERN_SUCCESS) {
			mach_error("processor_set_tasks", ret);
			exit(0);
		}
		for (j = 0; j < tcount; j++) {
			grab_task(tasks[j]);
			// don't delete our own task port
			if (tasks[j] != mach_task_self())
				mach_port_deallocate(mach_task_self(),	
				tasks[j]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)tasks,
			      tcount * sizeof(task_t));
		mach_port_deallocate(mach_task_self(), p);
		mach_port_deallocate(mach_task_self(), psets[i]);
	}
	vm_deallocate(mach_task_self(), (vm_address_t)psets,
		 pcount * sizeof(processor_set_t));
}

main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	*myname = "top";
	int	delay = Default_DELAY;
	kern_return_t	error;

        void screen_update();

	/* get our name */
	if (argc > 0) {
		if ((myname = rindex(argv[0], '/')) == 0) {
			myname = argv[0];
		}
		else {
			myname++;
		}
	}

	/* check for options */
	sort_by_usage = 0;
	wide_output = 0;
	do_proc0_vm = 0;
	events_only = 0;
	events_delta = 0;
	events_accumulate = 0;

	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 's':
			delay = atoi(&argv[1][2]);
			break;
		case 'u':
			sort_by_usage = 1;
			break;
		case 'w':
		        wide_output = 1;
			break;
		case 'k':
		        do_proc0_vm = 1;
			break;
		case 'e':
		        events_only = 1;
		        break;
		case 'd':
		        events_only = 1;
		        events_delta = 1;
			break;
		case 'a':
		        events_only = 1;
		        events_accumulate = 1;
			break;
		default:
			fprintf(stderr, "Usage: %s [-u] [-w] [-k] [-sn] [-e] [-d] [number]\n", myname);
			fprintf(stderr, "  -u      enables sort by usage\n");
			fprintf(stderr, "  -w      enables wide output of additional info\n");
			fprintf(stderr, "  -k      generate vm info for kernel(proc 0)... expensive\n");
			fprintf(stderr, "  -sn     change sample rate to every n seconds\n");
			fprintf(stderr, "  -e      switch to events info counter mode\n");
			fprintf(stderr, "  -d      switch to events info counter delta mode\n");
			fprintf(stderr, "  -a      switch to events info counter accumulate mode\n");
			fprintf(stderr, "  number  limit number of processes monitored\n");

			exit(1);
		}
		argc--;
		argv++;
	}

	if (events_only)
	  {
	    if ( wide_output || do_proc0_vm)
	      {
		fprintf(stderr, " The -w and -k flag have no effect in event mode.\n");
		wide_output = 0;
		do_proc0_vm = 0;
	      }
	  }

	host_priv_port = get_host_priv();
	host_port = get_host_port();

	/* get count of top processes to display (if any) */
	if (argc > 1) {
		wanted_topn = topn = atoi(argv[1]);
	} else
	        wanted_topn = -1;

	/* allocate space for proc structure array and array of pointers */
	nproc = 50;		/* starting point */
	kbase = (struct kinfo_proc *) malloc(nproc*sizeof(struct kinfo_proc));
	bzero(kbase, nproc*sizeof(struct kinfo_proc));
	proc  = (struct proc_info *) malloc(nproc*sizeof(struct proc_info));
	bzero(proc, nproc*sizeof(struct proc_info));
	oldproc  = (struct proc_info *) malloc(nproc*sizeof(struct proc_info));
	bzero(oldproc, nproc*sizeof(struct proc_info));
	pref  = (struct proc_info **) malloc(nproc*sizeof(struct proc_info *));
	bzero(pref, nproc*sizeof(struct proc_info *));

	(void) host_page_size(host_port, &pagesize);
	/* initializes curses and screen (last) */
	initscr();
	refresh();
	erase();
	clear();
	refresh();

	/* set up signal handlers */
	signal(SIGINT, leave);
	signal(SIGQUIT, leave);
	signal(SIGWINCH, sigwinch);
	
	/* can only display (LINES - Header_lines) processes */
	if (topn > LINES - Header_lines) {
		printw("Warning: this terminal can only display %d processes...\n",
				LINES - Header_lines);
		refresh();
		sleep(2);
		topn = LINES - Header_lines;
		clear();
	}
	if (topn == 0) {	// use default
		// leave one blank line at bottom

		topn = LINES - Header_lines - 1;
	}

	/* prime the pump for gathering networking stats */
	kread(0, 0, 0);

	/**************************************************/
	/* get ports and services for IODriveStats */
	/* Obtain the I/O Kit communication handle */

	error = IOMasterPort(bootstrap_port, &masterPort);

	/* Obtain the list of all drive objects */
	
	error = IOServiceGetMatchingServices(masterPort,
					     IOServiceMatching("IODrive"),
					     &drivelist);

	gettimeofday(&cur_tod, NULL);
	start_tod = cur_tod;
	elapsed_milliseconds = 0;

	/* main loop */

	while (1) {
	        if (newLINES) {
		        int   n;

			initscr();
			erase();
			clear();
			refresh();

		        n = LINES - Header_lines;

			if (topn >= n)
			        topn = n;
			else {
			        if (wanted_topn == -1)
				        topn = n;
			        else if (topn < wanted_topn) {
				        if (wanted_topn < n)
					        topn = wanted_topn;
					else
					        topn = n;
				}
			}
		        newLINES = 0;
		}
		(void)screen_update();

		sleep(delay);

		last_tod = cur_tod;
		gettimeofday(&cur_tod, NULL);

		if (events_accumulate)
		        timersub(&cur_tod, &start_tod, &elapsed_tod);
		else
		        timersub(&cur_tod, &last_tod, &elapsed_tod);

		elapsed_milliseconds = (elapsed_tod.tv_sec * 1000) + (elapsed_tod.tv_usec / 1000);
	}
}

void screen_update()
{
	char	c;
	int	i, n, mpid;
	int	active_procs;
	int	avenrun[3];
	long	curr_time;
	long    elapsed_secs;
	vm_size_t	total_virtual_size;
	vm_size_t	total_private_size;
	vm_size_t	total_aliased_size;
	vm_size_t       total_shared_size;
	int             total_shared_objects;
	vm_statistics_data_t	vm_stat;
	struct host_load_info load_data;
	int	host_count;
	kern_return_t	error;
	char    tbuf[256];
	int     clen;

	bzero((char *)state_breakdown, sizeof(state_breakdown));

	/* clear for new display */
	erase();

	/* read all of the process information */
	read_proc_table();

	/* get the load averages */
	host_count = sizeof(load_data)/sizeof(integer_t);
	error = host_statistics(host_priv_port, HOST_LOAD_INFO,
			(host_info_t)&load_data, &host_count);
	if (error != KERN_SUCCESS) {
	        mach_error("host_statistics", error);
	        exit(EXIT_FAILURE);
	}

	avenrun[0] = load_data.avenrun[0];
	avenrun[1] = load_data.avenrun[1];
	avenrun[2] = load_data.avenrun[2];

	/* get total - systemwide main memory usage structure */
	host_count = sizeof(vm_stat)/sizeof(integer_t);
	error = host_statistics(host_priv_port, HOST_VM_INFO,
			(host_info_t)&vm_stat, &host_count);
	if (error != KERN_SUCCESS) {
	        mach_error("host_info", error);
	        exit(EXIT_FAILURE);
	}
	getNETWORKcounters();
	getDISKcounters();

	/* count up process states and get pointers to interesting procs */

	mpid = 0;
	active_procs = 0;
	total_virtual_size = 0;
	total_private_size = 0;
	total_aliased_size = 0;

	prefp = pref;
	for (kpb = kbase, pp = proc, i = 0;
				i < total_procs;
				kpb++, pp++, i++) {

	        /* place pointers to each valid proc structure in pref[] */
	        get_proc_info(kpb, pp);

		if (kpb->kp_proc.p_stat != 0) {
		        *prefp++ = pp;
			active_procs++;
			if (pp->pid > mpid)
			        mpid = pp->pid;
			
			if ((unsigned int)pp->state > (unsigned int)STATE_MAX)
			        pp->state = STATE_MAX;
			state_breakdown[pp->state]++;
			total_virtual_size += pp->virtual_size;
			total_private_size += pp->private;
			total_aliased_size += pp->aliased;
		}
		else
		        state_breakdown[0]++;
	}
	pmem_shared_resident(&total_shared_size, &total_shared_objects);

	/* display the load averages */
	move(0,0);

	sprintf(tbuf, "Load Averages");
	clen = strlen(tbuf);

	for (i = 0; i < 3; i++) {
		sprintf(&tbuf[clen], "%s %4.2f", i == 0 ? ": " : ",",
			(double)avenrun[i] / LOAD_SCALE);
		clen = clen + strlen(&tbuf[clen]);
	}

	/*
	 *  Display the current time.
	 *  "ctime" always returns a string that looks like this:
	 *  
	 *	Sun Sep 16 01:03:52 1973
	 *      012345678901234567890123
	 *	          1         2
	 *
	 *  We want indices 11 thru 18 (length 8).
	 */
	curr_time = time((long *)0);

	if (start_time == 0)
	        start_time = curr_time;

	memset(&tbuf[clen], ' ', 103 - clen);

	if (wide_output || events_accumulate)
	        clen = 102 - 8;
	else
	        clen = 78 - 8;

	sprintf(&tbuf[clen], "%-8.8s", &(ctime(&curr_time)[11]));
	clen = clen + strlen(&tbuf[clen]);

	if (events_accumulate) {
	        int hours;
		int minutes;

	        elapsed_secs = curr_time - start_time;
		minutes = elapsed_secs / 60;
		hours = minutes / 60;

		sprintf(&tbuf[clen], "   %3ld:%02ld:%02ld\n", hours, minutes % 60, elapsed_secs % 60);
	} else {
		sprintf(&tbuf[clen], "\n");
	}
	        
	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	printw(tbuf);

	/* display process state breakdown */
	sprintf(tbuf, "Processes:  %d total", total_procs);
	clen = strlen(tbuf);


	for (i = 0; i <= STATE_MAX; i++) {
	    if (state_breakdown[i] != 0) {
		sprintf(&tbuf[clen], ", %d %s%s",
			state_breakdown[i],
			state_name[i],
			(i == 0 && state_breakdown[0] > 1) ? "s" : ""
		      );

		clen = clen + strlen(&tbuf[clen]);
	    }
	}
	sprintf(&tbuf[clen], "... %d threads\n", total_threads);

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	printw(tbuf);

	if (!events_only) {
	        sprintf(tbuf, "Resident:  %s private,", mem_to_string(total_private_size));
		clen = strlen(tbuf);
		sprintf(&tbuf[clen], " %s aliased,", mem_to_string(total_aliased_size));
		clen = clen + strlen(&tbuf[clen]);
		sprintf(&tbuf[clen], " %s shared", mem_to_string(total_shared_size));
		clen = clen + strlen(&tbuf[clen]);
		sprintf(&tbuf[clen], "     num shared: %d\n", total_shared_objects);

		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		printw(tbuf);

		/* display main memory statistics */
		{
		        vm_size_t	total_resident_size,
			                active_resident_size,
			                inactive_resident_size,
			                wire_resident_size,
			                free_size;

			active_resident_size = vm_stat.active_count * pagesize;
			inactive_resident_size = vm_stat.inactive_count * pagesize;
			wire_resident_size = vm_stat.wire_count * pagesize;
			total_resident_size = (vm_stat.active_count + vm_stat.inactive_count +
						vm_stat.wire_count) * pagesize;
			free_size = vm_stat.free_count   * pagesize;

			sprintf(tbuf, "Memory:  ");
			clen = strlen(tbuf);
			sprintf(&tbuf[clen], "%s wired, ", mem_to_string(wire_resident_size));
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "%s active, ", mem_to_string(active_resident_size));
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "%s inactive, ", mem_to_string(inactive_resident_size));
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "%s used, ", mem_to_string(total_resident_size));
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "%s free\n", mem_to_string(free_size));

			if (tbuf[COLS-2] != '\n') {
			        tbuf[COLS-1] = '\n';
				tbuf[COLS] = 0;
			}
			printw(tbuf);
		}
	} else {
	        int  i_io, o_io, i_kbytes, o_kbytes;

	        i_io = o_io = i_kbytes = o_kbytes = 0;

		if (events_delta) {
		        if (i_net.io_prev || o_net.io_prev) {
			        i_io = i_net.io - i_net.io_prev;
			        o_io = o_net.io - o_net.io_prev;
			        i_kbytes = i_net.kbytes - i_net.kbytes_prev;
			        o_kbytes = o_net.kbytes - o_net.kbytes_prev;
			}
		} else if (events_accumulate) {
		        if (i_net.io_prev || o_net.io_prev) {
			        i_net.io_accum += i_net.io - i_net.io_prev;
			        o_net.io_accum += o_net.io - o_net.io_prev;
			        i_net.kbytes_accum += i_net.kbytes - i_net.kbytes_prev;
			        o_net.kbytes_accum += o_net.kbytes - o_net.kbytes_prev;

				i_io = i_net.io_accum;
				o_io = o_net.io_accum;
				i_kbytes = i_net.kbytes_accum;
				o_kbytes = o_net.kbytes_accum;
			}
		} else {
		        i_io = i_net.io;
		        o_io = o_net.io;
		        i_kbytes = i_net.kbytes;
		        o_kbytes = o_net.kbytes;
		}
	        sprintf(tbuf, "en0:   %10d ipkts/%dK", i_io, i_kbytes);
		clen = strlen(tbuf);
		memset(&tbuf[clen], ' ', 36 - clen);
		sprintf(&tbuf[36], "%10d opkts /%dK\n", o_io, o_kbytes);
		
		i_net.io_prev = i_net.io;
		o_net.io_prev = o_net.io;
		i_net.kbytes_prev = i_net.kbytes;
		o_net.kbytes_prev = o_net.kbytes;

		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		printw(tbuf);


	        i_io = o_io = i_kbytes = o_kbytes = 0;

		if (events_delta) {
		        if (i_dsk.io_prev || o_dsk.io_prev) {
			        i_io = i_dsk.io - i_dsk.io_prev;
			        o_io = o_dsk.io - o_dsk.io_prev;
			        i_kbytes = i_dsk.kbytes - i_dsk.kbytes_prev;
			        o_kbytes = o_dsk.kbytes - o_dsk.kbytes_prev;
			}
		} else if (events_accumulate) {
		        if (i_dsk.io_prev || o_dsk.io_prev) {
			        i_dsk.io_accum += i_dsk.io - i_dsk.io_prev;
			        o_dsk.io_accum += o_dsk.io - o_dsk.io_prev;
			        i_dsk.kbytes_accum += i_dsk.kbytes - i_dsk.kbytes_prev;
			        o_dsk.kbytes_accum += o_dsk.kbytes - o_dsk.kbytes_prev;

				i_io = i_dsk.io_accum;
				o_io = o_dsk.io_accum;
				i_kbytes = i_dsk.kbytes_accum;
				o_kbytes = o_dsk.kbytes_accum;
			}
		} else {
		        i_io = i_dsk.io;
		        o_io = o_dsk.io;
		        i_kbytes = i_dsk.kbytes;
		        o_kbytes = o_dsk.kbytes;
		}
	        sprintf(tbuf, "Disks: %10d reads/%dK", i_io, i_kbytes);
		clen = strlen(tbuf);
		memset(&tbuf[clen], ' ', 36 - clen);
		sprintf(&tbuf[36], "%10d writes/%dK\n", o_io, o_kbytes);

		i_dsk.io_prev = i_dsk.io;
		o_dsk.io_prev = o_dsk.io;
		i_dsk.kbytes_prev = i_dsk.kbytes;
		o_dsk.kbytes_prev = o_dsk.kbytes;

		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		printw(tbuf);
	}

	/* display paging statistics */
	if (events_only) {
	        int pageins, pageouts;
		
		pageins = pageouts = 0;

		if (events_delta) {
		        if (i_vm.io_prev || o_vm.io_prev) {
			        pageins = vm_stat.pageins - i_vm.io_prev;
				pageouts = vm_stat.pageouts - o_vm.io_prev;
			}
		} else if (events_accumulate) {
		        if (i_vm.io_prev || o_vm.io_prev) {
			        i_vm.io_accum += vm_stat.pageins - i_vm.io_prev;
			        o_vm.io_accum += vm_stat.pageouts - o_vm.io_prev;
				
				pageins = i_vm.io_accum;
				pageouts = o_vm.io_accum;
			}
		} else {
		        pageins = vm_stat.pageins;
		        pageouts = vm_stat.pageouts;
		}
	        sprintf(tbuf, "VM:    %10d pageins", pageins);
		clen = strlen(tbuf);
		memset(&tbuf[clen], ' ', 36 - clen);
		sprintf(&tbuf[36], "%10d pageouts\n", pageouts);
	} else {
	        sprintf(tbuf, "VM: %5.5s mapped, ", mem_to_string(total_virtual_size));
		clen = strlen(tbuf);
		sprintf(&tbuf[clen], "%d(%d) pageins, ", vm_stat.pageins, vm_stat.pageins - i_vm.io_prev);
		clen = clen + strlen(&tbuf[clen]);
		sprintf(&tbuf[clen], "%d(%d) pageouts\n", vm_stat.pageouts, vm_stat.pageouts - o_vm.io_prev);
	}
	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	printw(tbuf);

	i_vm.io_prev = vm_stat.pageins;
	o_vm.io_prev = vm_stat.pageouts;

	
	/* display the processes */
	if (topn > 0) {
	    if (events_delta)
	            sprintf(tbuf, "\n  PID COMMAND     %%%%CPU   TIME   FAULTS PGINS/COWS MSENT/MRCVD  BSD/MACH CSW\n");
	    else if (events_only)
	            sprintf(tbuf, "\n  PID COMMAND     %%%%CPU   TIME    FAULTS   PAGEINS  COW_FAULTS MSGS_SENT  MSGS_RCVD  BSDSYSCALL MACHSYSCALL CSWITCH\n");
	    else if (wide_output)
	            sprintf(tbuf, "\n  PID COMMAND     %%%%CPU   TIME   #TH #PRTS(delta) #MREGS VPRVT  RPRVT RALIAS  RSHRD  RSIZE(delta)  VSIZE(delta)\n");
	    else
	            sprintf(tbuf, "\n  PID COMMAND     %%%%CPU   TIME   #TH #PRTS #MREGS RPRVT  RSHRD  RSIZE  VSIZE\n");


	    if (tbuf[COLS] != '\n') {
	            tbuf[COLS+1] = '\n';
		    tbuf[COLS+2] = 0;
	    }
	    printw(tbuf);

	    update_histdata();

	    /* sort */
	    qsort((char *)pref,
		  active_procs,
		  sizeof(struct proc_info *),
		  proc_compar);
    
	    /* now, show the top whatever */
	    if (active_procs > topn)
	    {
		/* adjust for too many processes */
		active_procs = topn;
	    }

	    for (prefp = pref, i = 0; i < active_procs; prefp++, i++)
	    {
		pp = *prefp;

		sprintf(tbuf, "%5d", pp->pid);	                /* pid */
		clen = strlen(tbuf);
		sprintf(&tbuf[clen], " %-10.10s", pp->command); /* command */
		clen = clen + strlen(&tbuf[clen]);

		print_usage(&tbuf[clen], pp->cpu_usage);
		clen = clen + strlen(&tbuf[clen]);

		sprintf(&tbuf[clen], " ");
		clen++;

		print_time(&tbuf[clen], pp->total_time);	/* cputime */
		clen = clen + strlen(&tbuf[clen]);


		if (events_only) {
		    if (events_delta) {
			sprintf(&tbuf[clen], " %6d", pp->deltatei.faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %5d", pp->deltatei.pageins);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "/%-4d", pp->deltatei.cow_faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %5d", pp->deltatei.messages_sent);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "/%-5d", pp->deltatei.messages_received);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %4d", pp->deltatei.syscalls_unix);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "/%-4d", pp->deltatei.syscalls_mach);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], "%4d", pp->deltatei.csw);
			clen = clen + strlen(&tbuf[clen]);
		    } else if (events_accumulate) {
			sprintf(&tbuf[clen], "  %-8d", pp->deltatei.faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-8d", pp->deltatei.pageins);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->deltatei.cow_faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->deltatei.messages_sent);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->deltatei.messages_received);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->deltatei.syscalls_unix);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-11d", pp->deltatei.syscalls_mach);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-8d", pp->deltatei.csw);
			clen = clen + strlen(&tbuf[clen]);
		    } else {
			sprintf(&tbuf[clen], "  %-8d", pp->tei.faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-8d", pp->tei.pageins);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->tei.cow_faults);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->tei.messages_sent);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->tei.messages_received);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-10d", pp->tei.syscalls_unix);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-11d", pp->tei.syscalls_mach);
			clen = clen + strlen(&tbuf[clen]);
			sprintf(&tbuf[clen], " %-8d", pp->tei.csw);
			clen = clen + strlen(&tbuf[clen]);
		    }
		} else {

		sprintf(&tbuf[clen], " %3d", pp->num_threads);	/* # of threads */
		clen = clen + strlen(&tbuf[clen]);
		sprintf(&tbuf[clen], " %5d", pp->num_ports);	/* # of ports */
		clen = clen + strlen(&tbuf[clen]);
	
		if (wide_output) {
		        if (pp->dnum_ports)
			        sprintf(&tbuf[clen], "(%5d)", pp->dnum_ports);
			else
			        sprintf(&tbuf[clen], "       ");
			clen = clen + strlen(&tbuf[clen]);
		}
		if (pp->pid || do_proc0_vm)
		        sprintf(&tbuf[clen], "  %4d", pp->obj_count);
		else
		        sprintf(&tbuf[clen], "     -");
		clen = clen + strlen(&tbuf[clen]);

		if (wide_output) {
		        if (pp->pid || do_proc0_vm) {
			        sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->vprivate));	/* res size */
				clen = clen + strlen(&tbuf[clen]);
			        sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->private));	/* res size */
				clen = clen + strlen(&tbuf[clen]);
				sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->aliased));	/* res size */
			} else
			        sprintf(&tbuf[clen], "      -      -      -");
		} else {
		        if (pp->pid || do_proc0_vm)
			        sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->private+pp->aliased));/* res size */
			else
			        sprintf(&tbuf[clen], "      -");
		}
		clen = clen + strlen(&tbuf[clen]);

	        if (pp->pid || do_proc0_vm)
		        sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->shared));
		else
		        sprintf(&tbuf[clen], "      -");

		clen = clen + strlen(&tbuf[clen]);

		sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->resident_size));	/* res size */
		clen = clen + strlen(&tbuf[clen]);

		if (wide_output) {
		        if (pp->drsize)
			        sprintf(&tbuf[clen], "(%5.5s)", offset_to_string(pp->drsize));
			else
			        sprintf(&tbuf[clen], "       ");
			clen = clen + strlen(&tbuf[clen]);
		}
		sprintf(&tbuf[clen], "  %5.5s", mem_to_string(pp->virtual_size));	/* size */
		clen = clen + strlen(&tbuf[clen]);

		if (wide_output) {
		        if (pp->dvsize)
			        sprintf(&tbuf[clen], "(%5.5s)", offset_to_string(pp->dvsize));
			else
			        sprintf(&tbuf[clen], "       ");
			clen = clen + strlen(&tbuf[clen]);
		}
		} /* else not events only */
		sprintf(&tbuf[clen], "\n");

		if (tbuf[COLS-1] != '\n') {
		        tbuf[COLS] = '\n';
			tbuf[COLS+1] = 0;
		}
		printw(tbuf);
	    }

	    for (n = 0, prefp = pref; n < total_procs && i < topn; prefp++, n++)
	    {
	            pp = *prefp;

		    if (pp->has_idle_thread == TRUE) {
		            sprintf(tbuf, "%5d", pp->pid);
			    clen = strlen(tbuf);
			    sprintf(&tbuf[clen], " %-10.10s", "idle_thread");
			    clen = clen + strlen(&tbuf[clen]);

			    print_usage(&tbuf[clen], pp->cpu_idle);
			    clen = clen + strlen(&tbuf[clen]);
			    sprintf(&tbuf[clen], " ");
			    clen++;
			    print_time(&tbuf[clen], pp->idle_time);
			    clen = clen + strlen(&tbuf[clen]);
			    sprintf(&tbuf[clen], "\n");

			    if (tbuf[COLS-1] != '\n') {
			            tbuf[COLS] = '\n';
				    tbuf[COLS+1] = 0;
			    }
			    printw(tbuf);
			    
			    i++;
		    }
	    }             
        }
	refresh();
}


void
update_eventsdata()
{
  /*  unimplemented */
}


static struct nlist nl_net[] = {
#define N_IFNET         0
  { "_ifnet" },
  { "" },
};



/*
 * Read kernel memory, return 0 on success.
 */
int
kread(addr, buf, size)
        u_long addr;
	char *buf;
	int size;
{
        static kvm_t *kvmd = 0;

        if (kvmd == 0) {
	        /*
		 * XXX.
		 */
	        kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, buf);
		if (kvmd != NULL) {
		        if (kvm_nlist(kvmd, nl_net) < 0)
			        errx(1, "kvm_nlist: %s", kvm_geterr(kvmd));

			if (nl_net[0].n_type == 0)
			        errx(1, "no namelist");
		} else {
			return(-1);
		}
	}
	if (!buf)
	        return (0);
	if (kvm_read(kvmd, addr, buf, size) != size) {
	        warnx("%s", kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}


getNETWORKcounters()
{
        struct ifnet ifnet;
	struct ifnethead ifnethead;
	u_long off;

	if (nl_net[N_IFNET].n_value == 0)
	       return;
	if (kread(nl_net[N_IFNET].n_value, (char *)&ifnethead, sizeof ifnethead))
	       return;

	for (off = (u_long)ifnethead.tqh_first; off; ) {
                  char name[16], tname[16];

		  if (kread(off, (char *)&ifnet, sizeof ifnet))
		          break;
		  if (kread((u_long)ifnet.if_name, tname, 16))
		          break;
                  tname[15] = '\0';

		  snprintf(name, 16, "%s%d", tname, ifnet.if_unit);

		  if (strcmp(name, "en0") == 0) {
		          i_net.io = ifnet.if_ipackets;
			  o_net.io = ifnet.if_opackets;
			  
			  i_net.kbytes = ifnet.if_ibytes/1024;
			  o_net.kbytes = ifnet.if_obytes/1024;

			  return;
		  }
		  off = (u_long) ifnet.if_link.tqe_next;
	}
	return;
}


getDISKcounters()
{
	io_registry_entry_t drive      = 0;  /* needs release */
	UInt64         totalReadBytes  = 0;
	UInt64         totalReadCount  = 0;
        UInt64         totalWriteBytes = 0;
	UInt64         totalWriteCount = 0;

	kern_return_t status = 0;

	while ( (drive = IOIteratorNext(drivelist)) )
	{
	        CFNumberRef number          = 0;  /* don't release */
		CFDictionaryRef properties  = 0;  /* needs release */
		CFDictionaryRef statistics  = 0;  /* don't release */
		UInt64 value                = 0;

		/* Obtain the properties for this drive object */

		status = IORegistryEntryCreateCFProperties (drive,
                                                            (CFTypeRef *) &properties,
	                                                    kCFAllocatorDefault,
	                                                    kNilOptions);

		/* Obtain the statistics from the drive properties */
	        statistics = (CFDictionaryRef) CFDictionaryGetValue(properties, CFSTR(kIODriveStatistics));

		if (statistics) {
			/* Obtain the number of bytes read from the drive statistics */
			number = (CFNumberRef) CFDictionaryGetValue (statistics,
								     CFSTR(kIODriveStatisticsBytesRead));
			if (number) {
				status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				totalReadBytes += value;
			}
			/* Obtain the number of reads from the drive statistics */
			number = (CFNumberRef) CFDictionaryGetValue (statistics,
								     CFSTR(kIODriveStatisticsReads));
			if (number) {
				status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				totalReadCount += value;
			}

			/* Obtain the number of writes from the drive statistics */
			number = (CFNumberRef) CFDictionaryGetValue (statistics,
								     CFSTR(kIODriveStatisticsWrites));
			if (number) {
				status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				totalWriteCount += value;
			}
			/* Obtain the number of bytes written from the drive statistics */
			number = (CFNumberRef) CFDictionaryGetValue (statistics,
								     CFSTR(kIODriveStatisticsBytesWritten));
			if (number) {
				status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				totalWriteBytes += value;
			}
		}
		/* Release resources */

		CFRelease(properties); properties = 0;
		IOObjectRelease(drive); drive = 0;
	}
	IOIteratorReset(drivelist);
	  
	i_dsk.io = (int)totalReadCount;
	o_dsk.io = (int)totalWriteCount; 
	i_dsk.kbytes = (int)(totalReadBytes / 1024);
	o_dsk.kbytes = (int)(totalWriteBytes / 1024);
}

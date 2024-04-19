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
#include <mach/mach_types.h>
#include <kern/queue.h>
#include <kern/ast.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/spl.h>
#include <kern/lock.h>
#include <vm/vm_map.h>
#include <vm/pmap.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_object.h>

#undef clear_wait
#undef act_deallocate
#undef act_reference
#undef thread_should_halt
#undef ipc_port_release
#undef thread_ast_set

decl_simple_lock_data(extern,reaper_lock)
extern queue_head_t           reaper_queue;

/* BSD KERN COMPONENT INTERFACE */

thread_act_t get_firstthread(task_t);
vm_map_t  get_task_map(task_t);
ipc_space_t  get_task_ipcspace(task_t);
boolean_t is_kerneltask(task_t);
boolean_t is_thread_idle(thread_t);
boolean_t is_thread_running(thread_t);
thread_shuttle_t getshuttle_thread( thread_act_t);
thread_act_t getact_thread( thread_shuttle_t);
vm_offset_t get_map_min( vm_map_t);
vm_offset_t get_map_max( vm_map_t);
void act_reference( thread_act_t);
void act_deallocate( thread_act_t);
int get_task_userstop(task_t);
int get_thread_userstop(thread_act_t);
int inc_task_userstop(task_t);
boolean_t thread_should_abort(thread_shuttle_t);
void task_act_iterate_wth_args(task_t, void(*)(thread_act_t, void *), void *);
void ipc_port_release(ipc_port_t);
void thread_ast_set(thread_act_t, ast_t);
boolean_t is_thread_active(thread_t);
event_t get_thread_waitevent(thread_t);
kern_return_t get_thread_waitresult(thread_t);
vm_size_t get_vmmap_size(vm_map_t);
int get_vmmap_entries(vm_map_t);
int  get_task_numacts(task_t);



/*
 *
 */
void  *get_bsdtask_info(task_t t)
{
	return(t->bsd_info);
}

/*
 *
 */
void set_bsdtask_info(task_t t,void * v)
{
	t->bsd_info=v;
}

/*
 *
 */
void *get_bsdthread_info(thread_act_t th)
{
	return(&th->bsd_space[0]);
}

/*
 *
 */
thread_act_t get_firstthread(task_t task)
{
	return((thread_act_t)queue_first(&task->thr_acts));
}

/*
 *
 */
vm_map_t  get_task_map(task_t t)
{
	return(t->map);
}

/*
 *
 */
ipc_space_t  get_task_ipcspace(task_t t)
{
	return(t->itk_space);
}

int  get_task_numacts(task_t t)
{
	return(t->thr_act_count);
}

/*
 *
 */
void set_task_map(task_t t,vm_map_t map)
{
	t->map = map;
}

/*
 *
 */
void set_act_map(thread_act_t t,vm_map_t map)
{
	t->map = map;
}

/*
 *
 */
pmap_t  get_task_pmap(task_t t)
{
	return(t->map->pmap);
}

/*
 *
 */
pmap_t  get_map_pmap(vm_map_t map)
{
	return(map->pmap);
}
/*
 *
 */
task_t	get_threadtask(thread_act_t th)
{
	return(th->task);
}


/*
 *
 */
boolean_t is_thread_idle(thread_t th)
{
	return(th->state & TH_IDLE == TH_IDLE);
}

/*
 *
 */
boolean_t is_thread_running(thread_t th)
{
	return(th->state & TH_RUN == TH_RUN);
}

/*
 *
 */
thread_shuttle_t
getshuttle_thread(
	thread_act_t	th)
{
#ifdef	DEBUG
	assert(th->thread);
#endif
	return(th->thread);
}

/*
 *
 */
thread_act_t
getact_thread(
	thread_shuttle_t	th)
{
#ifdef	DEBUG
	assert(th->top_act);
#endif
	return(th->top_act);
}

/*
 *
 */
vm_offset_t
get_map_min(
	vm_map_t	map)
{
	return(vm_map_min(map));
}

/*
 *
 */
vm_offset_t
get_map_max(
	vm_map_t	map)
{
	return(vm_map_max(map));
}
vm_size_t
get_vmmap_size(
	vm_map_t	map)
{
	return(map->size);
}
int
get_vmmap_entries(
	vm_map_t	map)
{
	return(map->hdr.nentries);
}

/*
 *
 */
void
act_reference(
	thread_act_t thr_act)
{
	if (thr_act) {
		act_lock(thr_act);
		assert((thr_act)->ref_count < ACT_MAX_REFERENCES);
		if ((thr_act)->ref_count <= 0)
			panic("act_reference: already freed");
		(thr_act)->ref_count++;
		act_unlock(thr_act);
	}
}

/*
 *
 */
void
act_deallocate(
	thread_act_t thr_act) 
{
	if (thr_act) {
		int new_value;
		act_lock(thr_act);
		assert((thr_act)->ref_count > 0 &&
		(thr_act)->ref_count <= ACT_MAX_REFERENCES);
		new_value = --(thr_act)->ref_count;
		if (new_value == 0)
			{ act_free(thr_act); }
		else 
			act_unlock(thr_act);
	} 
}

/*
 *
 */
int
get_task_userstop(
	task_t task)
{
	return(task->user_stop_count);
}

/*
 *
 */
int
get_thread_userstop(
	thread_act_t th)
{
	return(th->user_stop_count);
}

/*
 *
 */
int
inc_task_userstop(
	task_t	task)
{
	int i=0;
	i = task->user_stop_count;
	task->user_stop_count++;
	return(i);
}


/*
 *
 */
boolean_t
thread_should_abort(
	thread_shuttle_t th)
{
	return( (!th->top_act || !th->top_act->active || 
        th->state & TH_ABORT)); 
}
/*
 *
 */
void
task_act_iterate_wth_args(
	task_t task,
	void (*func_callback)(thread_act_t, void *),
	void *func_arg)
{
        thread_act_t inc, ninc;

        for (inc  = (thread_act_t)queue_first(&task->thr_acts);
             inc != (thread_act_t)&task->thr_acts;
             inc  = ninc) {
                ninc = (thread_act_t)queue_next(&inc->thr_acts);
                (void) (*func_callback)(inc, func_arg);
        }

}

void
ipc_port_release(
	ipc_port_t port)
{
	ipc_object_release(&(port)->ip_object);
}

void
thread_ast_set(
	thread_act_t act, 
	ast_t reason) 
{
          act->ast |= reason;
}

boolean_t
is_thread_active(
	thread_shuttle_t th)
{
	return(th->active);
}

event_t
get_thread_waitevent(
	thread_shuttle_t th)
{
	return(th->wait_event);
}

kern_return_t
get_thread_waitresult(
	thread_shuttle_t th)
{
	return(th->wait_result);
}

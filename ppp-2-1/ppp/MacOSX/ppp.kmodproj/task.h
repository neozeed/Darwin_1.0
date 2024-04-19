/*
 *	File:	thread.h
 *	Author:	A.Ramesh.
 *
 *	This file contains the structure definitions for BSD only.
 *
 */

#ifndef	_KERN_TASK_H_
#define _KERN_TASK_H_


#include <mach/boolean.h>
#include <mach/time_value.h>
#include <mach/mach_param.h>
#include <mach/task_info.h>
// Start APPLE
//#include <kern/queue.h>
#include "queue.h"
// End APPLE
#include <mach/mach_types.h>
#include <mach/kern_return.h>
// Start APPLE
//#include <vm/vm_map.h>
#include "vm_map.h"
// End APPLE

/*
 * JMM - These come out of mach_types.h now
 *       (more to come)
 *
 * struct task;
 * typedef struct task *task_t;
 * struct processor ;
 * typedef struct processor * processor_t;
 */

extern task_t kernel_task;
extern processor_t	master_processor;

#define TASK_NULL	((task_t) 0)

#define current_task() get_current_task()
extern task_t get_current_task();

extern void * get_bsdtask_info(task_t);
extern set_bsdtask_info(task_t,void *);
extern vm_map_t get_task_map(task_t);
extern set_task_map(task_t, vm_map_t);
extern pmap_t get_task_pmap(task_t);
extern task_lock(task_t);
extern task_unlock(task_t);
extern boolean_t is_kerneltask(task_t);

/*
 *	Exported routines/macros
 */


/*
 *	Internal only routines
 */




#endif	/* _KERN_TASK_H_ */

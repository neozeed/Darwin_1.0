#ifndef _PPC_MACHINE_CPU_H_
#define _PPC_MACHINE_CPU_H_

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <kern/kern_types.h>
#include <pexpert/pexpert.h>

void	cpu_machine_init(
	void);

kern_return_t cpu_register(
        int *);

kern_return_t cpu_start(
        int);

#endif /* _PPC_MACHINE_CPU_H_ */

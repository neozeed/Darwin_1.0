#ifndef	_VM_VM_MAP_H_
#define _VM_VM_MAP_H_

#include <mach/mach_types.h>

struct pmap ;
typedef struct pmap * pmap_t;

/*
 * JMM - Now comes from mach_types.h
 *       (more to come)
 * struct vm_map ;
 * typedef struct vm_map * vm_map_t;
 * struct vm_map_copy;
 * typedef struct vm_map_copy *vm_map_copy_t;
 */

struct vm_map_entry;
typedef struct vm_map_entry	*vm_map_entry_t;

extern vm_map_t kernel_map;

#define	vm_map_copyin(src_map, src_addr, len, src_destroy, copy_result) \
		vm_map_copyin_common(src_map, src_addr, len, src_destroy, \
					FALSE, copy_result, FALSE)

#endif	/* _VM_VM_MAP_H_ */

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
 *
 *	File: vm/vm_shared_memory_server.c
 *	Author: Chris Youngworth
 *
 *      Support routines for an in-kernel shared memory allocator
 */

#include <ipc/ipc_port.h>
#include <kern/thread.h>
#include <mach/shared_memory_server.h>
#include <kern/zalloc.h>
#include <mach/kern_return.h>
#include <mach/vm_inherit.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>


zone_t          lsf_zone;
vm_offset_t     alternate_load_base;
vm_offset_t     alternate_load_next;

/* called at boot time, allocates two regions, each 256 megs in size */
/* these regions are later mapped into task spaces, allowing them to */
/* share the contents of the regions.  shared_file_init is part of   */
/* a shared_memory_server which not only allocates the backing maps  */
/* but also coordinates requests for space.  */


kern_return_t
shared_file_init(
	ipc_port_t	*shared_text_region_handle,
	vm_size_t 	text_region_size, 
	ipc_port_t	*shared_data_region_handle,
	vm_size_t 	data_region_size,
	vm_offset_t	*shared_file_mapping_array)
{
	vm_offset_t		aligned_address;
	shared_file_info_t	*sf_head;
	vm_offset_t		table_mapping_address;
	ipc_port_t		sfma_handle;
	int			data_table_size;
	int			hash_size;
	int			i;
	kern_return_t		kret;

	vm_object_t		buf_object;
	vm_map_entry_t		entry;
	vm_size_t		alloced;
	vm_offset_t		b;
	vm_page_t		p;

	*shared_file_mapping_array = 0;

	/* create text and data maps/regions */
	if(kret = vm_region_object_create(kernel_map, 
				text_region_size, 
				shared_text_region_handle)) {
		
		return kret;
	}
	if(kret = vm_region_object_create(kernel_map, 
				data_region_size, 
				shared_data_region_handle)) {
		ipc_port_release_send(*shared_text_region_handle);
		return kret;
	}

	data_table_size = data_region_size >> 9;
	hash_size = data_region_size >> 14;

	buf_object = vm_object_allocate(data_table_size);

	if(vm_map_find_space(kernel_map, shared_file_mapping_array, 
			data_table_size, 0, &entry) != KERN_SUCCESS) {

		panic("shared_file_init: no space");
	}
	vm_map_unlock(kernel_map);
	entry->object.vm_object = buf_object;
	entry->offset = 0;

	for (b = *shared_file_mapping_array, alloced = 0; 
			alloced < round_page(sizeof(struct sf_mapping)); 
			alloced += PAGE_SIZE,  b += PAGE_SIZE) {
		vm_object_lock(buf_object);
		p = vm_page_alloc(buf_object, alloced);
		if (p == VM_PAGE_NULL) {
			panic("shared_file_init: no space");
		} 	
		p->busy = FALSE;
		vm_object_unlock(buf_object);
		pmap_enter(kernel_pmap, b, p->phys_addr,
			VM_PROT_READ | VM_PROT_WRITE, TRUE);
	}


	/* initialize loaded file array */
	sf_head = (shared_file_info_t *)*shared_file_mapping_array;
	sf_head->hash = (queue_head_t *) 
			(((int)*shared_file_mapping_array) + 
				sizeof(struct shared_file_info));
	sf_head->hash_size = hash_size/sizeof(queue_head_t);
	mutex_init(&(sf_head->lock), (ETAP_VM_MAP));
	sf_head->hash_init = FALSE;

	table_mapping_address = data_region_size - data_table_size;

	mach_make_memory_entry(kernel_map, &data_table_size, 
			*shared_file_mapping_array, VM_PROT_READ, &sfma_handle,
			NULL);

	vm_map(((vm_named_entry_t)
			(*shared_data_region_handle)->ip_kobject)->backing.map,
			&table_mapping_address,
			data_table_size, 0, SHARED_LIB_ALIAS, 
			sfma_handle, 0, FALSE, 
			VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);

	ipc_port_release_send(sfma_handle);

	lsf_zone = zinit(sizeof(struct load_file_ele), 
		data_table_size - hash_size,
		0, "load_file_server"); 

	zone_change(lsf_zone, Z_EXHAUST, TRUE);
	zone_change(lsf_zone, Z_COLLECT, FALSE);
	zone_change(lsf_zone, Z_EXPAND, FALSE);
	zone_change(lsf_zone, Z_FOREIGN, TRUE);
	
	alternate_load_base = text_region_size>>2;
	alternate_load_next = text_region_size>>2;
}

/* A call made from user space, copyin_shared_file requires the user to */
/* provide the address and size of a mapped file, the full path name of */
/* that file and a list of offsets to be mapped into shared memory.     */
/* By requiring that the file be pre-mapped, copyin_shared_file can     */
/* guarantee that the file is neither deleted nor changed after the user */
/* begins the call.  */

kern_return_t
copyin_shared_file(
	vm_offset_t	mapped_file,
	vm_size_t	mapped_file_size,
	vm_offset_t	*base_address, 
	int 		map_cnt,
	sf_mapping_t	*mappings,
	vm_object_t	file_object,
	int		*flags)
{
	vm_map_entry_t		entry;
	shared_file_info_t	*shared_file_header;
	load_struct_t		*file_entry;
	sf_mapping_t		*file_mapping;
	boolean_t		alternate;
	int			i;
	kern_return_t		ret;
	

	shared_file_header = (shared_file_info_t *)shared_file_mapping_array;

	mutex_lock(&shared_file_header->lock);

	if(shared_file_header->hash_init == FALSE) {
		vm_size_t	hash_table_size;
		vm_size_t	hash_table_offset;
		
		hash_table_size = (shared_file_header->hash_size) 
						* sizeof(struct queue_entry);
		hash_table_offset = hash_table_size + 
					round_page(sizeof(struct sf_mapping));
		for (i = 0; i < shared_file_header->hash_size; i++)
            		queue_init(&shared_file_header->hash[i]);

		zcram(lsf_zone,  shared_file_mapping_array + 
				hash_table_offset,
				(hash_table_size<<5) - hash_table_offset);
		shared_file_header->hash_init = TRUE;
	}

	
	if(vm_map_lookup_entry(current_map(), mapped_file, &entry)) {
		vm_object_t	mapped_object;
		if(entry->is_sub_map) {
			mutex_unlock(&shared_file_header->lock);
			return KERN_FAILURE;
		}
		mapped_object = entry->object.vm_object;
		while(mapped_object->shadow != NULL) {
			mapped_object = mapped_object->shadow;
		}
		if(file_object != mapped_object) {
			mutex_unlock(&shared_file_header->lock);
			return KERN_FAILURE;
		}
	} else {
		mutex_unlock(&shared_file_header->lock);
		return KERN_FAILURE;
	}

	alternate = (*flags & ALTERNATE_LOAD_SITE) ? TRUE : FALSE;

	if (file_entry = lsf_hash_lookup(shared_file_header->hash, 
			(void *) file_object, 
			shared_file_header->hash_size, alternate)) {
		/* File is loaded, check the load manifest for exact match */
		/* we simplify by requiring that the elements be the same  */
		/* size and in the same order rather than checking for     */
		/* semantic equivalence. */

		/* If the file is being loaded in the alternate        */
		/* area, one load to alternate is allowed per mapped   */
		/* object the base address is passed back to the       */
		/* caller and the mappings field is filled in.  If the */
		/* caller does not pass the precise mappings_cnt       */
		/* and the Alternate is already loaded, an error       */
		/* is returned.  */
		i = 0;
		file_mapping = file_entry->mappings;
		while(file_mapping != NULL) {
			if(i>=map_cnt) {
				mutex_unlock(&shared_file_header->lock);
				return KERN_FAILURE;
			}
			if (mappings[i].mapping_offset != 
					file_mapping->mapping_offset ||
					mappings[i].size != 
						file_mapping->size ||	
					mappings[i].file_offset != 
						file_mapping->file_offset ||	
					mappings[i].protection != 
						file_mapping->protection) {
				break;
			}
			file_mapping = file_mapping->next;
			i++;
		}
		if(i!=map_cnt) {
			mutex_unlock(&shared_file_header->lock);
			return KERN_FAILURE;
		}
		*base_address = file_entry->base_address;
		*flags = SF_PREV_LOADED;
		mutex_unlock(&shared_file_header->lock);
		return KERN_SUCCESS;
	} else {
		/* File is not loaded, lets attempt to load it */
		ret = lsf_load(mapped_file, mapped_file_size, base_address,
					     mappings, map_cnt, 
					     (void *)file_object, *flags);
		*flags = 0;
		mutex_unlock(&shared_file_header->lock);
		return ret;
	}
}

/* A hash lookup function for the list of loaded files in      */
/* shared_memory_server space.  */

load_struct_t  *
lsf_hash_lookup(
	queue_head_t	*hash_table,
	void		*file_object,
	int		size,
	boolean_t	alternate)
{
	register queue_t	bucket;
	load_struct_t		*entry;
	
	bucket = &(hash_table[load_file_hash((int)file_object, size)]);
	for (entry = (load_struct_t *)queue_first(bucket);
		!queue_end(bucket, &entry->links);
		entry = (load_struct_t *)queue_next(&entry->links)) {
		if (entry->file_object == (int)file_object) {
			if(alternate) {
				if (entry->base_address >= alternate_load_base) 
					return entry;
			} else {
				if (entry->base_address < alternate_load_base) 
					return entry;
			}
		}
	}

	return (load_struct_t *)0;
}

/* Removes a map_list, (list of loaded extents) for a file from     */
/* the loaded file hash table.  */

load_struct_t *
lsf_hash_delete(
	void		*file_object)
{
	register queue_t	bucket;
	shared_file_info_t *shared_file_header;
	load_struct_t		*entry;
	load_struct_t		*prev_entry;

	shared_file_header = (shared_file_info_t *)shared_file_mapping_array;

	bucket = &shared_file_header->hash
	     [load_file_hash((int)file_object, shared_file_header->hash_size)];

	for (entry = (load_struct_t *)queue_first(bucket);
		!queue_end(bucket, &entry->links);
		entry = (load_struct_t *)queue_next(&entry->links)) {
		if (entry->file_object == (int) file_object) {
			queue_remove(bucket, entry, load_struct_ptr_t, links);
			return entry;
		}

	}

	return (load_struct_t *)0;
}

/* Inserts a new map_list, (list of loaded file extents), into the */
/* server loaded file hash table. */

void
lsf_hash_insert(
	load_struct_t	*entry)
{
	shared_file_info_t *shared_file_header;

	shared_file_header = (shared_file_info_t *)shared_file_mapping_array;
	queue_enter(&shared_file_header->hash
			[load_file_hash(entry->file_object, 
					shared_file_header->hash_size)],
			entry, load_struct_ptr_t, links);
}
	
/* Looks up the file type requested.  If already loaded and the */
/* file extents are an exact match, returns Success.  If not    */
/* loaded attempts to load the file extents at the given offsets */
/* if any extent fails to load or if the file was already loaded */
/* in a different configuration, lsf_load fails.                 */

kern_return_t
lsf_load(
	vm_offset_t	mapped_file,
	vm_size_t	mapped_file_size,
	vm_offset_t	*base_address, 
	sf_mapping_t	*mappings,
	int		map_cnt,
	void		*file_object,
	int		flags)
{

	load_struct_t	*entry;
	vm_map_copy_t	copy_object;
	sf_mapping_t	*file_mapping;
	sf_mapping_t	**tptr;
	int		i;
	ipc_port_t	local_map;

	entry = (load_struct_t *)zalloc(lsf_zone);
	entry->file_object = (int)file_object;
	entry->base_address = *base_address;
	entry->mapping_cnt = map_cnt;
	entry->mappings = NULL;
	entry->links.prev = (queue_entry_t) 0;
	entry->links.next = (queue_entry_t) 0;

	lsf_hash_insert(entry);
	tptr = &(entry->mappings);


	if (flags & ALTERNATE_LOAD_SITE) {
		int 	max_loadfile_offset;

		*base_address = alternate_load_next;
		max_loadfile_offset = 0;
		for(i = 0; i<map_cnt; i++) {
			if((mappings[i].mapping_offset + mappings[i].size) >
				max_loadfile_offset) {
				max_loadfile_offset = 
					mappings[i].mapping_offset 
							+ mappings[i].size;
			}
		}
		alternate_load_next += round_page(max_loadfile_offset);
	}
	/* copyin mapped file data */
	for(i = 0; i<map_cnt; i++) {
		vm_offset_t	target_address;

		if(mappings[i].protection & VM_PROT_COW) {
			local_map = shared_data_region_handle;
		} else {
			local_map = shared_text_region_handle;
		}
		if((mapped_file + mappings[i].file_offset + 
				mappings[i].size) > 
				(mapped_file + mapped_file_size)) {
			lsf_unload(file_object);
			return KERN_FAILURE;
		}
		target_address = *base_address + mappings[i].mapping_offset;
		if(vm_allocate(((vm_named_entry_t)local_map->ip_kobject)
				->backing.map, &target_address,
				mappings[i].size, FALSE)) {
			lsf_unload(file_object);
			return KERN_FAILURE;
		}
		if(vm_map_copyin(current_map(), 
			mapped_file + mappings[i].file_offset, 
			mappings[i].size, TRUE, &copy_object)) {
			lsf_unload(file_object);
			return KERN_FAILURE;
		}
		target_address = *base_address + mappings[i].mapping_offset;
		vm_map_copy_overwrite(((vm_named_entry_t)local_map->ip_kobject)
			->backing.map, target_address,
			copy_object, FALSE);
		vm_map_protect(((vm_named_entry_t)local_map->ip_kobject)
				->backing.map, target_address,
				target_address + mappings[i].size,
				mappings[i].protection & ~VM_PROT_COW, FALSE);
		file_mapping = (sf_mapping_t *)zalloc(lsf_zone);
		file_mapping->mapping_offset = mappings[i].mapping_offset;
		file_mapping->size = mappings[i].size;
		file_mapping->file_offset = mappings[i].file_offset;
		file_mapping->protection = mappings[i].protection;
		file_mapping->next == NULL;
		*tptr = file_mapping;
		tptr = &(file_mapping->next);
	}
	return KERN_SUCCESS;
			
}


/* finds the file_object extent list in the shared memory hash table       */
/* If one is found the associated extents in shared memory are deallocated */
/* and the extent list is freed */

void
lsf_unload(
	void	*file_object)
{
	load_struct_t	*entry;
	ipc_port_t	local_map;
	sf_mapping_t	*map_ele;
	sf_mapping_t	*back_ptr;

	entry = lsf_hash_delete(file_object);
	if(entry) {
		map_ele = entry->mappings;
		while(map_ele != NULL) {
			if(map_ele->protection & VM_PROT_COW) {
				local_map = shared_data_region_handle;
			} else {
				local_map = shared_text_region_handle;
			}
			vm_deallocate(((vm_named_entry_t)local_map->ip_kobject)
					->backing.map, entry->base_address + 
					map_ele->mapping_offset,
					map_ele->size);
			back_ptr = map_ele;
			map_ele = map_ele->next;
			zfree(lsf_zone, (vm_offset_t)back_ptr);
		}
		zfree(lsf_zone, (vm_offset_t)entry);
	}
}

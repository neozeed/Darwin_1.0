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
#include <pexpert/protos.h>
#include <pexpert/pexpert.h>
#include <pexpert/ppc/powermac.h>
#include <pexpert/ppc/powermac_pci.h>
#include <pexpert/device_tree.h>

/* Local declarations */
void		pe_identify_machine(void);
int		get_clock_frequency(void);
int		init_powermac_machine_info(void);

/* External declarations */

unsigned int LockTimeOut = 12500000;

/* pe_identify_machine:
 *
 *   Sets up platform parameters.
 *   Returns:    nothing
 */
void pe_identify_machine(void)
{
	unsigned int dec_rate;
	union {
	  unsigned long fixed;
	  unsigned char top;
	} tmp_fixed;

	/* Everything starts out zeroed... */
	bzero((void *)&powermac_info,         sizeof(powermac_info_t));

	powermac_info.struct_version	= POWERMAC_INFO_VERSION;

	/* 
	 * First, find out everything we can from the device tree.
	 * 
	 */

	powermac_info.bus_clock_rate_hz	    = get_clock_frequency();

	LockTimeOut = powermac_info.bus_clock_rate_hz >> 4;		/* XXX */

	powermac_info.proc_clock_to_nsec_numerator   = 100000 * 4;
	powermac_info.proc_clock_to_nsec_denominator = powermac_info.bus_clock_rate_hz / 10000;

	    dec_rate = powermac_info.bus_clock_rate_hz / 4;
	    // Do the lower 24 bits (ie the remainder * 1_Fixed / dec_rate)
	    tmp_fixed.fixed =
	      ((1000000000 % dec_rate) << 24) / dec_rate;

	    // Do the top 8 bits (ie the whole part of the number)
	    tmp_fixed.top = 1000000000 / dec_rate;

	powermac_info.dec_clock_period = tmp_fixed.fixed;

	init_powermac_machine_info();

	//powermac_info.io_coherent  = powermac_is_coherent();
	powermac_info.io_coherent  = 1;
}

int pe_map_physical_range( vm_offset_t phys, unsigned int size,
		int findSpace, int cache, vm_offset_t * virt)
{

	*virt = io_map( phys, size);

	return( 0);
}


/* get_io_base_addr():
 *
 *   Get the base address of the io controller.  
 */
vm_offset_t get_io_base_addr(void)
{
	DTEntry 	entryP;
	vm_offset_t 	*address;
	int		size;

	if( (DTFindEntry("device_type", "dbdma", &entryP) == kSuccess)
	 || (DTFindEntry("device_type", "mac-io", &entryP) == kSuccess))
	{
	    if (DTGetProperty(entryP, "AAPL,address", (void **)&address, &size) == kSuccess)
		    return(*address);

	    if (DTGetProperty(entryP, "assigned-addresses", (void **)&address, &size) == kSuccess)
		    // address calculation not correct
		    return(*(address+2));
	}
	
	panic("Uhmmm.. I can't get this machine's io base address\n");
	return( 0 );
}

boolean_t PE_init_ethernet_debugger( void )
{
	DTEntry 	entryP;
	vm_offset_t   * address;
	unsigned char * netAddr;
	int		size;
	vm_offset_t	io;
	boolean_t	result;

	if( (io = get_io_base_addr())
	 && (DTFindEntry("name", "mace", &entryP) == kSuccess)
	 && (DTGetProperty(entryP, "local-mac-address", (void **)&netAddr, &size) == kSuccess)
	 && (DTGetProperty(entryP, "reg", (void **)&address, &size) == kSuccess)
	 && (size == (2 * 3 * sizeof(vm_offset_t)) ))
        {
            extern boolean_t kdp_mace_init( void * baseAddresses[3],
						unsigned char * netAddr );
	    void * maceAddrs[3];

            // address calculation not correct
	    maceAddrs[0] = (void *) io_map(io + address[0], address[1]);
	    maceAddrs[1] = (void *) io_map(io + address[2], 0x1000);
	    maceAddrs[2] = (void *) (((vm_offset_t)maceAddrs[1])
					+ address[4] - address[2]);
            result = kdp_mace_init( maceAddrs, netAddr );

	} else
	     result = FALSE;
	
	return( result );
}

vm_offset_t PE_find_scc( void )
{
	vm_offset_t	io;
	DTEntry 	entryP;

        if( (io = get_io_base_addr())
        && (DTFindEntry("name", "escc", &entryP) == kSuccess))
            io += PCI_SCC_OFFSET_PHYS;
	else
            io = 0;

	return( io );
}

/* get_clock_frequency():
 *
 *   Get the clock freq
 */
int get_clock_frequency()
{
	DTEntry		entry;
	int		*freq;
	int		size;

	if (DTLookupEntry(0, "/", &entry) == kSuccess)
	    if (DTGetProperty(entry, "clock-frequency", (void **)&freq, &size) == kSuccess)
		    return(*freq);

	return(50000000);		// 50 MHz default
}

int
init_powermac_machine_info(void)
{
    DTEntry     entry;
	int     *value;
	int     size;

	value = (int *)0;
	size = 0;

	if (DTFindEntry("device_type", "cpu", &entry) == kSuccess) {
		if (DTGetProperty(entry, "d-cache-block-size",
			(void **)&value, &size) == kSuccess)
			powermac_info.dcache_block_size = *value;

		if (DTGetProperty(entry, "d-cache-size",
			(void **)&value, &size) == kSuccess)
			powermac_info.dcache_size = *value;

		if (DTGetProperty(entry, "i-cache-size",
			(void **)&value, &size) == kSuccess)
			powermac_info.icache_size = *value;

		if (DTGetProperty(entry, "cpu-version",
			(void **)&value, &size) == kSuccess)
			powermac_info.processor_version = *value;

		if (DTGetProperty(entry, "clock-frequency",
			(void **)&value, &size) == kSuccess)
			powermac_info.cpu_clock_rate_hz = *value;
		else powermac_info.cpu_clock_rate_hz = 300000000;

		if (DTGetProperty(entry, "timebase-frequency",
			(void **)&value, &size) == kSuccess)
			powermac_info.dec_clock_rate_hz = *value;
		else powermac_info.dec_clock_rate_hz = 25000000;

		if (DTGetProperty(entry, "bus-frequency",
			(void **)&value, &size) == kSuccess)
			powermac_info.bus_clock_rate_num = *value;
		else powermac_info.bus_clock_rate_num = get_clock_frequency();

	} else
		return 1;

	powermac_info.bus_clock_rate_den = 1;

	powermac_info.bus_to_dec_rate_num = 1;
	powermac_info.bus_to_dec_rate_den = powermac_info.bus_clock_rate_num /
	  powermac_info.dec_clock_rate_hz;

	powermac_info.bus_to_cpu_rate_num =
	  (2 * powermac_info.cpu_clock_rate_hz) /
	  powermac_info.bus_clock_rate_num;
	powermac_info.bus_to_cpu_rate_den = 2;

	if (DTFindEntry("device_type", "cache", &entry) == kSuccess) {
		if (DTGetProperty(entry, "cache-unified",
			(void **)&value, &size) == kSuccess)
			powermac_info.caches_unified = 1;
	} else
		return 1;

	return 0;
}

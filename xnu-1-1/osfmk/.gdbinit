#
# Kernel gdb macros
#
#  These gdb macros should be useful during kernel development in
#  determining what's going on in the kernel.
#
#  All the convenience variables used by these macros begin with $kgm_

set $kgm_vers = 2

echo Loading Kernel GDB Macros package.  Type "help kgm" for more info.\n

define kgm
printf "These are the kernel gdb macros version %d.  ", $kgm_vers
echo  Type "help kgm" for more info.\n
end

document kgm
| These are the kernel gdb macros.  These gdb macros are intended to be
| used when debugging a remote kernel via the kdp protocol.  Typically, you
| would connect to your remote target like so:
|     (gdb) target remote-kdp
|     (gdb) attach <name-of-remote-host>
|
| The following macros are available in this package:
|
|     showalltasks   Display a summary listing of tasks
|     showallacts    Display a summary listing of all activations
|     showallstacks  Display the kernel stacks for all activations
|     showallvm      Display a summary listing of all the vm maps
|     showallvme     Display a summary listing of all the vm map entries
|     showallipc     Display a summary listing of all the ipc spaces
|     showallrights  Display a summary listing of all the ipc rights
|
|     showtask       Display status of the specified task
|     showtaskacts   Display the status of all activations in the task
|     showtaskstacks Display all kernel stacks for all activations in the task
|     showtaskvm     Display status of the specified task's vm_map
|     showtaskvme    Display a summary list of the task's vm_map entries
|     showtaskipc    Display status of the specified task's ipc space
|     showtaskrights Display a summary list of the task's ipc space entries
|
|     showact	     Display status of the specified thread activation
|     showactstack   Display the kernel stack for the specified activation
|
|     showmap	     Display the status of the specified vm_map
|     showmapvme     Display a summary list of the specified vm_map's entries
|
|     showipc        Display the status of the specified ipc space
|     showrights     Display a summary list of all the rights in an ipc space
|
|     showpid        Display the status of the process identified by pid
|     showproc       Display the status of the process identified by a proc pointer
|
| Type "help <macro>" for more specific help on a particular macro.
| Type "show user <macro>" to see what the macro is really doing.
end


define showactheader
    printf "            activation  "
    printf "thread      pri  state  wait_queue  wait_event\n"
end


define showactint
   printf "            0x%08x  ", $arg0
   set $kgm_actp = *(Thread_Activation *)$arg0
   if $kgm_actp.thread
	set $kgm_thread = *$kgm_actp.thread
	printf "0x%08x  ", $kgm_actp.thread
	printf "%3d  ", $kgm_thread.sched_pri
	set $kgm_state = $kgm_thread.state
	if $kgm_state & 0x80
	    printf "I" 
	end
	if $kgm_state & 0x40
	    printf "P" 
	end
	if $kgm_state & 0x20
	    printf "A" 
	end
	if $kgm_state & 0x10
	    printf "H" 
	end
	if $kgm_state & 0x08
	    printf "U" 
	end
	if $kgm_state & 0x04
	    printf "R" 
	end
	if $kgm_state & 0x02
	    printf "S" 
	end
   	if $kgm_state & 0x01
	    printf "W\t" 
	    printf "0x%08x  ", $kgm_thread.wait_queue
	    output /a $kgm_thread.wait_event
	end
	if $arg1 != 0
	    if ($kgm_thread.kernel_stack != 0)
		set $mysp = $kgm_actp->mact.pcb.ss.r1
	    	while ($mysp != 0) && (($mysp & 0xf) == 0)
				printf "\n\t\t\t"
				output /a * ($mysp + 8)
				set $mysp = * $mysp
		end
	    else
		printf "\n\t\t\tcontinuation="
		output /a $kgm_thread.continuation
	    end
	    printf "\n"
	else
	    printf "\n"
	end
    end
end	    

define showact
    showactheader
    showactint $arg0 0
end
document showact
| Routine to print out the state of a specific thread activation.
| The following is the syntax:
|     (gdb) showact <activation> 
end


define showactstack
    showactheader
    showactint $arg0 1
end
document showactstack
| Routine to print out the stack of a specific thread activation.
| The following is the syntax:
|     (gdb) showactstack <activation> 
end


define showallacts
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
	showtaskint $kgm_taskp
	showactheader
	set $kgm_head_actp = &($kgm_taskp->thr_acts)
        set $kgm_actp = (Thread_Activation *)($kgm_taskp->thr_acts.next)
	while $kgm_actp != $kgm_head_actp
	    showactint $kgm_actp 0
  	    set $kgm_actp = (Thread_Activation *)($kgm_actp->thr_acts.next)
        end
	printf "\n"
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallacts
| Routine to print out a summary listing of all the thread activations.
| The following is the syntax:
|     (gdb) showallacts
end


define showallstacks
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
	showtaskint $kgm_taskp
	set $kgm_head_actp = &($kgm_taskp->thr_acts)
        set $kgm_actp = (Thread_Activation *)($kgm_taskp->thr_acts.next)
	while $kgm_actp != $kgm_head_actp
	    showactheader
	    showactint $kgm_actp 1
  	    set $kgm_actp = (Thread_Activation *)($kgm_actp->thr_acts.next)
        end
	printf "\n"
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallstacks
| Routine to print out a summary listing of all the thread kernel stacks.
| The following is the syntax:
|     (gdb) showallstacks
end




define showmapheader
    printf "vm_map      pmap        vm_size    "
    printf "#ents rpage  hint        first_free\n"
end

define showvmeheader
    printf "            entry       start       "
    printf "prot #page  object      offset\n"
end

define showvmint
    set $kgm_mapp = (vm_map_t)$arg0
    set $kgm_map = *$kgm_mapp
    printf "0x%08x  ", $arg0
    printf "0x%08x  ", $kgm_map.pmap
    printf "0x%08x  ", $kgm_map.size
    printf "%3d  ", $kgm_map.hdr.nentries
    printf "%5d  ", $kgm_map.pmap->stats.resident_count
    printf "0x%08x  ", $kgm_map.hint
    printf "0x%08x\n", $kgm_map.first_free
    if $arg1 != 0
	showvmeheader	
	set $kgm_head_vmep = &($kgm_mapp->hdr.links)
	set $kgm_vmep = $kgm_map.hdr.links.next
	while (($kgm_vmep != 0) && ($kgm_vmep != $kgm_head_vmep))
	    set $kgm_vme = *$kgm_vmep
	    printf "            0x%08x  ", $kgm_vmep
	    printf "0x%08x  ", $kgm_vme.links.start
	    printf "%1x", $kgm_vme.protection
	    printf "%1x", $kgm_vme.max_protection
	    if $kgm_vme.inheritance == 0x0
		printf "S"
	    end
	    if $kgm_vme.inheritance == 0x1
		printf "C"
	    end
	    if $kgm_vme.inheritance == 0x2
		printf "-"
	    end
	    if $kgm_vme.inheritance == 0x3
		printf "D"
	    end
	    if $kgm_vme.is_sub_map
		printf "s "
	    else
		if $kgm_vme.needs_copy
		    printf "n "
		else
		    printf "  "
		end
	    end
	    printf "%5d  ",($kgm_vme.links.end - $kgm_vme.links.start) >> 12
	    printf "0x%08x  ", $kgm_vme.object.vm_object
	    printf "0x%08x\n", $kgm_vme.offset
  	    set $kgm_vmep = $kgm_vme.links.next
        end
    end
    printf "\n"
end


define showmapvme
	showmapheader
	showvmint $arg0 1
end
document showmapvme
| Routine to print out a summary listing of all the entries in a vm_map
| The following is the syntax:
|     (gdb) showmapvme <vm_map>
end


define showmap
	showmapheader
	showvmint $arg0 0
end
document showmap
| Routine to print out a summary description of a vm_map
| The following is the syntax:
|     (gdb) showmap <vm_map>
end

define showallvm
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
	showmapheader
	showtaskint $kgm_taskp
	showvmint $kgm_taskp->map 0
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallvm
| Routine to print a summary listing of all the vm maps
| The following is the syntax:
|     (gdb) showallvm
end


define showallvme
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
	showmapheader
	showtaskint $kgm_taskp
	showvmint $kgm_taskp->map 1
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallvme
| Routine to print a summary listing of all the vm map entries
| The following is the syntax:
|     (gdb) showallvme
end


define showipcheader
    printf "ipc_space   is_table    table_next "
    printf "flags tsize  splaytree   splaybase\n"
end

define showipceheader
    printf "            entry       name        "
    printf "rite urefs  object      request\n"
end

define showipceint
    set $kgm_ie = *(ipc_entry_t)$arg0
    printf "            0x%08x  ", $arg0
    printf "0x%08x  ", $arg1
    if $kgm_ie.ie_bits & 0x00010000
	if $kgm_ie.ie_bits & 0x00020000
	    printf " SR"
	else
	    printf "  S"
	end
    else
	if $kgm_ie.ie_bits & 0x00020000
	    printf "  R"
	end
    end
    if $kgm_ie.ie_bits & 0x00040000
	printf "  O"
    end
    if $kgm_ie.ie_bits & 0x00080000
	printf "SET"
    end
    if $kgm_ie.ie_bits & 0x00100000
	printf "  D"
    end
    if $kgm_ie.ie_bits & 0x00800000
	printf "c "
    else
	printf "  "
    end
    printf "%5d  ", $kgm_ie.ie_bits & 0xffff
    printf "0x%08x  ", $kgm_ie.ie_object
    printf "0x%08x\n", $kgm_ie.index.request
end

define showipcint
    set $kgm_isp = (ipc_space_t)$arg0
    set $kgm_is = *$kgm_isp
    printf "0x%08x  ", $arg0
    printf "0x%08x  ", $kgm_is.is_table
    printf "0x%08x  ", $kgm_is.is_table_next
    if $kgm_is.is_growing != 0
	printf "G"
    else
	printf " "
    end
    if $kgm_is.is_fast != 0
	printf "F"
    else
	printf " "
    end
    if $kgm_is.is_active != 0
	printf "A  "
    else
	printf "   "
    end
    printf "%5d  ", $kgm_is.is_table_size
    printf "0x%08x  ", $kgm_is.is_tree_total
    printf "0x%08x\n", &$kgm_isp->is_tree
    if $arg1 != 0
	showipceheader
	set $kgm_iindex = 0
	set $kgm_iep = $kgm_is.is_table
        while ( $kgm_iindex < $kgm_is.is_table_size )
	    set $kgm_ie = *$kgm_iep
	    if $kgm_ie.ie_bits & 0x001f0000
		set $kgm_name = (($kgm_iindex << 8)|($kgm_ie.ie_bits >> 24))
		showipceint $kgm_iep $kgm_name
	    end
	    set $kgm_iindex = $kgm_iindex + 1
	    set $kgm_iep = &($kgm_is.is_table[$kgm_iindex])
	end
	if $kgm_is.is_tree_total
	    printf "Still need to write tree traversal\n"
	end
    end
    printf "\n"
end


define showipc
	set $kgm_isp = (ipc_space_t)$arg0
        showipcheader
	showipcint $kgm_isp 0
end
document showipc
| Routine to print the status of the specified ipc space
| The following is the syntax:
|     (gdb) showipc <ipc_space>
end

define showrights
	set $kgm_isp = (ipc_space_t)$arg0
        showipcheader
	showipcint $kgm_isp 1
end
document showrights
| Routine to print a summary list of all the rights in a specified ipc space
| The following is the syntax:
|     (gdb) showrights <ipc_space>
end


define showtaskipc
	set $kgm_taskp = (task_t)$arg0
	showtaskheader
    showipcheader
	showtaskint $kgm_taskp
	showipcint $kgm_taskp->itk_space 0
end
document showtaskipc
| Routine to print the status of the ipc space for a task
| The following is the syntax:
|     (gdb) showtaskipc <task>
end


define showtaskrights
	set $kgm_taskp = (task_t)$arg0
	showtaskheader
        showipcheader
	showtaskint $kgm_taskp
	showipcint $kgm_taskp->itk_space 1
end
document showtaskrights
| Routine to print a summary listing of all the ipc rights for a task
| The following is the syntax:
|     (gdb) showtaskrights <task>
end

define showallipc
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
        showipcheader
	showtaskint $kgm_taskp
	showipcint $kgm_taskp->itk_space 0
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallipc
| Routine to print a summary listing of all the ipc spaces
| The following is the syntax:
|     (gdb) showallipc
end


define showallrights
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
        showtaskheader
        showipcheader
	showtaskint $kgm_taskp
	showipcint $kgm_taskp->itk_space 1
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showallrights
| Routine to print a summary listing of all the ipc rights
| The following is the syntax:
|     (gdb) showallrights
end


define showtaskvm
	set $kgm_taskp = (task_t)$arg0
	showtaskheader
	showmapheader
	showtaskint $kgm_taskp
	showvmint $kgm_taskp->map 0
end
document showtaskvm
| Routine to print out a summary description of a task's vm_map
| The following is the syntax:
|     (gdb) showtaskvm <task>
end

define showtaskvme
	set $kgm_taskp = (task_t)$arg0
	showtaskheader
	showmapheader
	showtaskint $kgm_taskp
	showvmint $kgm_taskp->map 1
end
document showtaskvme
| Routine to print out a summary listing of a task's vm_map_entries
| The following is the syntax:
|     (gdb) showtaskvme <task>
end


define showtaskheader
    printf "task        vm_map      ipc_space  #acts  "
    showprocheader
end


define showtaskint
    set $kgm_task = *(Task *)$arg0
    printf "0x%08x  ", $arg0
    printf "0x%08x  ", $kgm_task.map
    printf "0x%08x  ", $kgm_task.itk_space
    printf "%3d  ", $kgm_task.thr_act_count
    showprocint $kgm_task.bsd_info
end

define showtask
    showtaskheader
    showtaskint $arg0
end
document showtask
| Routine to print out info about a task.
| The following is the syntax:
|     (gdb) showtask <task>
end


define showtaskacts
    showtaskheader
    set $kgm_taskp = (Task *)$arg0
    showtaskint $kgm_taskp
    showactheader
    set $kgm_head_actp = &($kgm_taskp->thr_acts)
    set $kgm_actp = (Thread_Activation *)($kgm_taskp->thr_acts.next)
    while $kgm_actp != $kgm_head_actp
	showactint $kgm_actp 0
    	set $kgm_actp = (Thread_Activation *)($kgm_actp->thr_acts.next)
    end
end
document showtaskacts
| Routine to print a summary listing of the activations in a task
| The following is the syntax:
|     (gdb) showtaskacts <task>
end


define showtaskstacks
    showtaskheader
    set $kgm_taskp = (Task *)$arg0
    showtaskint $kgm_taskp
    set $kgm_head_actp = &($kgm_taskp->thr_acts)
    set $kgm_actp = (Thread_Activation *)($kgm_taskp->thr_acts.next)
    while $kgm_actp != $kgm_head_actp
        showactheader
	showactint $kgm_actp 1
    	set $kgm_actp = (Thread_Activation *)($kgm_actp->thr_acts.next)
    end
end
document showtaskstacks
| Routine to print a summary listing of the activations in a task and their stacks
| The following is the syntax:
|     (gdb) showtaskstacks <task>
end


define showalltasks
    showtaskheader
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
	showtaskint $kgm_taskp
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end
document showalltasks
| Routine to print a summary listing of all the tasks
| The following is the syntax:
|     (gdb) showalltasks
end


define showprocheader
    printf " pid  proc        command\n"
end

define showprocint
    set $kgm_procp = (struct proc *)$arg0
    if $kgm_procp != 0
        printf "%5d  ", $kgm_procp->p_pid
	printf "0x%08x  ", $kgm_procp
	printf "%s\n", $kgm_procp->p_comm
    else
	printf "  *0*  0x00000000  --\n"
    end
end

define showpid
    showtaskheader
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while $kgm_taskp != $kgm_head_taskp
	set $kgm_procp = (struct proc *)$kgm_taskp->bsd_info
	if (($kgm_procp != 0) && ($kgm_procp->p_pid == $arg0))
	    showtaskint $kgm_taskp
	    set $kgm_taskp = $kgm_head_taskp
	else
    	    set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
	end
    end
end
document showpid
| Routine to print a summary listing of all the tasks
| The following is the syntax:
|     (gdb) showalltasks
end

define showproc
    showtaskheader
    set $kgm_procp = (struct proc *)$arg0
    showtaskint $kgm_procp->task $arg1 $arg2
end


define kdb
    set switch_debugger=1
    continue
end
document kdb
| kdb - Switch to the inline kernel debugger
|
| usage: kdb
|
| The kdb macro allows you to invoke the inline kernel debugger.
end

define showpsetheader
    printf "portset     waitqueue   recvname    "
    printf "flags refs  recvspace   process\n"
end

define showportheader
    printf "port        mqueue      recvname    "
    printf "flags refs  recvspace   process\n"
end

define showportmemberheader
    printf "            port        recvname    "
    printf "flags refs  mqueue      msgcount\n"
end

define showkmsgheader
    printf "            kmsg        size        "
    printf "disp msgid  remote-port local-port\n"
end

define showkmsgint
    printf "            0x%08x  ", $arg0
    set $kgm_kmsgh = ((ipc_kmsg_t)$arg0)->ikm_header
    printf "0x%08x  ", $kgm_kmsgh.msgh_size
    if (($kgm_kmsgh.msgh_bits & 0xff) == 19)
	printf "rC"
    else
	printf "rM"
    end
    if (($kgm_kmsgh.msgh_bits & 0xff00) == (19 < 8))
	printf "lC"
    else
	printf "lM"
    end
    if ($kgm_kmsgh.msgh_bits & 0xf0000000)
	printf "c"
    else
	printf "s"
    end
    printf "%5d  ", $kgm_kmsgh.msgh_msgid
    printf "0x%08x  ", $kgm_kmsgh.msgh_remote_port
    printf "0x%08x\n", $kgm_kmsgh.msgh_local_port
end

define showprocforspace
    set $kgm_spacep = (ipc_space_t)$arg0
    set $kgm_found = 0
    set $kgm_head_taskp = &default_pset.tasks
    set $kgm_taskp = (Task *)($kgm_head_taskp->next)
    while (!$kgm_found && ($kgm_taskp != $kgm_head_taskp))
	if ($kgm_taskp->itk_space == $kgm_spacep)
	    set $kgm_found = 1
	    set $kgm_procp = (struct proc *)$kgm_taskp->bsd_info
	    if $kgm_procp != 0
		printf "%s\n", $kgm_procp->p_comm
	    else
		printf "task 0x%08x\n", $kgm_taskp
	    end
	end
    	set $kgm_taskp = (Task *)($kgm_taskp->pset_tasks.next)
    end
end

define showportmember
    printf "            0x%08x  ", $arg0
    set $kgm_portp = (ipc_port_t)$arg0
    printf "0x%08x  ", $kgm_portp->ip_object.io_receiver_name
    if ($kgm_portp->ip_object.io_bits & 0x80000000)
	printf "A"
    else
	printf " "
    end
    if ($kgm_portp->ip_object.io_bits & 0x7fff0000)
	printf "Set "
    else
	printf "Port"
    end
    printf "%5d  ", $kgm_portp->ip_object.io_references
    printf "0x%08x  ", &($kgm_portp->ip_messages)
    printf "0x%08x\n", $kgm_portp->ip_messages.data.port.msgcount
end

define showportint
    printf "0x%08x  ", $arg0
    set $kgm_portp = (ipc_port_t)$arg0
    printf "0x%08x  ", &($kgm_portp->ip_messages)
    printf "0x%08x  ", $kgm_portp->ip_object.io_receiver_name
    if ($kgm_portp->ip_object.io_bits & 0x80000000)
	printf "A"
    else
	printf "D"
    end
    if ($kgm_portp->ip_object.io_bits & 0x7fff0000)
	printf "Set "
    else
	printf "Port"
    end
    printf "%5d  ", $kgm_portp->ip_object.io_references
    printf "0x%08x  ", $kgm_portp->data.receiver
    showprocforspace $kgm_portp->data.receiver
    set $kgm_kmsgp = (ipc_kmsg_t)$kgm_portp->ip_messages.data.port.messages.ikmq_base
    if $arg1 && $kgm_kmsgp
	showkmsgheader
	showkmsgint $kgm_kmsgp
	set $kgm_kmsgheadp = $kgm_kmsgp
	set $kgm_kmsgp = $kgm_kmsgp->ikm_next
	while $kgm_kmsgp != $kgm_kmsgheadp
	    showkmsgint $kgm_kmsgp
	    set $kgm_kmsgp = $kgm_kmsgp->ikm_next
        end
    end
end

define showpsetint
    printf "0x%08x  ", $arg0
    set $kgm_psetp = (ipc_pset_t)$arg0
    printf "0x%08x  ", &($kgm_psetp->ips_messages)
    printf "0x%08x  ", $kgm_psetp->ips_object.io_receiver_name
    if ($kgm_psetp->ips_object.io_bits & 0x80000000)
	printf "A"
    else
	printf "D"
    end
    if ($kgm_psetp->ips_object.io_bits & 0x7fff0000)
	printf "Set "
    else
	printf "Port"
    end
    printf "%5d  ", $kgm_psetp->ips_object.io_references
    set $kgm_sublinksp = &($kgm_psetp->ips_messages.data.set_queue.wqs_sublinks)
    set $kgm_wql = (wait_queue_link_t)$kgm_sublinksp->next
    set $kgm_found = 0
    while ( (queue_entry_t)$kgm_wql != (queue_entry_t)$kgm_sublinksp)
        set $kgm_portp = (ipc_port_t)((int)($kgm_wql->wql_element->wqe_queue) - ((int)$kgm_portoff))
	if !$kgm_found  
	    printf "0x%08x  ", $kgm_portp->data.receiver
	    showprocforspace $kgm_portp->data.receiver
	    showportmemberheader
	    set $kgm_found = 1
	end
	showportmember $kgm_portp 0
	set $kgm_wql = (wait_queue_link_t)$kgm_wql->wql_sublinks.next
    end
    if !$kgm_found   
	printf "--n/e--     --n/e--\n"
    end
end

define showpset
    showpsetheader
    showpsetint $arg0 1
end

define showport
    showportheader
    showportint $arg0 1
end

define showmqueue
    set $kgm_mqueue = *(ipc_mqueue_t)$arg0
    set $kgm_psetoff = &(((ipc_pset_t)0)->ips_messages)
    set $kgm_portoff = &(((ipc_port_t)0)->ip_messages)
    if ($kgm_mqueue.data.set_queue.wqs_wait_queue.wq_issub)
	set $kgm_pset = (((int)$arg0) - ((int)$kgm_psetoff))
        showpsetheader
	showpsetint $kgm_pset 1
    else
	showportheader
	set $kgm_port = (((int)$arg0) - ((int)$kgm_portoff))
	showportint $kgm_port 1
    end
end

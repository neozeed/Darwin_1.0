#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-dyld-process.h"
#include "nextstep-nat-inferior-debug.h"

#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <mach/mach.h>

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include <string.h>
#include <signal.h>
#include <sys/ptrace.h>

#include "mach-o.h"

const char *ptrace_request_unparse (int request)
{
  switch (request) {
  case PTRACE_TRACEME: return "PTRACE_TRACEME";
  case PTRACE_CONT: return "PTRACE_CONT";
  case PTRACE_KILL: return "PTRACE_KILL";
  case PTRACE_SINGLESTEP: return "PTRACE_SINGLESTEP";
  case PTRACE_ATTACH: return "PTRACE_ATTACH";
  case PTRACE_DETACH: return "PTRACE_DETACH";
  default: return "[UNKNOWN]";
  }
}

int call_ptrace (int request, int pid, int arg3, int arg4)
{
  int ret;
  errno = 0;
  ret = ptrace (request, pid, (caddr_t) arg3, arg4);

  inferior_debug ("ptrace (%s, %d, %d, %d): %d (%s)\n",
		  ptrace_request_unparse (request),
		  pid, arg3, arg4, ret, (ret != 0) ? strerror (errno) : "no error");
  return ret;
}

void next_inferior_reset (next_inferior_status *s)
{
  s->pid = 0;
  s->task = TASK_NULL;

  s->attached_in_ptrace = 0;
  s->stopped_in_ptrace = 0;

  s->suspend_count = 0;

  s->last_thread = THREAD_NULL;

  s->thread_list = NULL;

  next_signal_thread_init (&s->signal_status);
  s->signal_port = PORT_NULL;

  next_dyld_thread_init (&s->dyld_status);
  s->dyld_port = PORT_NULL;

  s->notify_port = PORT_NULL;
  s->exception_port = PORT_NULL;
  s->exception_reply_port = PORT_NULL;

#if defined (__MACH30__)
  memset (&s->saved_exceptions, 0, sizeof (s->saved_exceptions));
  memset (&s->saved_exceptions_step, 0, sizeof (s->saved_exceptions_step));
#endif /* __MACH30__ */
  s->saved_exceptions_stepping = 0;

}

void next_inferior_destroy (next_inferior_status *s)
{
  if (s->signal_port != PORT_NULL) {
    next_signal_thread_destroy (&s->signal_status);
    port_deallocate (task_self (), s->signal_port);
    s->signal_port = PORT_NULL;
  }

  if (s->dyld_port != PORT_NULL) {
    next_dyld_thread_destroy (s, &s->dyld_status);
    port_deallocate (task_self (), s->dyld_port);
    s->dyld_port = PORT_NULL;
  }

  if (s->notify_port != PORT_NULL) {
    port_deallocate (task_self (), s->notify_port);
    s->notify_port = PORT_NULL;
  }

  if (s->cfm_info.cfm_receive_right != PORT_NULL) {
    port_deallocate (task_self(), s->cfm_info.cfm_receive_right);
    port_deallocate (task_self(), s->cfm_info.cfm_send_right);
    s->cfm_info.cfm_receive_right = PORT_NULL;
    s->cfm_info.cfm_send_right = PORT_NULL;
  }
    
  next_thread_list_destroy (s->thread_list);
  s->thread_list = NULL;

  s->task = TASK_NULL;
  s->pid = 0;

  next_inferior_reset (s);
}

int next_inferior_valid (next_inferior_status *s)
{
  kern_return_t kret;
  int ret;

  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  kret = task_info (s->task, TASK_BASIC_INFO, (task_info_t) &info, &info_count);
  if (kret != KERN_SUCCESS) { return 0; }

  ret = kill (s->pid, 0);
  if (ret != 0) { return 0; }

  return 1;
}

void next_inferior_check_stopped (next_inferior_status *s)
{
  GDB_CHECK (s != NULL);

  GDB_CHECK (s->task != TASK_NULL);
  GDB_CHECK (s->pid != 0);

  if (s->stopped_in_ptrace) { 
    GDB_CHECK (s->attached_in_ptrace);
  }

  GDB_CHECK ((s->stopped_in_ptrace == 1) || (s->suspend_count > 0));
  GDB_CHECK ((s->suspend_count == 0) || (s->suspend_count == 1));
}  

kern_return_t next_inferior_suspend_mach (next_inferior_status *s)
{
  kern_return_t kret;

  GDB_CHECK (s != NULL);
  GDB_CHECK (next_task_valid (s->task));

  if (s->suspend_count == 0) {
    inferior_debug ("suspending task\n");
    kret = task_suspend (s->task);
    if (kret != KERN_SUCCESS) {
      return kret;
    }
    s->suspend_count++;
    inferior_debug ("suspended task (suspend count now %d)\n", s->suspend_count);
  }

  return KERN_SUCCESS;
}

kern_return_t next_inferior_resume_mach (next_inferior_status *s, int count)
{
  kern_return_t kret;
  
  GDB_CHECK (s != NULL);
  GDB_CHECK (next_task_valid (s->task));

  for (;;) {
    if (s->suspend_count == 0) {
      break;
    }
    if (count == 0) {
      break;
    }
    inferior_debug ("resuming task\n");
    kret = task_resume (s->task);
    if (kret != KERN_SUCCESS) {
      return kret;
    }
    s->suspend_count--;
    count--;
    inferior_debug ("resumed task (suspend count now %d)\n", s->suspend_count);
  }

  return KERN_SUCCESS;
}

void next_inferior_suspend_ptrace (next_inferior_status *s)
{
  struct target_waitstatus status;

  GDB_CHECK (s != NULL);
  GDB_CHECK (s->attached_in_ptrace);
  GDB_CHECK (next_inferior_valid (s));
  next_inferior_check_stopped (s);
  GDB_CHECK (! s->stopped_in_ptrace);

  next_inferior_suspend_mach (s);
  kill (s->pid, SIGSTOP);
  next_inferior_resume_mach (s, -1);
  
  next_wait (s, &status);
  GDB_CHECK (status.kind == TARGET_WAITKIND_STOPPED);
  GDB_CHECK (status.value.sig == TARGET_SIGNAL_STOP);
}

void next_inferior_resume_ptrace (next_inferior_status *s, int nsignal, int val)
{
  GDB_CHECK (s != NULL);
  GDB_CHECK ((val == PTRACE_DETACH) || (val == PTRACE_CONT));

  GDB_CHECK (s->attached_in_ptrace);
  GDB_CHECK (next_inferior_valid (s));
  next_inferior_check_stopped (s);
  GDB_CHECK (s->stopped_in_ptrace);

  next_inferior_suspend_mach (s);
 
  if (call_ptrace (val, s->pid, 1, nsignal) != 0) {
    error ("Error calling ptrace (%d, %d, %d, %d): %s\n",
	   val, s->pid, 1, nsignal, strerror (errno));
  }

  s->stopped_in_ptrace = 0;
  if (val == PTRACE_DETACH) {
    s->attached_in_ptrace = 0;
  }
}

void next_thread_list_lookup_by_id (next_inferior_status *s, int id, int *ppid, thread_t *pthread)
{
  thread_process_id *tpid = s->thread_list;

  GDB_CHECK (s != NULL);
  GDB_CHECK (id > 0);
  GDB_CHECK (ppid != NULL);
  GDB_CHECK (pthread != NULL);

  while ((tpid != NULL) && (tpid->id != id)) {
    tpid = tpid->next;
  }

  if (tpid == NULL) {
    error ("invalid thread ID %d", id);
  }

  *ppid = tpid->pid;
  *pthread = tpid->thread;
}

void next_thread_list_lookup_by_info (next_inferior_status *s, int pid, thread_t thread, int *id)
{
  thread_process_id *tpid = s->thread_list;

  GDB_CHECK (s != NULL);
  GDB_CHECK (pid != NULL);


  while ((tpid != NULL) && ((tpid->pid != pid) || (tpid->thread != thread))) {
    tpid = tpid->next;
  }

  if (tpid == NULL) {
    error ("unknown thread id 0x%lx for process %d", (unsigned long) thread, pid);
  }

  *id = tpid->id;
}

int next_thread_list_insert (next_inferior_status *s, int pid, thread_t thread)
{
  thread_process_id *tpid = s->thread_list;
  size_t i = 0;
  
  GDB_CHECK (s != NULL);

  while ((tpid != NULL) && ((tpid->pid != pid) || (tpid->thread != thread))) {
    tpid = tpid->next;
    i++;
  }

  if (tpid == NULL) {

    tpid = (thread_process_id *) xmalloc (sizeof (thread_process_id));

    tpid->next = s->thread_list;

    tpid->pid = pid;
    tpid->thread = thread;
    tpid->id = i + 1;

    s->thread_list = tpid;
  }

  return tpid->id;
}

void next_thread_list_destroy (thread_process_id *tpid)
{
  while (tpid != NULL) {
    thread_process_id *dead = tpid;
    tpid = tpid->next;
    xfree (dead);
  }
}

#if defined (__MACH30__)

void next_save_exception_ports
(task_t task, struct mach_exception_info *info)
{
  kern_return_t kret;

  info->count = (sizeof (info->ports) / sizeof (info->ports[0]));
  kret = task_get_exception_ports 
    (task,
     EXC_MASK_ALL & ~(EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT | EXC_MASK_SOFTWARE),
     info->masks, &info->count, info->ports, info->behaviors, info->flavors);
  MACH_CHECK_ERROR (kret);
}

void next_restore_exception_ports
(task_t task, struct mach_exception_info *info)
{
  int i;
  kern_return_t kret;

  for (i = 0; i < info->count; i++) {
    kret = task_set_exception_ports
      (task, info->masks[i], info->ports[i], info->behaviors[i], info->flavors[i]);
    MACH_CHECK_ERROR (kret);
  }
}

#else /* ! __MACH30__ */

void next_save_exception_ports
(task_t task, struct mach_exception_info *info)
{
  kern_return_t kret;
  kret = task_get_exception_port (task, &info->port);
  MACH_CHECK_ERROR (kret);
}

void next_restore_exception_ports
(task_t task, struct mach_exception_info *info)
{
  kern_return_t kret;
  kret = task_set_exception_port (task, info->port);
  MACH_CHECK_ERROR (kret);
}

#endif /* __MACH30__ */

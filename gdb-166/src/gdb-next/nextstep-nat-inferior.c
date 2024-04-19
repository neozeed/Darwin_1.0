#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-infthread.h"
#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-threads.h"
#include "nextstep-xdep.h"
#include "nextstep-nat-inferior-util.h"

#if WITH_CFM
#include "nextstep-nat-cfm.h"
#endif

#include "defs.h"
#include "top.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "environ.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/sysctl.h>

extern bfd *exec_bfd;

extern struct target_ops child_ops;
extern struct target_ops next_child_ops;

next_inferior_status *next_status = NULL;

int inferior_ptrace_flag = 1;
int inferior_ptrace_on_attach_flag = 1;
int inferior_bind_exception_port_flag = 1;
int inferior_bind_notify_port_flag = 0;
int inferior_handle_exceptions_flag = 1;
int inferior_handle_all_events_flag = 1;
int inferior_auto_start_dyld_flag = 1;
int inferior_auto_start_dyld_on_attach_flag = 1;
int inferior_handle_dyld_events_asynchronously_flag = 1;

#if WITH_CFM
int inferior_auto_start_cfm_flag = 1;
#endif /* WITH_CFM */

static void next_handle_signal
(next_signal_thread_message *msg, struct target_waitstatus *status);

static void next_process_events (struct next_inferior_status *ns, struct target_waitstatus *status, int timeout);

static void next_child_resume (int tpid, int step, enum target_signal signal);

static int next_mach_wait (int pid, struct target_waitstatus *status);

static void next_mourn_inferior ();

static int next_lookup_task (char *args, task_t *ptask, int *ppid);

static void next_child_attach (char *args, int from_tty);

static void next_child_detach (char *args, int from_tty);

static int next_kill_inferior (kern_return_t *);
static void next_kill_inferior_safe ();

static void next_ptrace_me ();

static int next_ptrace_him (int pid);

static void next_child_create_inferior (char *exec_file, char *allargs, char **env);

static void next_child_files_info (struct target_ops *ops);

static char *next_mach_pid_to_str (int tpid);

static int next_child_thread_alive (int tpid);

static void next_handle_signal
(next_signal_thread_message *msg, struct target_waitstatus *status)
{
  kern_return_t kret;

  GDB_CHECK_FATAL (next_status != NULL);

  GDB_CHECK_FATAL (next_status->attached_in_ptrace);
  GDB_CHECK_FATAL (! next_status->stopped_in_ptrace);

  if (inferior_debug_flag) {
    inferior_debug ("next_handle_signal: received signal message: ");
    next_signal_thread_debug_status (stderr, msg->status);
  }

  if (msg->pid != next_status->pid) {
    warning ("next_handle_signal: signal message was for pid %d, not for inferior process (pid %d)\n", 
	     msg->pid, next_status->pid);
    return;
  }
  
  if (WIFEXITED (msg->status)) {
    status->kind = TARGET_WAITKIND_EXITED;
    status->value.integer = WEXITSTATUS (msg->status);
    return;
  }

  if (! WIFSTOPPED (msg->status)) {
    status->kind = TARGET_WAITKIND_SIGNALLED;
    status->value.sig = target_signal_from_host (WTERMSIG (msg->status));
    return;
  }

  next_status->stopped_in_ptrace = 1;

  kret = next_inferior_suspend_mach (next_status);
  MACH_CHECK_ERROR (kret);

  prepare_threads_after_stop (next_status);

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = target_signal_from_host (WSTOPSIG (msg->status));
}

static void next_add_to_port_set
(struct next_inferior_status *inferior, port_set_name_t event_port_set, int handling_exceptions)
{
  kern_return_t kret;
 
  if ((inferior->dyld_port != PORT_NULL) && inferior_handle_dyld_events_asynchronously_flag) {
    kret = mach_port_move_member (mach_task_self(), inferior->dyld_port, event_port_set);
    MACH_CHECK_ERROR (kret);
  }

  if (inferior_bind_notify_port_flag) {
    kret = mach_port_move_member (mach_task_self(), inferior->notify_port, event_port_set);
    MACH_CHECK_ERROR (kret);
  }

  if (inferior_bind_exception_port_flag && handling_exceptions) {
    kret = mach_port_move_member (mach_task_self(), inferior->exception_port, event_port_set);
    MACH_CHECK_ERROR (kret);
  }

  kret = mach_port_move_member (mach_task_self(), inferior->signal_port, event_port_set);
  MACH_CHECK_ERROR (kret);
    
#if WITH_CFM
  if (inferior_auto_start_cfm_flag) {
    if (inferior->cfm_info.cfm_receive_right != PORT_NULL) {
      kret = mach_port_move_member (mach_task_self(), inferior->cfm_info.cfm_receive_right, event_port_set);
      MACH_CHECK_ERROR (kret);
    }
  }
#endif
}

static void next_process_events
(struct next_inferior_status *inferior, struct target_waitstatus *status, int timeout)
{
  kern_return_t kret, kret2;

  unsigned char msg_data[1024];
  msg_header_t *msgin = (msg_header_t *) msg_data;

  int handling_exceptions = 1;

  for (;;) {

    port_t local_port;
    port_set_name_t event_port_set;

    GDB_CHECK_FATAL (status->kind == TARGET_WAITKIND_SPURIOUS);

    kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &event_port_set);
    MACH_CHECK_ERROR (kret);

    next_add_to_port_set (inferior, event_port_set, handling_exceptions);

    /* wait for event */
    inferior_debug ("next_process_events: waiting for message for inferior\n");
    kret = next_mach_msg_receive (msgin, sizeof (msg_data), timeout, event_port_set);

    next_add_to_port_set (inferior, MACH_PORT_NULL, handling_exceptions);

    kret2 = mach_port_destroy (mach_task_self(), event_port_set);
    MACH_CHECK_ERROR (kret2);

    if (kret == RCV_TIMED_OUT) { return; }
    MACH_CHECK_ERROR (kret);

#if defined (__MACH30__)
    local_port = msgin->msgh_local_port;
#else
    local_port = msgin->msg_local_port;
#endif

    if (local_port == inferior->dyld_port) {
      
      /* process dyld events */

      inferior_debug ("next_process_events: got dyld message\n");
      kret = next_mach_process_dyld_messages (status, (struct _dyld_event_message_request *) msgin, 0);
      MACH_CHECK_ERROR (kret);
      if (status->kind == TARGET_WAITKIND_LOADED) {
	status->kind = TARGET_WAITKIND_SPURIOUS;
      }
    }

    else if (local_port == inferior->notify_port) {

      /* process notify messages */

      for (;;) {
	inferior_debug ("next_process_events: got notify message\n");
	inferior_debug ("next_mach_process_message: got a notify message");
	next_debug_notification_message (next_status, msgin);
	kret = next_mach_msg_receive (msgin, sizeof (msg_data), 1, inferior->notify_port);
	if (RCV_TIMED_OUT == kret) {
	  break;
	}
      }
    }

    else if (local_port == inferior->exception_port) {

      /* process exceptions */

      for (;;) {

	inferior_debug ("next_process_events: got exception message\n");
	if (! handling_exceptions) {
	  break;
	}

	GDB_CHECK_FATAL (inferior_bind_exception_port_flag);
	next_handle_exception (inferior, msgin, status);
  
	if (! inferior_handle_all_events_flag) {
	  handling_exceptions = 0;
	}

	kret = next_mach_msg_receive (msgin, sizeof (msg_data), 1, inferior->exception_port);
	if (kret == RCV_TIMED_OUT) {
	  break;
	}
      }

      if (status->kind != TARGET_WAITKIND_SPURIOUS) {
	GDB_CHECK_FATAL (inferior_handle_exceptions_flag);
	break;
      }
    }

    else if (local_port == inferior->signal_port) {

      /* process signals */
      for (;;) {

	inferior_debug ("next_process_events: got signal message\n");
	next_handle_signal ((next_signal_thread_message *) msgin, status);
	GDB_CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);
	if (! inferior_handle_all_events_flag) {
	  break;
	}
	kret = next_mach_msg_receive (msgin, sizeof (msg_data), 1, inferior->signal_port);
	if (RCV_TIMED_OUT == kret) {
	  break;
	}
      }

      if (status->kind != TARGET_WAITKIND_SPURIOUS) {
	break;
      }
    }

#if WITH_CFM      
    else if (local_port == inferior->cfm_info.cfm_receive_right) {
      
      if (inferior_auto_start_cfm_flag) {
	next_handle_cfm_event (next_status, msgin);
	status->kind = TARGET_WAITKIND_LOADED;
	break;
      }
    }
#endif /* WITH_CFM */

    else {
      error ("got message on unknown port: 0x%08x\n", local_port);
      break;
    }
  }

  inferior_debug ("next_process_events: returning with (status->kind == %d)\n", status->kind);
}

void next_mach_check_new_threads ()
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;

  kret = task_threads (next_status->task, &thread_list, &nthreads);
  if (kret != KERN_SUCCESS) { return; }
  MACH_CHECK_ERROR (kret);
  
  for (i = 0; i < nthreads; i++) {
    int tpid = next_thread_list_insert (next_status, next_status->pid, thread_list[i]);
    if (! in_thread_list (tpid)) {
      add_thread (tpid);
    }
  }

  kret = vm_deallocate (task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

static void next_child_resume (int tpid, int step, enum target_signal signal)
{
  int nsignal = target_signal_to_host (signal);
  struct target_waitstatus status;

  int pid;
  thread_t thread;

  if (tpid == -1) { 
    tpid = inferior_pid;
  }
  next_thread_list_lookup_by_id (next_status, tpid, &pid, &thread);

  GDB_CHECK_FATAL (tm_print_insn != NULL);
  GDB_CHECK_FATAL (next_status != NULL);

  next_inferior_check_stopped (next_status);
  if (! next_inferior_valid (next_status)) {
    return;
  }

  inferior_debug ("next_child_resume: checking for pending events\n");
  status.kind = TARGET_WAITKIND_SPURIOUS;
  next_process_events (next_status, &status, 1);
  GDB_CHECK_FATAL (status.kind == TARGET_WAITKIND_SPURIOUS);
  
  inferior_debug ("next_child_resume: %s process with signal %d\n", 
		  step ? "stepping" : "continuing", nsignal);

  if (inferior_debug_flag) {
    CORE_ADDR pc = read_register (PC_REGNUM);
    fprintf (stdout, "[%d inferior]: next_child_resume: about to execute instruction at ", getpid ());
    print_address (pc, gdb_stdout);
    fprintf (stdout, " (");
    pc += (*tm_print_insn) (pc, &tm_print_insn_info);
    fprintf (stdout, ")\n");
    fprintf (stdout, "[%d inferior]: next_child_resume: subsequent instruction is ", getpid ());
    print_address (pc, gdb_stdout);
    fprintf (stdout, " (");
    pc += (*tm_print_insn) (pc, &tm_print_insn_info);
    fprintf (stdout, ")\n");
  }
  
  if (next_status->stopped_in_ptrace) {
    next_inferior_resume_ptrace (next_status, nsignal, PTRACE_CONT);
  }

  if (! next_inferior_valid (next_status)) {
    return;
  }

  if (step) {
    prepare_threads_before_run (next_status, step, thread, (tpid != -1));
  } else {
    prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);
  }

  next_inferior_resume_mach (next_status, -1);
}

int next_wait (struct next_inferior_status *ns, struct target_waitstatus *status)
{
  int thread_id;
  kern_return_t kret;

  GDB_CHECK_FATAL (ns != NULL);
  
  set_sigint_trap ();
  set_sigio_trap ();

  status->kind = TARGET_WAITKIND_SPURIOUS;
  next_process_events (ns, status, 0);
  GDB_CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);
  
  clear_sigio_trap ();
  clear_sigint_trap();

  if ((status->kind == TARGET_WAITKIND_EXITED) || (status->kind == TARGET_WAITKIND_SIGNALLED)) {
    return 0;
  }

  kret = next_mach_process_dyld_messages (NULL, NULL, 0);
  MACH_CHECK_ERROR (kret);

  next_mach_check_new_threads ();

  if (! next_thread_valid (next_status->task, next_status->last_thread)) {
    if (next_task_valid (next_status->task)) {
      warning ("Currently selected thread no longer alive; selecting intial thread");
      next_status->last_thread = next_primary_thread_of_task (next_status->task);
    }
  }

  next_thread_list_lookup_by_info (next_status, next_status->pid, next_status->last_thread, &thread_id);
  
  inferior_debug ("next_wait: returning 0x%lx\n", thread_id);
  return thread_id;
}

static int next_mach_wait (int pid, struct target_waitstatus *status)
{
  GDB_CHECK_FATAL (next_status != NULL);
  return next_wait (next_status, status);
}

static void next_mourn_inferior ()
{
  unpush_target (&next_child_ops);
  child_ops.to_mourn_inferior ();
  next_inferior_destroy (next_status);

  inferior_pid = 0;
  attach_flag = 0;

  if (symfile_objfile != NULL) {
    GDB_CHECK_FATAL (symfile_objfile->obfd != NULL);
    next_init_dyld_symfile (symfile_objfile->obfd);
  } else {
    next_init_dyld_symfile (NULL);
  }
}

void next_fetch_task_info (struct kinfo_proc **info, size_t *count)
{
  struct kinfo_proc *proc;
  unsigned int control[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
  size_t length;

  GDB_CHECK_FATAL (info != NULL);
  GDB_CHECK_FATAL (count != NULL);

  sysctl (control, 3, NULL, &length, NULL, 0);
  proc = (struct kinfo_proc *) xmalloc (length);
  sysctl (control, 3, proc, &length, NULL, 0);

  *count = length / sizeof (struct kinfo_proc);
  *info = proc;
}

char **next_process_completer (char *text, char *word)
{
  struct kinfo_proc *proc = NULL;
  size_t count, i;
  
  char **procnames = NULL;
  char **ret = NULL;

  next_fetch_task_info (&proc, &count);
  
  procnames = (char **) xmalloc ((count + 1) * sizeof (char *));

  for (i = 0; i < count; i++) {
    procnames[i] = (char *) xmalloc (strlen (proc[i].kp_proc.p_comm) + 1 + 16);
    sprintf (procnames[i], "%s.%d", proc[i].kp_proc.p_comm, proc[i].kp_proc.p_pid);
  }
  procnames[i] = NULL;

  ret = complete_on_enum (procnames, text, word);

  xfree (proc);
  return ret;
}

static void next_lookup_task_remote (char *host_str, char *pid_str, int pid, task_t *ptask, int *ppid)
{
  GDB_CHECK_FATAL (ptask != NULL);
  GDB_CHECK_FATAL (ppid != NULL);

#if defined (__MACH30__)

  error ("Unable to attach to remote processes on Mach 3.0 (no netname_look_up ()).");

#else /* ! __MACH30__ */

  if (pid_str == NULL) {

    kern_return_t kret;
    task_t pid_server = task_self ();

    if (host_str != NULL) {
      kret = netname_look_up (name_server_port, host_str, "pid-server", &pid_server);
      MACH_WARN_ERROR (kret);
      if (kret != KERN_SUCCESS) {
	error ("Unable to locate pid server on host \"%s\".", host_str);
      }
    }
    kret = task_by_unix_pid (pid_server, pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to access task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }

  } else {

    kern_return_t kret;
    if (host_str == NULL) {
      host_str = "localhost";
    }
    kret = netname_look_up (name_server_port, host_str, pid_str, &itask);
    MACH_WARN_ERROR (kret);
    if (kret != KERN_SUCCESS) {
      error ("Unable to find task named \"%s\" on host \"%s\"", pid_str, host_str);
    }
  }

#endif /* __MACH30__ */  
}

static void next_lookup_task_local (char *pid_str, int pid, task_t *ptask, int *ppid)
{
  GDB_CHECK_FATAL (ptask != NULL);
  GDB_CHECK_FATAL (ppid != NULL);

  if (pid_str == NULL) {

    task_t itask;
    kern_return_t kret;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to access task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }
    *ptask = itask;
    *ppid = pid;

  } else {

    struct cleanup *cleanups = NULL;
    char **ret = next_process_completer (pid_str, pid_str);
    char *tmp = NULL;
    char *tmp2 = NULL;
    unsigned long lpid = 0;

    task_t itask;
    kern_return_t kret;

    cleanups = make_cleanup (free, ret);

    if ((ret == NULL) || (ret[0] == NULL)) {
      error ("Unable to locate process named \"%s\".", pid_str);
    }
    if (ret[1] != NULL) {
      error ("Multiple processes exist with the name \"%s\".", pid_str);
    }
  
    tmp = strrchr (ret[0], '.');
    if (tmp == NULL) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    tmp++;
    lpid = strtoul (tmp, &tmp2, 10);
    if (! isdigit (*tmp) || (*tmp2 != '\0')) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to parse process-id \"%s\" (integer overflow).", ret[0]);
    }
    pid = lpid;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to locate task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }

    *ptask = itask;
    *ppid = pid;

    do_cleanups (cleanups);
  }
}    

static int next_lookup_task (char *args, task_t *ptask, int *ppid)
{
  char *host_str = NULL;
  char *pid_str = NULL;
  char *tmp = NULL;

  struct cleanup *cleanups = NULL;
  char **argv = NULL;
  unsigned int argc;
 
  unsigned long lpid = 0;
  int pid = 0;

  GDB_CHECK_FATAL (ptask != NULL);
  GDB_CHECK_FATAL (ppid != NULL);

  *ptask = TASK_NULL; 
  *ppid = 0;

  if (args == NULL) {
    return 0;
  }

  argv = buildargv (args);
  if (argv == NULL) {
    nomem (0);
  }

  cleanups = make_cleanup_freeargv (argv);

  for (argc = 0; argv[argc] != NULL; argc++);

  switch (argc) {
  case 1:
    pid_str = argv[0];
    break;
  case 2:
    host_str = argv[0];
    pid_str = argv[1];
    break;
  default:
    error ("Usage: attach [host] <pid|pid-string>.");
    break;
  }

  GDB_CHECK_FATAL (pid_str != NULL);
  lpid = strtoul (pid_str, &tmp, 10);
  if (isdigit (*pid_str) && (*tmp == '\0')) {
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to locate pid \"%s\" (integer overflow).", pid_str);
    }
    pid_str = NULL;
    pid = lpid;
  }

  if (host_str != NULL) {
    next_lookup_task_remote (host_str, pid_str, pid, ptask, ppid);
  } else {
    next_lookup_task_local (pid_str, pid, ptask, ppid);
  }

  do_cleanups (cleanups);
  return 0;
}

static void next_child_attach (char *args, int from_tty)
{
  struct target_waitstatus w;
  task_t itask;
  int pid;
  int ret;
  kern_return_t kret;
  struct _dyld_debug_task_state dyld_state;
  enum dyld_debug_return dret;

  if (args == NULL) {
    error_no_arg ("process-id to attach");
  }

  next_lookup_task (args, &itask, &pid);
  if (itask == TASK_NULL) {
    error ("unable to locate task");
  }

  if (itask == mach_task_self ()) {
    error ("unable to debug self");
  }
  
  dret = _dyld_debug_make_runnable (itask, &dyld_state);
  if (dret != DYLD_SUCCESS) {
    warning ("next_mach_start_dyld: failed to start inferior dyld_debug thread: %s (%d)",
	     dyld_debug_error_string (dret), dret);
  } else {
    dret = _dyld_debug_restore_runnable (itask, &dyld_state);
    if (dret != DYLD_SUCCESS) {
      warning ("next_mach_start_dyld: failed to restore inferior dyld_debug thread state: %s (%d)",
	       dyld_debug_error_string (dret), dret);
    }
  }
  
  GDB_CHECK_FATAL (next_status != NULL);
  next_inferior_destroy (next_status);

  next_create_inferior_for_task (next_status, itask, pid);

  if (inferior_ptrace_on_attach_flag) {

    ret = call_ptrace (PTRACE_ATTACH, pid, 0, 0);
    if (ret != 0) {
      next_inferior_destroy (next_status);
      error ("Unable to attach to process-id %d: %s (%d)",
	       pid, strerror (errno), errno);
    }
    next_status->attached_in_ptrace = 1;
    next_status->stopped_in_ptrace = 0;
    next_status->suspend_count = 0;

  } else {
    if (inferior_bind_exception_port_flag) {
      kret = next_inferior_suspend_mach (next_status);
      if (kret != KERN_SUCCESS) {
	next_inferior_destroy (next_status);
	MACH_CHECK_ERROR (kret);
      }
    }
  }
  
  next_mach_check_new_threads ();

  next_thread_list_lookup_by_info (next_status, pid, next_status->last_thread, &inferior_pid);
  attach_flag = 1;

  push_target (&next_child_ops);

  if (next_status->attached_in_ptrace) {
    /* read attach notification */
    next_wait (next_status, &w);
  }
}

static void next_child_detach (char *args, int from_tty)
{
  GDB_CHECK_FATAL (next_status != NULL);

  if (inferior_pid == 0) {
    return 0;
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }
  
  next_inferior_check_stopped (next_status);
  GDB_CHECK (next_inferior_valid (next_status));
    
  if (next_status->attached_in_ptrace && (! next_status->stopped_in_ptrace)) {
    next_inferior_suspend_ptrace (next_status);
    GDB_CHECK_FATAL (next_status->stopped_in_ptrace);
  }

  if (inferior_bind_exception_port_flag) {
    next_restore_exception_ports (next_status->task, &next_status->saved_exceptions);
  }

  if (next_status->attached_in_ptrace) {
    next_inferior_resume_ptrace (next_status, 0, PTRACE_DETACH);
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }

  next_inferior_suspend_mach (next_status);

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }

  prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);
  next_inferior_resume_mach (next_status, -1);

  target_mourn_inferior ();
  return;
}

static int next_kill_inferior (kern_return_t *errval)
{
  GDB_CHECK_FATAL (next_status != NULL);
  *errval = KERN_SUCCESS;

  if (inferior_pid == 0) {
    return 1;
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return 1;
  }

  next_inferior_check_stopped (next_status);
  GDB_CHECK (next_inferior_valid (next_status));
  
  if (next_status->attached_in_ptrace && (! next_status->stopped_in_ptrace)) {
    next_inferior_suspend_ptrace (next_status);
    GDB_CHECK_FATAL (next_status->stopped_in_ptrace);
  }
  
  next_inferior_suspend_mach (next_status);
  prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);
  
  if (next_status->attached_in_ptrace) {
    GDB_CHECK_FATAL (next_status->stopped_in_ptrace);
    if (call_ptrace (PTRACE_KILL, next_status->pid, 0, 0) != 0) {
	  error ("next_child_detach: ptrace (%d, %d, %d, %d): %s",
		 PTRACE_KILL, next_status->pid, 0, 0, strerror (errno));
    }
    next_status->stopped_in_ptrace = 0;
  }

  next_inferior_resume_mach (next_status, -1);
  target_mourn_inferior ();

  return 1;
}

static void next_kill_inferior_safe ()
{
  kern_return_t kret;
  int ret;

  ret = catch_errors
    (next_kill_inferior, &kret, "error while killing target (killing anyway): ", RETURN_MASK_ALL);

  if (ret == 0) {
    kret = task_terminate (next_status->task);
    MACH_WARN_ERROR (kret);
    target_mourn_inferior ();
  }
}

static void next_ptrace_me ()
{
  call_ptrace (PTRACE_TRACEME, 0, 0, 0);
}

static int next_ptrace_him (int pid)
{
  task_t itask;
  kern_return_t kret;
  int traps_expected;
  int pret;

  GDB_CHECK_FATAL (! next_status->attached_in_ptrace);
  GDB_CHECK_FATAL (! next_status->stopped_in_ptrace);
  GDB_CHECK_FATAL (next_status->suspend_count == 0);

  kret = task_by_unix_pid (task_self (), pid, &itask);
  if (kret != KERN_SUCCESS) {
    error ("Unable to find Mach task port for process-id %d: %s (0x%lx).", 
	   pid, MACH_ERROR_STRING (kret), kret);
  }

  inferior_debug ("inferior task: 0x%08x, pid: %d\n", itask, pid);
  
  push_target (&next_child_ops);
  next_create_inferior_for_task (next_status, itask, pid);

  next_status->attached_in_ptrace = 1;
  next_status->stopped_in_ptrace = 0;
  next_status->suspend_count = 0;

  traps_expected = (start_with_shell_flag ? 2 : 1);
  startup_inferior (traps_expected);
  
  if (inferior_pid == 0) {
    return 0;
  }

  if (! next_task_valid (next_status->task)) {
    target_mourn_inferior ();
    return 0;
  }

  next_inferior_check_stopped (next_status);
  GDB_CHECK (next_inferior_valid (next_status));

  if (inferior_ptrace_flag) {
    GDB_CHECK_FATAL (next_status->attached_in_ptrace);
    GDB_CHECK_FATAL (next_status->stopped_in_ptrace);
  } else {
    next_inferior_resume_ptrace (next_status, 0, PTRACE_DETACH);
    GDB_CHECK_FATAL (! next_status->attached_in_ptrace);
    GDB_CHECK_FATAL (! next_status->stopped_in_ptrace);
  }

  next_thread_list_lookup_by_info (next_status, pid, next_status->last_thread, &pret);
  return pret;
}

static void next_child_create_inferior (char *exec_file, char *allargs, char **env)
{
  fork_inferior (exec_file, allargs, env, next_ptrace_me, next_ptrace_him, NULL, NULL);
  if (inferior_pid == 0)
    return;

  next_clear_start_breakpoint ();
  if (inferior_auto_start_dyld_flag) {
    next_set_start_breakpoint (exec_bfd);
  }
  attach_flag = 0;

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

static void next_child_files_info (struct target_ops *ops)
{
  GDB_CHECK_FATAL (next_status != NULL);
  next_debug_inferior_status (next_status);
}

static char *next_mach_pid_to_str (int tpid)
{
  static char buf[128];
  int pid;
  thread_t thread;

  next_thread_list_lookup_by_id (next_status, tpid, &pid, &thread);
  sprintf (buf, "process %d thread 0x%lx", pid, (unsigned long) thread);
  return buf;
}

static int next_child_thread_alive (int tpid)
{
  int pid;
  thread_t thread;

  next_thread_list_lookup_by_id (next_status, tpid, &pid, &thread);
  GDB_CHECK_FATAL (pid == next_status->pid);

  return next_thread_valid (next_status->task, thread);
}

struct target_ops next_child_ops;

void 
_initialize_next_inferior ()
{
  struct cmd_list_element *cmd;

  GDB_CHECK_FATAL (next_status == NULL);
  next_status = (struct next_inferior_status *)
      xmalloc (sizeof (struct next_inferior_status));

  next_inferior_reset (next_status);

  dyld_init_paths (&next_status->dyld_status.path_info);
  dyld_objfile_info_init (&next_status->dyld_status.current_info);

#if WITH_CFM
  cfm_info_init (&next_status->cfm_info);
#endif /* WITH_CFM */

  next_child_ops = child_ops;
  child_ops.to_can_run = NULL;

  next_child_ops.to_shortname = "macos-child";
  next_child_ops.to_longname = "NeXT / Mac OS X child process";
  next_child_ops.to_doc = "NeXT / Mac OS X child process (started by the \"run\" command).";
  next_child_ops.to_attach = next_child_attach;
  next_child_ops.to_detach = next_child_detach;
  next_child_ops.to_create_inferior = next_child_create_inferior;
  next_child_ops.to_files_info = next_child_files_info;
  next_child_ops.to_wait = next_mach_wait;
  next_child_ops.to_mourn_inferior = next_mourn_inferior;
  next_child_ops.to_kill = next_kill_inferior_safe;
  next_child_ops.to_resume = next_child_resume;
  next_child_ops.to_thread_alive = next_child_thread_alive;
  next_child_ops.to_pid_to_str = next_mach_pid_to_str;
  next_child_ops.to_load = NULL;
  next_child_ops.to_xfer_memory = mach_xfer_memory;

  add_target (&next_child_ops);

  inferior_stderr = fdopen (fileno (stderr), "w+");
  inferior_debug ("GDB task: 0x%lx, pid: %d\n", task_self(), getpid());

  cmd = add_set_cmd ("inferior-bind-notify-port", class_obscure, var_boolean, 
		     (char *) &inferior_bind_notify_port_flag,
		     "Set if GDB should bind the task notify port.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("inferior-bind-exception-port", class_obscure, var_boolean, 
		     (char *) &inferior_bind_exception_port_flag,
		     "Set if GDB should bind the task exception port.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("inferior-handle-exceptions", class_obscure, var_boolean, 
		     (char *) &inferior_handle_exceptions_flag,
		     "Set if GDB should handle exceptions or pass them to the UNIX handler.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-handle-all-events", class_obscure, var_boolean, 
		     (char *) &inferior_handle_all_events_flag,
		     "Set if GDB should immediately handle all exceptions upon each stop, "
		     "or only the first received.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("inferior-ptrace", class_obscure, var_boolean, 
		     (char *) &inferior_ptrace_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
  
  cmd = add_set_cmd ("inferior-ptrace-on-attach", class_obscure, var_boolean, 
		     (char *) &inferior_ptrace_on_attach_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
  
  cmd = add_set_cmd ("inferior-auto-start-dyld", class_obscure, var_boolean, 
		     (char *) &inferior_auto_start_dyld_flag,
		     "Set if GDB will try to start dyld automatically.",
		     &setlist);
  add_show_from_set (cmd, &showlist);	

  cmd = add_set_cmd ("inferior-auto-start-dyld-on-attach", class_obscure, var_boolean, 
		     (char *) &inferior_auto_start_dyld_on_attach_flag,
		     "Set if GDB will try to start dyld automatically.",
		     &setlist);
  add_show_from_set (cmd, &showlist);	

  cmd = add_set_cmd ("inferior-handle-dyld-events-asynchronously", class_obscure, var_boolean, 
		     (char *) &inferior_handle_dyld_events_asynchronously_flag,
		     "Set if GDB will process dyld events during the wait loop.",
		     &setlist);
  add_show_from_set (cmd, &showlist);	

#if WITH_CFM
  cmd = add_set_cmd ("inferior-auto-start-cfm", class_obscure, var_boolean, 
		     (char *) &inferior_auto_start_cfm_flag,
		     "Set if GDB should enable debugging of CFM shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
#endif /* WITH_CFM */
}

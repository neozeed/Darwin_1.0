#include "nextstep-nat-dyld.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-dyld.h"
#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-dyld-process.h"

#if WITH_CFM
#include "nextstep-nat-cfm.h"
#endif /* WITH_CFM */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "annotate.h"
#include "mach-o.h"
#include "gdbcore.h"		/* for core_ops */

#include <assert.h>
#include <unistd.h>

#define DYLD_TRANSACTION_END ((enum dyld_event_type) -1)

bfd *sym_bfd;

extern struct target_ops exec_ops;

extern int inferior_auto_start_dyld_flag;
#if WITH_CFM
extern int inferior_auto_start_cfm_flag;
#endif /* WITH_CFM */

#if defined (__MACH30__)
static int dyld_use_event_server_flag = 1;
#else
static int dyld_use_event_server_flag = 0;
#endif

extern next_inferior_status *next_status;

#define DYLD_THREAD_FATAL(ret) \
{ \
  dyld_debug ("error on line %u of \"%s\" in function \"%s\": %s (0x%lx)\n", \
	      __LINE__, __FILE__, __MACH_CHECK_FUNCTION, MACH_ERROR_STRING (ret), ret); \
  abort (); \
}

static FILE *dyld_stderr = NULL;
static gdb_mutex_t dyld_stderr_mutex;

static int dyld_debug_flag = 0;

static int dyld_preload_libraries_flag = 1;

void dyld_debug (const char *fmt, ...)
{
  va_list ap;
  int ret;
  if (dyld_debug_flag) {
    ret = gdb_mutex_lock (&dyld_stderr_mutex);
    if (ret != 0) { abort (); }
    va_start (ap, fmt);
    fprintf (dyld_stderr, "[%d dyld]: ", getpid ());
    vfprintf (dyld_stderr, fmt, ap);
    va_end (ap);
    fflush (dyld_stderr);
    ret = gdb_mutex_unlock (&dyld_stderr_mutex);
    if (ret != 0) { abort (); }
  }
}

const char *dyld_debug_error_string (enum dyld_debug_return ret)
{
  switch (ret) {
  case DYLD_SUCCESS: return "DYLD_SUCCESS";
  case DYLD_INCONSISTENT_DATA: return "DYLD_INCONSISTENT_DATA";
  case DYLD_INVALID_ARGUMENTS: return "DYLD_INVALID_ARGUMENTS";
  case DYLD_FAILURE: return "DYLD_FAILURE";
  default: return "[UNKNOWN]";
  }  
}

const char *dyld_debug_event_string (enum dyld_event_type type)
{
  switch (type) {
  case DYLD_IMAGE_ADDED: return "DYLD_IMAGE_ADDED";
  case DYLD_MODULE_BOUND: return "DYLD_MODULE_BOUND";
  case DYLD_MODULE_REMOVED: return "DYLD_MODULE_REMOVED";
  case DYLD_MODULE_REPLACED: return "DYLD_MODULE_REPLACED";
  case DYLD_PAST_EVENTS_END: return "DYLD_PAST_EVENTS_END";
  case DYLD_TRANSACTION_END: return "DYLD_TRANSACTION_END";
  default: return "[UNKNOWN]";
  }
}

static void debug_dyld_event_request (struct _dyld_event_message_request *request)
{
  next_debug_message (&request->head);
  dyld_debug ("               type: %s (0x%lx)\n", 
	      dyld_debug_event_string (request->event.type),
	      (unsigned long) request->event.type);
  dyld_debug ("arg[0].vmaddr_slide: 0x%lx\n", (unsigned long) request->event.arg[0].vmaddr_slide);
  dyld_debug ("arg[0].module_index: 0x%lx\n", (unsigned long) request->event.arg[0].module_index);
  dyld_debug ("arg[1].vmaddr_slide: 0x%lx\n", (unsigned long) request->event.arg[1].vmaddr_slide);
  dyld_debug ("arg[1].module_index: 0x%lx\n", (unsigned long) request->event.arg[1].module_index);
}

void dyld_print_status_info (struct next_dyld_thread_status *s) 
{
  switch (s->state) {
  case dyld_clear:
    printf_filtered ("The DYLD shared library state has not yet been initialized.\n"); 
    break;
  case dyld_initialized:
    printf_filtered 
      ("The DYLD shared library state has been initialized from the "
       "executable's shared library information.  All symbols should be "
       "present, but the addresses of some symbols may move when the program "
       "is executed, as DYLD may relocate library load addresses if "
       "necessary.\n");
    break;
  case dyld_started:
    printf_filtered ("DYLD shared library information has been read from the DYLD debugging thread.\n");
    break;
  default:
    internal_error ("invalid value for s->dyld_state");
    break;
  }

  if (s->start_error) {
    printf_filtered ("\nAn error occurred trying to start the DYLD debug thread.\n");
  }

  dyld_print_shlib_info (&s->current_info);
}

void next_dyld_thread_init (next_dyld_thread_status *s)
{
  s->transmit_thread = PORT_NULL;
  s->receive_thread = PORT_NULL;

  s->port = NULL;
  s->start_error = 0;
  s->state = dyld_clear;

  s->requests = xmalloc (1000 * sizeof (struct _dyld_event_message_request));
  s->max_requests = 1000;
  s->start_request = 0;
  s->end_request = 0;

  gdb_mutex_init (&s->queue_mutex, NULL);
  gdb_cond_init (&s->nonempty_condition, NULL);

  gdb_mutex_init (&s->mutex, NULL);
}

void next_dyld_thread_destroy (struct next_inferior_status *ns, struct next_dyld_thread_status *s)
{
  int ret;

#if 0
  struct dyld_objfile_info saved_info, new_info;

  dyld_objfile_info_init (&saved_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&saved_info, &s->current_info);
  dyld_objfile_info_free (&s->current_info);

  dyld_update_shlibs (ns, &ns->dyld_status.path_info, &saved_info, &new_info, &s->current_info);

  dyld_objfile_info_free (&saved_info);
  dyld_objfile_info_free (&new_info);
#endif

  /* The dyld port must be removed on detaching, or the inferior 
     will hang trying to talk to this port. */

  if (s->port != PORT_NULL) {
    port_deallocate (task_self (), s->port);
    s->state = dyld_clear;
    s->port = PORT_NULL;
  }

  if (s->receive_thread != THREAD_NULL) {
    gdb_thread_kill (s->receive_thread);
    s->receive_thread = NULL;
  }

  if (s->transmit_thread != THREAD_NULL) {
    gdb_thread_kill (s->transmit_thread);
    s->transmit_thread = NULL;
  }

  gdb_mutex_destroy (&s->queue_mutex);
  /* assert (ret == 0); */

  gdb_cond_destroy (&s->nonempty_condition);
  /* assert (ret == 0); */

  ret = gdb_mutex_destroy (&s->mutex);
  assert (ret == 0);
}

#if ! defined (__MACH30__)

static void dyld_process_message_local
(struct _dyld_event_message_request *request, 
 struct _dyld_event_message_reply *reply)
{
  reply->head.msg_simple = 1;
  reply->head.msg_size = sizeof (struct _dyld_event_message_reply);
  reply->head.msg_type = request->head.msg_type;
  reply->head.msg_local_port = PORT_NULL;
  reply->head.msg_remote_port = request->head.msg_remote_port;
  reply->head.msg_id = request->head.msg_id + 100;

  reply->RetCodeType.msg_type_name = MSG_TYPE_INTEGER_32;
  reply->RetCodeType.msg_type_size = sizeof (int) * 8;
  reply->RetCodeType.msg_type_number = 1;
  reply->RetCodeType.msg_type_inline = 1;
  reply->RetCodeType.msg_type_longform = 0;
  reply->RetCodeType.msg_type_deallocate = 0;

  if (request->head.msg_id != 200) {
    reply->RetCode = MIG_BAD_ID; 
    return; 
  }

  if ((request->head.msg_size != 56) ||
      (request->head.msg_simple != TRUE)) {
    reply->RetCode = MIG_BAD_ARGUMENTS;
    return;
  }

  if ((request->eventType.msg_type_inline != TRUE) ||
      (request->eventType.msg_type_longform != FALSE) ||
      (request->eventType.msg_type_name != MSG_TYPE_INTEGER_32) ||
      (request->eventType.msg_type_number != 7) ||
      (request->eventType.msg_type_size != 32)) {
    reply->RetCode = MIG_BAD_ARGUMENTS;
    return;
  }

  reply->RetCode = KERN_SUCCESS;
}

#endif /* ! __MACH30__ */

#if defined (__MACH30__)

static dyld_process_message_server
(struct _dyld_event_message_request *request,
 struct _dyld_event_message_reply *reply)
{
  _dyld_event_server (&request->head, &reply->head);
}

kern_return_t
_dyld_event_server_callback (port_t subscriber, struct dyld_event event)
{
  return KERN_SUCCESS;
}

#else /* ! __MACH30__ */ 

static void dyld_process_message_server
(struct _dyld_event_message_request *request,
 struct _dyld_event_message_reply *reply)
{
   _dyld_event_server (request, reply);
}

void
_dyld_event_server_callback (port_t subscriber, struct dyld_event event)
{
  return;
}

#if defined (__DYNAMIC__)
/* The following symbol is referenced by libsys symbolically (instead of
   through undefined reference). To get strip(1) to know this symbol is not
   to be stripped it needs to have the REFERENCED_DYNAMICALLY bit
   (0x10) set. This would have been done automaticly by ld(1) if this symbol
   were referenced through an undefined symbol. */
asm (".desc __dyld_event_server_callback, 0x10");
#endif /* __DYNAMIC__ */

#endif /* __MACH30__ */

static void dyld_process_message
(struct _dyld_event_message_request *request,
 struct _dyld_event_message_reply *reply)
{
#ifndef __MACH30__
  if (dyld_use_event_server_flag) {
    dyld_process_message_server (request, reply);
  } else {
    dyld_process_message_local (request, reply);
  }
#else
  dyld_process_message_server (request, reply);
#endif

}

static void next_mach_update_dyld (struct next_inferior_status *s, int expect_end)
{
  struct target_waitstatus ws;
  next_mach_process_dyld_messages (&ws, NULL, expect_end);
}

static void next_dyld_thread_enqueue 
(next_dyld_thread_status *s, struct _dyld_event_message_request *r)
{
  unsigned int new_end_request;
  gdb_mutex_lock (&s->queue_mutex);
  new_end_request = (s->end_request + 1) % s->max_requests;
  if (new_end_request == s->start_request) {
    /* overflow */
    abort ();
  } 
  s->requests[s->end_request] = *r;
  s->end_request = new_end_request;
  gdb_cond_signal (&s->nonempty_condition);
  gdb_mutex_unlock (&s->queue_mutex);
}

static struct _dyld_event_message_request next_dyld_thread_dequeue (next_dyld_thread_status *s)
{
  struct _dyld_event_message_request request;
  gdb_mutex_lock (&s->queue_mutex);
  while (s->start_request == s->end_request) {
    gdb_cond_wait (&s->nonempty_condition, &s->queue_mutex);
  }
  assert (s->start_request != s->end_request);
  request = s->requests[s->start_request];
  s->start_request = (s->start_request + 1) % s->max_requests;
  gdb_mutex_unlock (&s->queue_mutex);
  
  return request;
}

static void next_dyld_transmit_thread (void *arg)
{
  struct _dyld_event_message_request request;
  next_dyld_thread_status *s;
  kern_return_t kret;

  s = (next_dyld_thread_status *) arg;
  assert (s != NULL);

  for (;;) { 
    
    request = next_dyld_thread_dequeue (s);
    dyld_debug ("forwarding message from buffer to gdb: %s (0x%lx)\n",
		dyld_debug_event_string (request.event.type), request.event.type);
    /* debug_dyld_event_request (&request); */

#if defined (__MACH30__)
    request.head.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0);
    request.head.msgh_size = sizeof (struct _dyld_event_message_request);
    request.head.msgh_remote_port = next_status->dyld_port;
    request.head.msgh_local_port = MACH_PORT_NULL;
    request.head.msgh_reserved = 0;
    kret = mach_msg (&request.head, MACH_SEND_MSG, sizeof (struct _dyld_event_message_request), 0,
		    MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
#else /* ! __MACH30__ */
    request.head.msg_simple = 1;
    request.head.msg_size = sizeof (struct _dyld_event_message_request);
    request.head.msg_type = MSG_TYPE_NORMAL;
    request.head.msg_id = 0;
    request.head.msg_remote_port = next_status->dyld_port;
    request.head.msg_local_port = PORT_NULL;
    kret = msg_send (&request.head, 0, 0);
#endif /* __MACH30__ */

    if (kret != KERN_SUCCESS) {
      DYLD_THREAD_FATAL (kret);
    }
  }
}

static void next_dyld_receive_thread (void *arg)
{
  struct {
    struct _dyld_event_message_request request;
#if defined (__MACH30__)        
    unsigned char trailer[MAX_TRAILER_SIZE];
#endif        
  } event_message;

  struct _dyld_event_message_reply reply;

  next_dyld_thread_status *s;
  kern_return_t kret;

  s = (next_dyld_thread_status *) arg;
  assert (s != NULL);

  for (;;) {

#if defined (__MACH30__)

    kret = mach_msg (&event_message.request.head, MACH_RCV_MSG, 0, sizeof (event_message),
		     s->port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

#else /* ! __MACH30__ */

    event_message.request.head.msg_local_port = s->port;
    event_message.request.head.msg_size = sizeof (event_message);
    
    kret = msg_receive (&event_message.request.head, MSG_OPTION_NONE, 0);

#endif /* __MACH30__ */
    
    MACH_CHECK_ERROR (kret);
    
    registers_changed ();
    dyld_process_message (&event_message.request, &reply);
    registers_changed ();

    dyld_debug ("received and queued message from dyld: %s (0x%lx)\n",
		dyld_debug_event_string (event_message.request.event.type), event_message.request.event.type);
    next_dyld_thread_enqueue (s, &event_message.request);
    
#if defined (__MACH30__)
    /* event message is one-way - no reply needed */
#else /* ! __MACH30__ */
    if (event_message.request.event.type != DYLD_TRANSACTION_END) {
      dyld_debug ("acknowledging message from dyld: %s (0x%lx)\n",
		  dyld_debug_event_string (event_message.request.event.type),
		  event_message.request.event.type);
      kret = msg_send (&reply.head, SEND_TIMEOUT, 0);
      MACH_CHECK_ERROR (kret);
    }
#endif /* __MACH30__ */
  }
}

static void next_dyld_restore_runnable (PTR arg)
{
  struct _dyld_debug_task_state *state = (struct _dyld_debug_task_state *) arg;
  assert (next_status != NULL);
  _dyld_debug_restore_runnable (next_status->task, state);
}

void dyld_error_handler (struct dyld_debug_error_data *e)
{
  fprintf (stderr, "Error on line %lu of \"%s\"", e->line_number, e->file_name);
  if (e->mach_error != 0) {
    fprintf (stderr, "; mach error \"%s\"", mach_error_string (e->mach_error));
  }
  if (e->dyld_debug_errno != 0) {
    fprintf (stderr, "; unix error \"%s\"", strerror (e->dyld_debug_errno));
  }
  fprintf (stderr, "; local error %lu", e->local_error);
  fprintf (stderr, "\n");
}

int next_mach_start_dyld (struct next_inferior_status *s)
{
  enum dyld_debug_return dret;
  kern_return_t kret;

  _dyld_debug_set_error_func (dyld_error_handler);

  dyld_debug ("next_mach_start_dyld: starting inferior dyld_debug thread\n");

  assert (s->dyld_status.state != dyld_started);
  s->dyld_status.start_error = 1;

#if defined (__MACH30__)

  kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->dyld_status.port);
  MACH_CHECK_ERROR (kret);

  kret = mach_port_insert_right (mach_task_self(), s->dyld_status.port, s->dyld_status.port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

#else

  kret = port_allocate (task_self (), &s->dyld_status.port);
  MACH_CHECK_ERROR (kret);

  kret = port_set_backlog (task_self (), s->dyld_status.port, PORT_BACKLOG_MAX);
  MACH_CHECK_ERROR (kret);

#endif
  
  s->dyld_status.receive_thread =
    gdb_thread_fork ((gdb_thread_fn_t) &next_dyld_receive_thread, &s->dyld_status);
  s->dyld_status.transmit_thread =
    gdb_thread_fork ((gdb_thread_fn_t) &next_dyld_transmit_thread, &s->dyld_status);
  s->dyld_status.start_error = 0;
  s->dyld_status.state = dyld_started;

  registers_changed ();
  dret = _dyld_debug_add_event_subscriber (s->task, 0, 120000, 0, s->dyld_status.port);
  registers_changed ();

  /* Check for new threads created by dyld.  This would probably be
     better done elsewhere. */

  next_mach_check_new_threads ();

  if (dret != DYLD_SUCCESS) {
    warning ("next_mach_start_dyld: failed to start inferior dyld_debug thread: %s (%d)\n",
             dyld_debug_error_string (dret), dret);
    return -1;
  } else {
    dyld_debug ("next_mach_start_dyld: started inferior dyld_debug thread\n");
  }

  registers_changed ();
  next_mach_update_dyld (s, 1);
  registers_changed ();
  
#if WITH_CFM
  if (inferior_auto_start_cfm_flag) {
    next_init_cfm_info_api (next_status);
  }
#endif /* WITH_CFM */

  return 0;
}

void next_clear_start_breakpoint ()
{
  remove_solib_event_breakpoints ();
}

void next_set_start_breakpoint (bfd *exec_bfd)
{
  struct breakpoint *b;

  struct symtab_and_line sal;

  if ((exec_bfd == NULL) || (bfd_get_start_address (exec_bfd) == (CORE_ADDR) -1)) {
    warning ("next_set_start_breakpoint: unable to determine entry point of executable\n");
    return;
  }
    
  INIT_SAL (&sal);
  sal.pc = bfd_get_start_address (exec_bfd);

  b = set_momentary_breakpoint (sal, NULL, bp_shlib_event);
  
  b->disposition = donttouch;
  b->thread = -1;

  b->addr_string = strsave ("start");

  breakpoints_changed ();
}

static void info_dyld_command (args, from_tty)
     char *args;
     int from_tty;
{
  assert (next_status != NULL);
  dyld_print_status_info (&next_status->dyld_status);
}

void next_mach_try_start_dyld ()
{
  assert (next_status != NULL);

  if (! inferior_auto_start_dyld_flag) {
    return; 
  }

  /* Don't to start DYLD debug thread in the kernel */
  if (strcmp (current_target.to_shortname, "remote-kdp") == 0) {
    return;
  }

  /* DYLD debug thread has already been started */
  if (next_status->dyld_status.state == dyld_started) {
    return;
  }

  if (next_status->dyld_status.start_error) {
    return;
  }

  next_mach_start_dyld (next_status);
}

void next_mach_add_shared_symbol_files ()
{
  struct dyld_objfile_info *result = NULL;
  
  assert (next_status != NULL);
  result = &next_status->dyld_status.current_info;

  dyld_load_libraries (&next_status->dyld_status.path_info, result);

  dyld_merge_section_tables (result);
  dyld_update_section_tables (result, &current_target);
  dyld_update_section_tables (result, &exec_ops);

  reread_symbols ();
  breakpoint_re_set ();
  re_enable_breakpoints_in_shlibs (0);
}

void next_init_dyld (struct next_inferior_status *s, bfd *sym_bfd)
{
  struct dyld_objfile_info previous_info, new_info;

  dyld_init_paths (&s->dyld_status.path_info);

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &s->dyld_status.current_info);
  dyld_objfile_info_free (&s->dyld_status.current_info);

  if (dyld_preload_libraries_flag) {
    dyld_add_inserted_libraries (&s->dyld_status.current_info, &s->dyld_status.path_info);
    if (sym_bfd != NULL) {
      dyld_add_image_libraries (&s->dyld_status.current_info, sym_bfd);
    }
  }

  dyld_update_shlibs (s, &s->dyld_status.path_info,
		      &previous_info, &s->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&previous_info);
  
  dyld_objfile_info_copy (&s->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&new_info);

  s->dyld_status.state = dyld_initialized;
}

void next_init_dyld_symfile (bfd *sym_bfd)
{
  assert (next_status != NULL);
  next_init_dyld (next_status, sym_bfd);
}

static void next_dyld_init_command (args, from_tty)
     char *args;
     int from_tty;
{
  assert (next_status != NULL);
  next_init_dyld (next_status, sym_bfd);
}

static void next_dyld_start_command (char *args, int from_tty)
{
  assert (next_status != NULL);
  next_mach_start_dyld (next_status);
}

static void next_dyld_update_command (char *args, int from_tty)
{
  assert (next_status != NULL);
  next_mach_update_dyld (next_status, 0);
}

kern_return_t next_mach_process_dyld_messages
(struct target_waitstatus *status, struct _dyld_event_message_request *initial_message, int expect_end)
{
  struct dyld_objfile_info previous_info, new_info;
  struct _dyld_debug_task_state state;
  struct cleanup *state_cleanup;
  kern_return_t kret;

  assert (next_status != NULL);

  if (next_status->dyld_status.state != dyld_started) {
    return 0;
  }

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &next_status->dyld_status.current_info);

  if (initial_message != NULL) {
    dyld_process_event (&next_status->dyld_status.current_info, &initial_message->event);
  }
       
  if (! expect_end) {

    /* send message to loopback port */
    
    struct _dyld_event_message_request r;
    dyld_debug ("sending DYLD_TRANSACTION_END message to loopback port\n");

    r.event.type = DYLD_TRANSACTION_END;
#if defined (__MACH30__)
    r.head.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0);
    r.head.msgh_size = sizeof (struct _dyld_event_message_request);
    r.head.msgh_remote_port = next_status->dyld_status.port;
    r.head.msgh_local_port = MACH_PORT_NULL;
    r.head.msgh_reserved = 0;
    kret = mach_msg (&r.head, MACH_SEND_MSG, sizeof (struct _dyld_event_message_request), 0,
		     MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
#else /* ! __MACH30__ */
    r.head.msg_simple = 1;
    r.head.msg_size = sizeof (struct _dyld_event_message_request);
    r.head.msg_type = MSG_TYPE_NORMAL;
    r.head.msg_id = 0;
    r.head.msg_remote_port = next_status->dyld_status.port;
    r.head.msg_local_port = PORT_NULL;
    kret = msg_send (&r.head, 0, 0);
#endif /* __MACH30__ */
    MACH_CHECK_ERROR (kret);
  }

  for (;;) {

    char message_buffer[512];
    struct _dyld_event_message_request *request = (struct _dyld_event_message_request *) message_buffer;

#if defined (__MACH30__)
    kret = mach_msg (&request->head, MACH_RCV_MSG, 0, sizeof (message_buffer),
		     next_status->dyld_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
#else /* ! __MACH30__ */
    request->head.msg_local_port = next_status->dyld_port;
    request->head.msg_size = sizeof (struct _dyld_event_message_request);
    kret = msg_receive (&request->head, RCV_LARGE, 0);
#endif /* __MACH30__ */

    MACH_CHECK_ERROR (kret);

#if defined (__MACH30__)
    if (dyld_debug_flag && (request->head.msgh_size != sizeof (struct _dyld_event_message_request))) {
      warning ("message header has incorrect size; ignoring");
    }
#endif /* __MACH30__ */

    dyld_debug ("message received by gdb: %s (0x%lx)\n",
		dyld_debug_event_string (request->event.type), request->event.type);
    /* debug_dyld_event_request (&request); */

    if ((request->event.type == DYLD_PAST_EVENTS_END)
	|| (request->event.type == DYLD_TRANSACTION_END)) {
      break;
    }

    dyld_process_event (&next_status->dyld_status.current_info, &request->event);
  }

  dyld_debug ("all pending dyld messages cleared\n");

  if (next_status->dyld_status.current_info.nents == previous_info.nents) {
    dyld_objfile_info_free (&previous_info);
    return KERN_SUCCESS;
  }

  dyld_debug ("processing new library\n");

  registers_changed ();
  dyld_debug ("making runnable\n");

  _dyld_debug_make_runnable (next_status->task, &state);
  state_cleanup = make_cleanup (next_dyld_restore_runnable, (PTR) &state);

  dyld_update_shlibs (next_status, &next_status->dyld_status.path_info,
		      &previous_info, &next_status->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&previous_info);
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&new_info);
    
  _dyld_debug_restore_runnable (next_status->task, &state);
  discard_cleanups (state_cleanup);

  dyld_debug ("restored runnable\n");
  registers_changed ();

  if (status != NULL) {
    status->kind = TARGET_WAITKIND_LOADED;
  }
  return KERN_SUCCESS;
}

static void dyld_cache_purge_comand (char *exp, int from_tty)
{
  assert (next_status != NULL);
  dyld_purge_cached_libraries (&next_status->dyld_status.current_info);
}

void
_initialize_nextstep_nat_dyld ()
{
  struct cmd_list_element *cmd;
  int ret;

  dyld_stderr = fdopen (fileno (stderr), "w+");
  ret = gdb_mutex_init (&dyld_stderr_mutex, NULL);
  if (ret != 0) { abort (); }

  add_com ("dyld-start", class_run, next_dyld_start_command,
	   "Start the DYLD debugging thread.");

  add_com ("dyld-init", class_run, next_dyld_init_command,
	   "Init DYLD libraries to initial guesses.");

  add_com ("dyld-update", class_run, next_dyld_update_command,
	   "Process all pending DYLD events.");

#ifndef __MACH30__
  cmd = add_set_cmd ("dyld-use-event-server", class_obscure, var_boolean, 
		     (char *) &dyld_use_event_server_flag,
		     "Set if GDB should the system-provided DYLD event server.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
#endif

  cmd = add_set_cmd ("dyld-preload-libraries", class_obscure, var_boolean, 
		     (char *) &dyld_preload_libraries_flag,
		     "Set if GDB should pre-load symbols for DYLD libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("debug-dyld", class_obscure, var_boolean, 
		     (char *) &dyld_debug_flag,
		     "Set if printing dyld debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  add_info ("dyld", info_dyld_command,
	    "Show current DYLD state.");

  add_info ("sharedlibrary", info_dyld_command,
	    "Show current DYLD state.");

  add_com ("dyld-cache-purge", class_obscure, dyld_cache_purge_comand,
	   "Purge all symbols for DYLD images cached by GDB.");
}

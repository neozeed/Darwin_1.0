#include <mach/mach.h>

/* this is to placate bfd */
#define TRUE_FALSE_ALREADY_DEFINED

#include <assert.h>

#include "defs.h"
#include "breakpoint.h"
#include "gdbcmd.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-cfm.h"

static void enable_breakpoints_in_containers(void);

static CFMClosure *gClosures = NULL;

extern OSStatus CCFM_SetInfoSPITarget
(MPProcessID targetProcess, void *targetCFMHook, mach_port_t notifyPort);

static void
output_data (unsigned char *buf, size_t len)
{
  size_t i;

  printf_filtered ("0x%lx\n", len);
  for (i = 0; i < len; i++) {
    if ((i % 8) != 0) {
      printf_filtered (" ");
    }
    printf_filtered ("0x%02x", buf[i]);
    if (((i + 1) % 8) == 0) {
      printf_filtered ("\n");
    }
  }
  if (((i + 1) % 8) != 0) {
    printf_filtered ("\n");
  }
}

static void
annotate_closure_begin (CFragClosureInfo *closure)
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032closure-begin\n");
      output_data ((unsigned char *) closure, sizeof (*closure));
    }
}

static void
annotate_closure_end (CFragClosureInfo *closure)
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032closure-end\n");
    }
}

static void
annotate_connection (CFragConnectionInfo *connection)
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032connection\n");
      output_data ((unsigned char *) connection, sizeof (*connection));
    }
}

static void
annotate_container (CFragContainerInfo *container)
{
  printf_filtered 
    ("Noted CFM object \"%.*s\" at 0x%lx\n",
     container->name[0], &container->name[1],
     (unsigned long) container->address);

  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032container\n");
      output_data ((unsigned char *) container, sizeof (*container));
    }
}

static void
annotate_section (ItemCount index, CFragSectionInfo *section)
{
  if (annotation_level > 1)
    {
      printf_filtered ("\n\032\032section\n");
      output_data ((unsigned char *) section, sizeof (*section));
    }
}

void
cfm_info_init (next_cfm_info *cfm_info)
{
  cfm_info->info_api_cookie = NULL;
  cfm_info->cfm_send_right = MACH_PORT_NULL;
  cfm_info->cfm_receive_right = MACH_PORT_NULL;

  gClosures = NULL;
}

void
next_init_cfm_info_api (struct next_inferior_status *status)
{
  kern_return_t	kret;
  next_cfm_info *i;
  
  i = &status->cfm_info;

  if (status->cfm_info.cfm_receive_right != MACH_PORT_NULL)
    return;

  if (status->cfm_info.info_api_cookie == NULL)
    return;

  if (status->task == MACH_PORT_NULL)
    return;

  kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &i->cfm_receive_right);
  MACH_CHECK_ERROR (kret);
	
  kret = mach_port_allocate (status->task, MACH_PORT_RIGHT_RECEIVE, &i->cfm_send_right);
  MACH_CHECK_ERROR (kret);
	
  kret = mach_port_destroy (status->task, i->cfm_send_right);
  MACH_CHECK_ERROR (kret);

  kret = mach_port_insert_right (status->task, i->cfm_send_right, i->cfm_receive_right, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

  kret = CCFM_SetInfoSPITarget ((MPProcessID) status->task, i->info_api_cookie, i->cfm_send_right);
  MACH_CHECK_ERROR (kret);
}

void
next_handle_cfm_event (struct next_inferior_status* status, void* messageData)
{
#if !defined(__MACH30__)    
    msg_header_t		reply;
    msg_header_t*		message = (msg_header_t*) messageData;
#else
    mach_msg_header_t	reply;
    mach_msg_header_t*	message = (mach_msg_header_t*) messageData;
#endif    
    CFragNotifyInfo*	messageBody = (CFragNotifyInfo*) (message + 1);
    kern_return_t		result;
    ItemCount			numberOfConnections, totalCount, index;
    CFragConnectionID*	connections;
    CFragClosureInfo	closureInfo;
    CFMClosure*			closure;
    
    gdb_flush (gdb_stdout);	/* clean everything out before out annotations are printed */
    
    assert ((kCFragPrepareClosureNotify == messageBody->notifyKind) ||
            (kCFragReleaseClosureNotify == messageBody->notifyKind));

    result = CFragGetClosureInfo(messageBody->u.closureInfo.closureID,
                                 kCFragClosureInfoVersion,
                                 &closureInfo);
    assert (noErr == result);
    annotate_closure_begin(&closureInfo);

    closure = xcalloc(1, sizeof(CFMClosure));
    assert (NULL != closure);
    closure->mClosure = closureInfo;

    result = CFragGetConnectionsInClosure(closureInfo.closureID,
                                          0,
					  0,
                                          &numberOfConnections,
                                          NULL);
    assert (noErr == result);

    connections = xmalloc(numberOfConnections * sizeof(CFragConnectionID));
    assert (NULL != connections);

    result = CFragGetConnectionsInClosure(closureInfo.closureID,
                                          numberOfConnections,
					  0,
                                          &totalCount,
                                          connections);
    assert (noErr == result);
    assert (totalCount == numberOfConnections);

    for (index = 0; index < numberOfConnections; ++index)
    {
        CFragConnectionInfo	connectionInfo;
        CFragContainerInfo	containerInfo;
        ItemCount			sectionIndex;
        CFMConnection*		connection;
        CFMContainer*		container;
        
        result = CFragGetConnectionInfo(connections[index],
                                        kCFragConnectionInfoVersion,
                                        &connectionInfo);
        assert (noErr == result);
        annotate_connection(&connectionInfo);

        connection = xcalloc(1, sizeof(CFMConnection));
        assert (NULL != connection);
        connection->mConnection = connectionInfo;
        connection->mNext = closure->mConnections;
        closure->mConnections = connection;

        result = CFragGetContainerInfo(connectionInfo.containerID,
                                       kCFragContainerInfoVersion,
                                       &containerInfo);
        assert (noErr == result);
        annotate_container(&containerInfo);

        container = xcalloc(1, sizeof(CFMContainer));
        assert (NULL != container);
        container->mContainer = containerInfo;
        connection->mContainer = container;

        for (sectionIndex = 0; sectionIndex < containerInfo.sectionCount; ++sectionIndex)
        {
            CFragSectionInfo	sectionInfo;
            CFMSection*			section;
            
            result = CFragGetSectionInfo(connections[index],
                                         sectionIndex,
                                         kCFragSectionInfoVersion,
                                         &sectionInfo);
            assert (noErr == result);
            annotate_section(sectionIndex, &sectionInfo);

            section = xcalloc(1, sizeof(CFMSection));
            assert (NULL != section);
            section->mSection = sectionInfo;
            section->mContainer = container;
            section->mNext = container->mSections;
            container->mSections = section;
        }
    }

    xfree (connections);

    annotate_closure_end(&closureInfo);

    closure->mNext = gClosures;
    gClosures = closure;
       
    next_inferior_suspend_mach(status);
    enable_breakpoints_in_containers();

    bzero(&reply, sizeof(reply));
#if !defined (__MACH30__)    
    reply.msg_simple = TRUE;
    reply.msg_size = sizeof(reply);
    reply.msg_remote_port = message->msg_remote_port;
    reply.msg_local_port = MACH_PORT_NULL;

    result = msg_send(&reply, MSG_OPTION_NONE, MACH_MSG_TIMEOUT_NONE);
#else
    reply.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    reply.msgh_remote_port = message->msgh_remote_port;
    reply.msgh_local_port = MACH_PORT_NULL;

    result = mach_msg(&reply,
                      (MACH_SEND_MSG | MACH_MSG_OPTION_NONE),
                      sizeof(reply),
                      0,
                      MACH_PORT_NULL,
                      MACH_MSG_TIMEOUT_NONE,
                      MACH_PORT_NULL);
#endif    
    MACH_CHECK_ERROR(result);
}

void enable_breakpoints_in_containers (void)
{
  struct breakpoint *b;
  extern struct breakpoint* breakpoint_chain;

  for (b = breakpoint_chain; b; b = b->next) {

    if (b->enable != disabled) { break; } 
    if (strncmp (b->addr_string, "@metrowerks:", strlen ("@metrowerks:")) != 0) { break; } 

    {
      char **argv;
      unsigned long section;
      unsigned long offset;
      CFMContainer *cfmContainer;

      argv = buildargv (b->addr_string + strlen ("@metrowerks:"));
      if (argv == NULL) { 
	nomem (0); 
      } 
      
      section = strtoul (argv[1], NULL, 16); 
      offset = strtoul (argv[2], NULL, 16); 

      cfmContainer = CFM_FindContainerByName (argv[0], strlen (argv[0]));
      if (cfmContainer != NULL) {
	CFMSection *cfmSection = CFM_FindSection(cfmContainer, kCFContNormalCode);
	if (cfmSection != NULL) {
	  b->address = cfmSection->mSection.address + offset;
	  b->enable = enabled;
	  b->addr_string = xmalloc (64);
	  sprintf (b->addr_string, "*0x%lx", (unsigned long) b->address);
	}
      }
    }
  }
}

CFMContainer *CFM_FindContainerByName (char *name, int length)
{
    CFMContainer*	found = NULL;
    CFMClosure* 	closure = gClosures;
    
    while ((NULL == found) && (NULL != closure))
    {
        CFMConnection* connection = closure->mConnections;
        while ((NULL == found) && (NULL != connection))
        {
            if (NULL != connection->mContainer)
            {
                CFMContainer* container = connection->mContainer;
                if (0 == memcmp(name,
                                &container->mContainer.name[1],
                                min(container->mContainer.name[0], length)))
                {
                    found = container;
                    break;
                }
            }

            connection = connection->mNext;
        }

        closure = closure->mNext;
    }

    return found;
}

CFMSection *CFM_FindSection (CFMContainer *container, CFContMemoryAccess accessType)
{
  int done = false;
  CFMSection *section = container->mSections;

  while (!done && (NULL != section))
    {
      CFContMemoryAccess access = section->mSection.access;
      if ((access & accessType) == accessType)
        {
	  done = true;
	  break;
        }
        
      section = section->mNext;
    }

  return (done ? section : NULL);
}

CFMSection *CFM_FindContainingSection (CORE_ADDR address)
{
  CFMSection*	section = NULL;
  CFMClosure*	closure = gClosures;

  while ((NULL == section) && (NULL != closure))
    {
      CFMConnection* connection = closure->mConnections;
      while ((NULL == section) && (NULL != connection))
        {
	  CFMContainer*	container = connection->mContainer;
	  CFMSection*		testSection = container->mSections;
	  while ((NULL == section) && (NULL != testSection))
            {
	      if ((address >= testSection->mSection.address) &&
		  (address < (testSection->mSection.address + testSection->mSection.length)))
                {
		  section = testSection;
		  break;
                }
                
	      testSection = testSection->mNext;
            }
            
	  connection = connection->mNext;
        }

      closure = closure->mNext;
    }

  return section;
}

static void info_cfm_command (args, from_tty)
     char *args;
     int from_tty;
{
  CFMClosure *closure = gClosures;
  while (closure != NULL) {

    CFMConnection *connection = closure->mConnections;
    annotate_closure_begin (&closure->mClosure);

    while (connection != NULL) {
      
      CFMContainer *container = connection->mContainer;
      CFMSection *section = container->mSections;
      size_t secnum = 0;

      annotate_container (&container->mContainer);
      annotate_connection (&connection->mConnection);

      while (section != NULL) {
	annotate_section (secnum, &section->mSection);
	secnum++;
	section = section->mNext;
      }
      
      connection = connection->mNext;
    }

    annotate_closure_end (&closure->mClosure);

    closure = closure->mNext;
  }

  return;
}

void
_initialize_nextstep_nat_cfm ()
{
  add_info ("cfm", info_cfm_command,
	    "Show current CFM state.");
}

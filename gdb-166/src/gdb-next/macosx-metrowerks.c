#pragma options align=mac68k
#include <CodeFragmentInfoPriv.h>
#pragma options align=reset

#define TRUE_FALSE_ALREADY_DEFINED

#include <assert.h>

#include "defs.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "frame.h"
#include "breakpoint.h"
#include "symtab.h"
#include "annotate.h"
#include "target.h"
#include "breakpoint.h"
#include "gdbcore.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-cfm.h"
#include "nextstep-nat-dyld-info.h"

#define IS_BC_x(instruction) \
((((instruction) & 0xFC000000) >> 26) == 16)

#define IS_B_x(instruction) \
((((instruction) & 0xFC000000) >> 26) == 18)

#define IS_BCLR_x(instruction) \
(((((instruction) & 0xFC000000) >> 26) == 19) && ((((instruction) & 0x000007FE) >> 1) == 16))

#define IS_BCCTR_x(instruction) \
(((((instruction) & 0xFC000000) >> 26) == 19) && ((((instruction) & 0x000007FE) >> 1) == 528))

#define WILL_LINK(instruction) \
(((instruction) & 0x00000001) != 0)

static unsigned long mread (CORE_ADDR addr, unsigned long len, unsigned long mask)
{
  long ret = read_memory_unsigned_integer (addr, len);
  if (mask) { ret &= mask; }
  return ret;
}

static struct breakpoint *
set_momentary_breakpoint_address (CORE_ADDR addr)
{
  struct symtab_and_line sal;
  struct breakpoint *breakpoint;

  INIT_SAL (&sal);
  sal.pc = addr;

  breakpoint = set_momentary_breakpoint (sal, NULL, bp_breakpoint);
  breakpoint->disposition = del;
  breakpoint->addr_string = xmalloc (64);
  sprintf (breakpoint->addr_string, "*0x%lx", (unsigned long) addr);

  return breakpoint;
}

static void
metrowerks_step_command (char *args, int from_tty)
{
  CORE_ADDR range_start = 0;
  CORE_ADDR range_stop = 0;
  CORE_ADDR pc = 0;
  int step_into = 0;
  int single_instruction = 0;

  range_start = strtoul (args, &args, 16);
  range_stop = strtoul (args, &args, 16);
  step_into = strtoul (args, &args, 16);

  if (! target_has_execution)
    error ("The program is not running.");

  /* When single stepping in assembly, the plugin passes (start + 1)
     as the stop address. Round the stop address up to the next valid
     instruction */

  if ((range_stop & ~0x3) != range_stop)
    {
      range_stop = ((range_stop + 4) & ~0x3);
      single_instruction = 1;
    }

  pc = read_pc();

  if (range_start >= range_stop)
    error ("invalid step range (the stop address must be greater than the start address)");

  if (pc < range_start)
    error ("invalid step range ($pc is 0x%lx, less than the stop address of 0x%lx)",
	   (unsigned long) pc, (unsigned long) range_start);
  if (pc == range_stop)
    error ("invalid step range ($pc is 0x%lx, equal to the stop address of 0x%lx)",
	   (unsigned long) pc, (unsigned long) range_stop);
  if (pc > range_stop)
    error ("invalid step range ($pc is 0x%lx, greater than the stop address of 0x%lx)", 
	   (unsigned long) pc, (unsigned long) range_stop);

  /* printf_filtered ("\n\032\032metrowerks-running\n"); */

  while ((pc >= range_start) && (pc < range_stop))
    {
      CORE_ADDR step_resume = 0;
      unsigned long idata = 0;
      int is_branch = -1;
      
      clear_proceed_status ();

      target_read_memory (pc, (char *) &idata, sizeof (idata));
      is_branch = ((IS_BC_x (idata) 
		    || IS_B_x (idata)
		    || IS_BCLR_x (idata)
		    || IS_BCCTR_x (idata))
		   && WILL_LINK (idata));
      
      step_1 (FALSE, TRUE, NULL);
      pc = read_pc ();

      if (!step_into && !single_instruction && is_branch)
	{
	  step_resume = read_register (LR_REGNUM);
	}

      if (!single_instruction && (step_resume == 0))
	{
#if 0
	  if ((mread (pc, 4, 0) == 0x7c0802a6)        /* mflr  r0  */
	      && (mread (pc + 0x1c, 4, 0) == 0x7c0803a6) /* mtlr  r0  */
	      && (mread (pc + 0x20, 4, 0) == 0x4e800020) /* blr       */
	      && (mread (pc + 0x24, 4, 0) == 0x0))       /* .long 0x0 */
	    {
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      step_1 (FALSE, TRUE, NULL);
	      pc = read_pc ();
	    }
#endif /* 0 */
	}
	  
      if (step_resume != 0)
	{
	  set_momentary_breakpoint_address (step_resume);
	  clear_proceed_status ();
	  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);
	  pc = read_pc ();
	}
    }
}

static void
metrowerks_step_out_command (char *args, int from_tty)
{
  unsigned long	offset;
  struct symtab_and_line sal;
  register struct frame_info *frame;
  register struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;

  offset = strtoul (args, NULL , 16);

  /* printf_filtered ("\n\032\032metrowerks-running\n"); */

  if (! target_has_execution)
    error ("The program is not running.");
  if (selected_frame == NULL)
    error ("No selected frame.");

  frame = get_prev_frame (selected_frame);
  if (frame == 0)
    error ("\"metrowerks-step-out\" not meaningful in the outermost frame.");

  clear_proceed_status ();

  sal = find_pc_line (frame->pc, 0);
  sal.pc = frame->pc;

  breakpoint = set_momentary_breakpoint (sal, frame, bp_finish);

  old_chain = make_cleanup ((make_cleanup_func) delete_breakpoint, breakpoint);

  proceed_to_finish = 1;
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);

  do_cleanups (old_chain);
}

static void
metrowerks_break_command (char* args, int from_tty)
{
  char **argv; 
  unsigned long section; 
  unsigned long offset; 
  CFMContainer *cfmContainer = NULL; 
  CFMSection *cfmSection = NULL; 
  struct symtab_and_line sal = { 0, 0 }; 
  struct breakpoint *b = set_raw_breakpoint (sal); 
 
  argv = buildargv (args); 
  if (argv == NULL) { 
    nomem (0); 
  } 
 
  section = strtoul (argv[1], NULL, 16); 
  offset = strtoul (argv[2], NULL, 16); 

  cfmContainer = CFM_FindContainerByName (argv[0], strlen (argv[0]));
  if (cfmContainer != NULL) {
    cfmSection = CFM_FindSection(cfmContainer, kCFContNormalCode);
  }

  set_breakpoint_count (get_breakpoint_count() + 1);
  b->number = get_breakpoint_count();
  b->inserted = 0;
  b->disposition = donttouch;
  b->type = bp_breakpoint;

  if (cfmSection != NULL) {
    b->address = ((unsigned long) cfmSection->mSection.address + offset);
    b->enable = enabled;
    b->addr_string = xmalloc (64);
    sprintf (b->addr_string, "*0x%lx", (unsigned long) b->address);
  } else {
    b->address = 0; 
    b->enable = disabled; 
    b->addr_string = xmalloc (strlen ("@metrowerks:") + strlen (args) + 1);
    sprintf (b->addr_string, "%s:%s", "@metrowerks", args); 
  }
  
  annotate_breakpoint (b->number);

  if (modify_breakpoint_hook) {
    modify_breakpoint_hook (b);
  }
}

bfd *FindContainingBFD (CORE_ADDR address)
{
  struct target_stack_item *aTarget;
  bfd *result = NULL;

  for (aTarget = target_stack; (result == NULL) && (aTarget != NULL); aTarget = aTarget->next) {
    
    struct section_table* sectionTable;

    if ((NULL == aTarget->target_ops) || (NULL == aTarget->target_ops->to_sections)) {
      continue;
    }
        
    for (sectionTable = &aTarget->target_ops->to_sections[0];
	 (result == NULL) && (sectionTable < aTarget->target_ops->to_sections_end);
	 sectionTable++)
      {
	if ((address >= sectionTable->addr) && (address < sectionTable->endaddr)) {
	  result = sectionTable->bfd;
	}
      }
  }
  
  return result;
}

static void
metrowerks_address_to_name_command (char* args, int from_tty)
{
  CORE_ADDR address;
  CFMSection *cfmSection;
  bfd *aBFD;

  address = strtoul (args, NULL, 16);

  if (annotation_level > 1) {
    printf("\n\032\032fragment-name ");
  }

  cfmSection = CFM_FindContainingSection (address);
  if (cfmSection != NULL) {
    printf_unfiltered ("%.*s\n",
		       cfmSection->mContainer->mContainer.name[0],
		       &cfmSection->mContainer->mContainer.name[1]);
    return;
  }

  aBFD = FindContainingBFD (address);
  if (aBFD != NULL) {
    printf_unfiltered ("%s\n", aBFD->filename);
    return;
  }

  printf_unfiltered ("[unknown]\n");
}

void
_initialize_metrowerks(void)
{
    add_com ("metrowerks-step", class_obscure, metrowerks_step_command,
	     "GDB as MetroNub command");

    add_com ("metrowerks-step-out", class_obscure, metrowerks_step_out_command,
	     "GDB as MetroNub command");

    add_com ("metrowerks-break", class_breakpoint, metrowerks_break_command,
	     "GDB as MetroNub command");

    add_com ("metrowerks-address-to-name", class_obscure, metrowerks_address_to_name_command,
	     "GDB as MetroNub command");
}

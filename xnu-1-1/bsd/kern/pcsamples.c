#include <kern/cpu_number.h>
#include <machine/spl.h>

#define HZ      100
#include <mach/clock_types.h>
#include <mach/mach_types.h>

#include <sys/kdebug.h>
#include <sys/errno.h>
#include <sys/param.h>		/* for splhigh */
#include <sys/proc.h>
#include <sys/vm.h>
#include <sys/sysctl.h>
#include <vm/vm_kern.h>


unsigned int pc_buftomem = 0;
u_long     * pc_buffer   = 0;   /* buffer that holds each pc */
u_long     * pc_bufptr   = 0;
u_long     * pc_buflast  = 0;
u_long     * pc_readlast = 0;
unsigned int npcbufs         = 8192;      /* number of pc entries in buffer */
unsigned int pc_bufsize      = 0;
unsigned int pcsample_flags  = 0;
unsigned int pcsample_enable = 0;
unsigned int pcsample_nolog  = 1;

/* Set the default framework boundaries */
u_long pcsample_beg    = 0;
u_long pcsample_end    = 0;

void
add_pcsamples( pc)
     register u_long pc;
{

	if (pcsample_nolog && pcsample_enable)
	  return;

	if ((pc < pcsample_beg) || (pc > pcsample_end))
	  return;

	*pc_bufptr = (u_long)pc;
	pc_bufptr++;

	/* We never wrap the buffer */
	if (pc_bufptr >= pc_buflast)
	  {
	    pcsample_nolog = 1 ;
	  }
}



pcsamples_bootstrap()
{
	pc_bufsize = npcbufs * sizeof(* pc_buffer);
	if (kmem_alloc(kernel_map, &pc_buftomem,
		       (vm_size_t)pc_bufsize) == KERN_SUCCESS) 
	  pc_buffer = (u_long *) pc_buftomem;
	else 
	  pc_buffer= (u_long *) 0;

	if (pc_buffer) {
		pc_bufptr = pc_buffer;
		pc_buflast = &pc_bufptr[npcbufs];
		pc_readlast = pc_bufptr;
		pcsample_enable = 0;
		pcsample_nolog  = 1;
		return(0);
	} else {
		pc_bufsize=0;
		return(EINVAL);
	}
	
}

pcsamples_reinit()
{
int x;
int ret=0;
	if (pc_bufsize && pc_buffer)
		kmem_free(kernel_map,pc_buffer,pc_bufsize);

	ret= pcsamples_bootstrap();
	return(ret);
}



pcsamples_clear()
{
int x;
        /* Clean up the sample buffer, set defaults */ 
	pcsample_enable = 0;
	pcsample_nolog  = 1;
	if(pc_bufsize && pc_buffer)
	  kmem_free(kernel_map,pc_buffer,pc_bufsize);
	pc_buffer   = (u_long *)0;
	pc_bufptr   = (u_long *)0;
	pc_buflast  = (u_long *)0;
	pc_readlast = (u_long *)0;
	pc_bufsize  = 0;
	pcsample_beg= 0;
	pcsample_end= 0;
}

pcsamples_control(name, namelen, where, sizep)
int *name;
u_int namelen;
char *where;
size_t *sizep;
{
int ret=0;
int size=*sizep;
int max_entries;
unsigned int value = name[1];
pcinfo_t pc_bufinfo;
	switch(name[0]) {
	        case PCSAMPLE_ENABLE:    /* used to enable or disable */
		  if (value)
		    {
		      /* enable only if buffer is initialized */
		      if ((pc_bufsize <= 0) || (!pc_buffer))
			{
			  ret=EINVAL;
			  break;
			}
		    }
		  pcsample_enable=(value)?1:0;
		  pcsample_nolog = (value)?0:1;
		  break;
		case PCSAMPLE_SETNUMBUF:
		  /* We allow a maximum buffer size of 25% of memory */
		        max_entries = (mem_size/4) / sizeof(u_long);
			if (value <= max_entries)
				npcbufs = value;
			else
			  npcbufs = max_entries;
			break;
		case PCSAMPLE_GETNUMBUF:
		        if(size < sizeof(pcinfo_t)) {
		            ret=EINVAL;
			    break;
			}
			pc_bufinfo.npcbufs = npcbufs;
			pc_bufinfo.bufsize = pc_bufsize;
			pc_bufinfo.nolog = pcsample_nolog;
			pc_bufinfo.enable = pcsample_enable;
			pc_bufinfo.pcsample_beg = pcsample_beg;
			pc_bufinfo.pcsample_end = pcsample_end;
			if(copyout (&pc_bufinfo, where, sizeof(pc_bufinfo)))
			  {
			    ret=EINVAL;
			  }
			break;
		case PCSAMPLE_SETUP:
			ret=pcsamples_reinit();
			break;
		case PCSAMPLE_REMOVE:
			pcsamples_clear();
			break;
		case PCSAMPLE_READBUF:
			ret = pcsamples_read(where, sizep);
			break;
	        case PCSAMPLE_SETREG:
		        if (size < sizeof(pcinfo_t))
			  {
			    ret = EINVAL;
			    break;
			  }
			if (copyin(where, &pc_bufinfo, sizeof(pcinfo_t)))
			  {
			    ret = EINVAL;
			    break;
			  }

			pcsample_beg = pc_bufinfo.pcsample_beg;
			pcsample_end = pc_bufinfo.pcsample_end;
			break;
		default:
		        ret= EOPNOTSUPP;
			break;
	}
	return(ret);
}



pcsamples_read(u_long *buffer, size_t *number)
{
int x,i;
int ret=0;
int avail=*number;
int count=0;

	count = avail/sizeof(u_long);
	if (count) {
		if (pc_bufsize && pc_buffer) {
			if (count > npcbufs)
			    count = npcbufs;

			for (i = 0; i < count ; i++ )
			  {
			      if (pc_readlast == pc_bufptr)
				{  
				  /* Nothing left to read */
				  /* Reset pointers to begin of buffer */
				  pc_readlast = pc_buffer;
				  pc_bufptr = pc_buffer;
				  if (pcsample_enable)
				    {
				      /* Always be sure logging is on after a reset */
				      pcsample_nolog = 0;
				    }
				  break;
				}
			      if(copyout(pc_readlast,buffer,sizeof(u_long))) {
			          ret=EINVAL;
				  break;
			      }
			      pc_readlast++;
			      buffer ++;
			      if (pc_readlast >= pc_buflast)
				{
				  /* We've reached the end of the buffer */
				  /* Reset pointers to begin of buffer   */
				  pc_readlast = pc_buffer;
				  pc_bufptr = pc_buffer;
				  if (pcsample_enable)
				    {
				      /* Always be sure logging is on after a reset */
				      pcsample_nolog = 0;
				    }
				  break;				  
				}
			  }
			*number = i;
		} else ret = EINVAL;
	} else ret=EINVAL;
	return (ret);
}



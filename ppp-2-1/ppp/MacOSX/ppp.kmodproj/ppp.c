/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 * Nov 1999 - Christophe Allie - created.
 * 
 */

/* -----------------------------------------------------------------------------
PPP Includes
----------------------------------------------------------------------------- */

#include "ppp.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/socketvar.h>


#include <sys/syslog.h>
#include <mach/vm_types.h>
#include <mach/kmod.h>

#include <net/if.h>
#include <net/dlil.h>

#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <mach/mach_types.h>

//#define current_thread() get_current_thread()
//extern thread_t get_current_thread(void);

#include <machine/spl.h>
//#include <machine/thread.h>
//#include <mach/task.h>
#include "thread.h"
#include "task.h"

#ifndef NETISR_PPP
/* This definition should be moved to net/netisr.h */
#define NETISR_PPP	25	/* PPP software interrupt */
#endif

/* -----------------------------------------------------------------------------
PPP definitions
----------------------------------------------------------------------------- */

#define LOGVAL 		LOG_INFO
#define kFamilyName	"ppp"

struct if_attach {
    struct ifnet 	*ifp;
    u_long 		dl_tag;
};

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */
void pppattach(void *dummy);
void pppdetach(void *dummy);
int pppoutput(struct ifnet *ifp, struct mbuf *m0,
    struct sockaddr *dst, struct rtentry *rtp);

int  	ppp_add_if(struct ifnet *ifp);
int  	ppp_del_if(struct ifnet *ifp);
int  	ppp_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag);
int  	ppp_del_proto(struct if_proto *proto, u_long dl_tag);
int  	ppp_shutdown();

//int ppp_ioctl(u_long dl_tag, struct ifnet *ifp, int command, caddr_t data);
int ppp_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
              void  *if_proto_ptr);
int ppp_frameout(struct ifnet *ifp, struct mbuf **m0,
                 struct sockaddr *ndest, char *edst, char *ppp_type);

void pppisr_thread(void);

// missing in dlil.h
int dlil_dereg_if_modules(char *interface_family);

/* -----------------------------------------------------------------------------
PPP globals
----------------------------------------------------------------------------- */
static int 			ppp_inited = 0;
static struct if_attach 	ppp_array[NPPP];
static int 			ppp_count;

int	pppsoft_net_wakeup;
int	pppsoft_net_terminate;
int	pppnetisr;

/* -----------------------------------------------------------------------------
NKE entry point, start routine
----------------------------------------------------------------------------- */
int ppp_module_start(struct kmod_info *ki, void *data)
{
    extern int ppp_init(int);
    return(ppp_init(0));
}

/* -----------------------------------------------------------------------------
NKE entry point, stop routine
----------------------------------------------------------------------------- */
int ppp_module_stop(struct kmod_info *ki, void *data)
{
    extern int ppp_terminate(int);
    return(ppp_terminate(0));
}

/* -----------------------------------------------------------------------------
init function
----------------------------------------------------------------------------- */
int ppp_init(int init_arg)
{
    int ret, i;

    log(LOGVAL, "ppp_init\n");

    if (ppp_inited)
        return(KERN_SUCCESS);

    // init vars
    ppp_count = 0;
    for (i = 0; i < NPPP; i++) {
        ppp_array[i].ifp = 0;
        ppp_array[i].dl_tag = 0;
    }

    // register the module
    ret = dlil_reg_if_modules(kFamilyName, ppp_add_if, ppp_del_if,
                            ppp_add_proto, ppp_del_proto, ppp_shutdown);
    if (ret) {
        log(LOG_INFO, "ppp_init: dlil_reg_if_modules error = 0x%x\n", ret);
        return KERN_FAILURE;
    }

    // attach the interfaces
    pppattach(0);

    // Start up netisr thread
    pppsoft_net_terminate = 0;
    pppsoft_net_wakeup = 0;
    pppnetisr = 0;
    kernel_thread(kernel_task, pppisr_thread);

    // NKE is ready !
    ppp_inited = 1;
    return(KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
terminate function
----------------------------------------------------------------------------- */
int ppp_terminate(int term_arg)
{
    int ret;

    log(LOGVAL, "ppp_terminate\n");

    if (!ppp_inited)
        return(KERN_SUCCESS);

   // first, terminate netisr thread
    pppsoft_net_terminate = 1;
    wakeup(&pppsoft_net_wakeup);
    sleep(&pppsoft_net_terminate, PZERO+1);

    // detach the interfaces
    pppdetach(0);

    ret = dlil_dereg_if_modules(kFamilyName);
    if (ret) {
        log(LOGVAL, "ppp_terminate: dlil_dereg_if_modules error = 0x%x\n", ret);
        return KERN_FAILURE;
    }

    // bug in dlil... call it twice for the time being
    dlil_dereg_if_modules(kFamilyName);

    return(KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
add interface function
called from dlil when a new interface is attached for that family
needs to set the frameout and demux dlil specific functions in the ifp
----------------------------------------------------------------------------- */
int ppp_add_if(struct ifnet *ifp)
{

    log(LOGVAL, "ppp_add_if, ifp = 0x%x\n", ifp);
    ifp->if_framer = ppp_frameout;
    ifp->if_demux  = ppp_demux;
    return 0;
}

/* -----------------------------------------------------------------------------
delete interface function
called from dlil when an interface is detached for that family
----------------------------------------------------------------------------- */
int ppp_del_if(struct ifnet *ifp)
{
    log(LOGVAL, "ppp_del_if\n");
    return 0;
}

/* -----------------------------------------------------------------------------
add protocol function
called from dlil when a network protocol is attached for an
interface from that family (i.e ip is attached through ppp_attach_ip)
----------------------------------------------------------------------------- */
int ppp_add_proto(struct ddesc_head_str *desc_head, struct if_proto *proto, u_long dl_tag)
{
    log(LOGVAL, "ppp_add_proto\n");
    return 0;
}

/* -----------------------------------------------------------------------------
delete protocol function
called from dlil when a network protocol is detached for an
interface from that family (i.e ip is attached through ppp_detach_ip)
----------------------------------------------------------------------------- */
int  ppp_del_proto(struct if_proto *proto, u_long dl_tag)
{
    log(LOGVAL, "ppp_del_proto\n");
    return 0;
}

/* -----------------------------------------------------------------------------
shutdown function
called from dlil when this familiy is deregistered
----------------------------------------------------------------------------- */
int ppp_shutdown()
{
    log(LOGVAL, "ppp_shutdown\n");
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
/*int ppp_ioctl(u_long dl_tag, struct ifnet *ifp, int command, caddr_t data)
{
    log(LOGVAL, "ppp_ioctl, ifp = 0x%x\n", ifp);
    return 0;
}*/

/* -----------------------------------------------------------------------------
demux function
called from dlil when a packet from the interface needs to be dispatched.
the demux function will analyze the frame, determine the network protocol
transported and return the corresponding ifproto.
----------------------------------------------------------------------------- */
int ppp_demux(struct ifnet *ifp, struct mbuf *m, char *frame_header,
              void  *if_proto_ptr)
{
    return EJUSTRETURN;
}

/* -----------------------------------------------------------------------------
frameout function
a network packet needs to be send through the interface.
add the ppp header to the packet : FF 03 XX XX
----------------------------------------------------------------------------- */
int ppp_frameout(struct ifnet *ifp, struct mbuf **m0,
                   struct sockaddr *ndest, char *edst, char *ppp_type)
{
    //log(LOGVAL, "ppp_frameout\n");

    ndest->sa_family = AF_INET;
    pppoutput(ifp, *m0, ndest, 0);
    return EJUSTRETURN;

    // it's just a way to give the dest parameter to the if_output routine
    //ndest->sa_family = AF_INET;
    //ifp->if_private = ndest;
    //return (0);
}

/* -----------------------------------------------------------------------------
attach the PPPx interface ifp to the network protocol IP,
should be called through an event from the ppp interface
whan the ppp if becomes up (i.e. ipcp successfully negociated)
----------------------------------------------------------------------------- */
int ppp_attach_ip(struct ifnet *ifp, u_long *dl_tag)
{
    int 			ret, i;
    struct dlil_proto_reg_str   reg;
    unsigned long 		ip_dl_tag;

    log(LOGVAL, "ppp_attach_ip, name = %s, unit = %d\n", ifp->if_name, ifp->if_unit);

    for (i = 0; i < ppp_count; i++) {
        if (ppp_array[i].ifp == ifp) {
            // already attached...
            *dl_tag = ppp_array[i].dl_tag;
            return 0;
        }
    }

    if (ppp_count == NPPP)
        return ENOMEM;

    bzero(&reg, sizeof(struct dlil_proto_reg_str));
    reg.protocol_family = "ip";	// ????
    reg.interface_family = ifp->if_name;
    reg.unit_number      = ifp->if_unit;
//    reg.ioctl            = ppp_ioctl;
    ret = dlil_attach_protocol(&reg, &ip_dl_tag);
    if (ret) {
        log(LOG_INFO, "ppp_attach_ip: dlil_attach_protocol error = 0x%x\n", ret);
        return ret;
    }
    log(LOG_INFO, "ppp_attach_ip: dlil_attach_protocol tag = 0x%x\n", ip_dl_tag);

    // keep track of ifp/tag
    ppp_array[ppp_count].dl_tag = ip_dl_tag;
    ppp_array[ppp_count++].ifp = ifp;
    *dl_tag = ip_dl_tag;

    return 0;
}

/* -----------------------------------------------------------------------------
detach the PPPx interface ifp from the network protocol IP,
should be called through an event from the ppp interface
whan the ppp if becomes down (i.e. ipcp stops)
----------------------------------------------------------------------------- */
int ppp_detach_ip(struct ifnet *ifp)
{
    int 	ret, i;

    log(LOGVAL, "ppp_detach_ip\n");

    for (i = 0; i < ppp_count; i++) {
        if (ppp_array[i].ifp == ifp) {
            ret = dlil_detach_protocol(ppp_array[i].dl_tag);
            if (ret) {
                log(LOG_INFO, "dlil_detach_protocol: dlil_detach_protocol error = 0x%x\n", ret);
                return ret;
            }
            // update ifp/tag array
            ppp_array[i].ifp = 0;
            ppp_array[i].dl_tag = 0;
            i++;
            for ( ; i < ppp_count; i++) {
                ppp_array[i-1].ifp = ppp_array[i].ifp;
                ppp_array[i-1].dl_tag = ppp_array[i].dl_tag;
            }
            ppp_count--;
            return 0;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
 All routines this thread calls expect to be called at splnet
----------------------------------------------------------------------------- */
void pppisr_thread_continue(void)
{
    spl_t s;

    s = splnet();

    while (!pppsoft_net_terminate) {
        if (pppnetisr) {
            void pppintr(void);

            pppnetisr = 0;
            pppintr();
        }

/*        assert_wait(&pppsoft_net_wakeup, FALSE);
#if 0
        thread_block(pppisr_thread_continue);
#else
        thread_block(0);
#endif*/
       sleep(&pppsoft_net_wakeup, PZERO+1);
    }
    splx(s);

   wakeup(&pppsoft_net_terminate);
   //thread_terminate(current_act());
   thread_terminate_self();
    /* NOTREACHED */
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void pppisr_thread(void)
{
	register thread_t self = current_thread();
	extern void stack_privilege(thread_t thread);
#if NCPUS > 1
        sd
	extern void thread_bind(thread_t, processor_t);
#endif
	/*
	 *	Make sure that this thread 
	 *	always has a kernel stack, and
	 *	bind it to the master cpu.
	 */
	stack_privilege(self);
#if NCPUS > 1
	thread_bind(self, master_processor);
#endif
#if 0
	thread_block(pppisr_thread_continue);
	/* NOTREACHED */
#else
	pppisr_thread_continue();
#endif
}

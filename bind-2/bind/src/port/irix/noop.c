#include <port_before.h>
#include <port_after.h>

void
__bind_noop() {
	/* NOOP */
}

#ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF

#include <stdio.h>		/* NULL */
#include <sys/types.h>
#include <sys/sysctl.h>		/* sysctl() */
#include <sys/socket.h>		/* sysctl(), struct sockaddr */
#include <stdlib.h>		/* malloc() */
#include <net/route.h>		/* RTM_* */
#include <net/if_dl.h>		/* struct sockaddr_dl */
#include <string.h>		/* memcpy(), memset() */
#include <errno.h>		/* setoserror() */

#define ROUNDUP( sz ) (( (sz) > 0 ) ?\
			( 1 + (((sz) - 1) | (sizeof(__uint64_t) - 1))) :\
			0)

#ifdef _HAVE_SA_LEN
#define ADVANCE( ptr, sa ) ptr += ROUNDUP( (sa)->sa_len ) 
#else
#define ADVANCE( ptr, sa ) ptr += ROUNDUP( _FAKE_SA_LEN_DST(sa) ) 
#endif

int
emul_ioctl( struct ifconf * ifc )
{
    size_t		bufsz, used_sz;
    char		*buf, *ptr, *end_ptr;
    int			mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
    struct if_msghdr	*ifmsghdr;		/* Iface info */
    struct ifa_msghdr	*ifamsghdr;		/* Iface address info */
    char		if_name[IFNAMSIZ];	/* Interface name */
    struct sockaddr_dl	*if_addr;		/* Interface name struct */
    char		*addr_ptr;		/* Next address */
    struct ifreq	*ifreq_ptr;		/* current ifreq */
    struct sockaddr	*curr_addr;

    /* Check parameter */

    if ( (ifc == NULL) || (ifc->ifc_len <= 0) || (ifc->ifc_buf == NULL) )
    {

	setoserror(EINVAL);
	return -1;

    }

    /* Get size of buffer needed */

    if (sysctl(mib, 6, NULL, &bufsz, NULL, 0) < 0)
    {

	return -1;

    }


    /* Allocate memmory and retrieve info */

    if ((buf = (char *)malloc(bufsz)) == NULL)
    {

	return -1;
	
    }
	
    if (sysctl(mib, 6, buf, &bufsz, NULL, 0) < 0)
    {
	
	free(buf);
	return -1;

    }

    /* Place info in ifconf structure */

    for ( ptr = buf, end_ptr = buf + bufsz, if_name[0] = '\0', used_sz = 0,\
	      ifreq_ptr = (struct ifreq *)(ifc->ifc_buf);\
	  ptr < end_ptr;\
	  ptr += ifmsghdr->ifm_msglen )
    {
	
	ifmsghdr = (struct if_msghdr *)ptr;

	if ( ifmsghdr->ifm_type == RTM_IFINFO )
	{

	    memset( if_name, 0, IFNAMSIZ );

	    /* We only get interface name from this message */

	    if ( ifmsghdr->ifm_addrs & RTA_IFP )
	    {

#ifdef _IGNORE_DOWN_IFACES
		if (!(IFF_UP & ifcmsghdr->ifm_flags))
		{
		    
		    /* Skip if iface is down */
		    continue;
		    
		}
#endif
		
		if_addr = (struct sockaddr_dl *)(ifmsghdr + 1);
		memcpy( if_name, if_addr->sdl_data,\
			MIN( (size_t)(if_addr->sdl_nlen), IFNAMSIZ ) );

	    }/* if ( ifmsghdr->ifm_addrs & RTA_IFP ) */

	}
	else if ( ifmsghdr->ifm_type == RTM_NEWADDR )
	{

	    if ( if_name[0] == '\0' )
	    {

		/* No iface name for this address ignore */
		continue;

	    }

	    /* Here we get the address info and create an entry in ifc */

	    ifamsghdr = (struct ifa_msghdr *)ptr;	/* Correct format */
	    addr_ptr = (char *)(ifamsghdr + 1);
	    curr_addr= (struct sockaddr *)addr_ptr;

	    if ( ifamsghdr->ifam_addrs & RTA_NETMASK )
	    {

		/* Mask address present, skip it */

		ADVANCE( addr_ptr, curr_addr );
		curr_addr = (struct sockaddr *)addr_ptr;

	    }/* if ( ifamsghdr->ifam_addrs & RTA_NETMASK ) */


	    if ( ifamsghdr->ifam_addrs & RTA_IFA )
	    {

		/* Get interface address and create ifc entry */

		used_sz += sizeof(struct ifreq);

		if ( used_sz > ifc->ifc_len )
		{

		    /* Return whatever we were able to retrieve with given
		     * buffer.
		     */

		    free(buf);
		    ifc->ifc_len = used_sz - sizeof(struct ifreq);
		    setoserror( EINVAL );
		    return -1;

		}
		
		memcpy( ifreq_ptr->ifr_name, if_name, IFNAMSIZ );

#ifdef _HAVE_SA_LEN
		memcpy( &(ifreq_ptr->ifr_addr), curr_addr, curr_addr->sa_len );
#else
		memcpy( &(ifreq_ptr->ifr_addr), curr_addr,\
			_FAKE_SA_LEN_DST(curr_addr));
#endif
		ifreq_ptr++;
		
	    }
	 	    
	}
	
	/* The RTM_DELADDR message is ignored */
	
    }/* for ( ptr = buf, end_ptr = buf + bufsz, if_name[0] = '\0',...) */

    if ( ptr < end_ptr ) 
    {
	
	/* Not enough memory to place all interfaces */
	free(buf);
	setoserror(EINVAL);
	return -1;

    }

    ifc->ifc_len = used_sz;
    
    free(buf);
    return 0;

}/* int emul_ioctl() */

#endif	/* #ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF */

/* This is a substitute for bind() that if called for an AF_INET socket
port less than 1024, GETPRIVMODE() and GETUSERMODE() calls will be done. */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

extern GETPRIVMODE();
extern GETUSERMODE();

int __bind_mpe_bind(int s, void *addr, int addrlen) {
	int priv = 0;
	int result;

if (addrlen == sizeof(struct sockaddr_in)) { /* AF_INET */
	if (((struct sockaddr_in *)addr)->sin_port > 0 &&
	    ((struct sockaddr_in *)addr)->sin_port < 1024) {
		priv = 1;
		GETPRIVMODE();
	}
	((struct sockaddr_in *)addr)->sin_addr.s_addr = 0;
	result = bind(s,addr,addrlen);
	if (priv == 1) GETUSERMODE();
} else /* AF_UNIX */
	result = bind(s,addr,addrlen);

return result;
}

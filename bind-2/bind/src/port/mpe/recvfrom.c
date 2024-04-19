#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdio.h>

int
__bind_mpe_recvfrom(int s, void *buf, size_t len, int flags,
		    struct sockaddr *from, int *fromlen) {
	int n;

	if (len > 30000) len = 30000;
	n = recvfrom(s, buf, len, flags, from, fromlen);
	if (n == -1 && errno == EOPNOTSUPP) {
		n = recv(s, buf, len, flags);
		if (n == -1)
			return (-1);
		if (from != NULL && getpeername(s, from, fromlen) < 0)
			return (-1);
	}
	return (n);
}

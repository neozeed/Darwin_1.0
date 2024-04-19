#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>

int
__bind_mpe_fcntl(int fd, int cmd, int arg) {
	int len;
	struct sockaddr sa;

	len = sizeof sa;
	if (getsockname(fd, &sa, &len) == -1) {
		if (errno == EAFNOSUPPORT) /* AF_UNIX socket */
			return sfcntl(fd, cmd, arg);
		if (errno == ENOTSOCK) /* file or pipe */
			return fcntl(fd, cmd, arg);
		return (-1); /* unknown getsockname() failure */
	} else /* AF_INET socket */
		return sfcntl(fd, cmd, arg);
}

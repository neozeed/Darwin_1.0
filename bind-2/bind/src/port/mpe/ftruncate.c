/*
 * ftruncate - set file size, BSD Style
 *
 * shortens or enlarges the file as neeeded
 * uses some undocumented locking call. It is known to work on SCO unix,
 * other vendors should try.
 * The #error directive prevents unsupported OSes
 */

#include "port_before.h"
#include <fcntl.h>
#include "port_after.h"

extern FCONTROL(short, short, short);

int
__bind_mpe_ftruncate(int fd, long wantsize) {
	long cursize;

	/* determine current file size */
	if ((cursize = lseek(fd, 0L, 2)) == -1)
		return (-1);

	/* maybe lengthen... */
	if (cursize < wantsize) {
		if (lseek(fd, wantsize - 1, 0) == -1 ||
		    write(fd, "", 1) == -1) {
			return (-1);
		}
		return (0);
	}

	/* maybe shorten... */
	if (wantsize < cursize) {
		if (lseek(fd, wantsize - 1, 0) == -1) {
			return (-1);
		}
		FCONTROL(_MPE_FILENO(fd),6,0); /* Write smaller EOF */
		return (0);
	}
	return (0);
}

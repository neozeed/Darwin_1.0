#include <fcntl.h>

extern FCONTROL(short, short, void *);

int
__bind_mpe_fsync(int file) {
	FCONTROL(_MPE_FILENO(file),2,NULL);
	FCONTROL(_MPE_FILENO(file),6,NULL);
	return 0;
}

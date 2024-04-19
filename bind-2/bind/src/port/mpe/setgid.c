#undef setgid

#include <sys/types.h>
#include <unistd.h>

int __bind_mpe_setgid(gid_t gid) {

int result;

extern GETPRIVMODE();
extern GETUSERMODE();

GETPRIVMODE();
result = setgid(gid);
GETUSERMODE();

return result;
}

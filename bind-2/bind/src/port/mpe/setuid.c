#undef setuid

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

int __bind_mpe_setuid(uid_t uid) {

char *cwd;
char cwd_buf[PATH_MAX+1];
int result;

extern GETPRIVMODE();
extern GETUSERMODE();

/*
**      setuid() is broken as of MPE 5.5 in that it changes the current
**      working directory to be the home directory of the new uid.  Thus
**      we must obtain the cwd and restore it after the setuid().
*/

cwd = getcwd(cwd_buf, PATH_MAX+1); /* Preserve the current working directory */

GETPRIVMODE();
result = setuid(uid);
GETUSERMODE();

chdir(cwd_buf); /* Restore the current working directory */

return result;
}

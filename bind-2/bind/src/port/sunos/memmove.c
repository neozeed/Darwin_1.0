#include <sys/types.h>

#include <strings.h>

void *
memmove(void *pvTo, const void *pvFrom, size_t Len) {
	bcopy(pvFrom, pvTo, Len);

	return (pvTo);
}

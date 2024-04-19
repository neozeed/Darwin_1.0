#include "symtab.h"

struct objfile *symbol_file_add_bfd_safe
PARAMS ((bfd *abfd, int from_tty, struct section_addr_info *addrs,
 int mainline, int flags, CORE_ADDR mapaddr, const char *prefix));

bfd *symfile_bfd_open_safe
PARAMS ((const char *filename));

bfd *dyld_map_image
PARAMS ((CORE_ADDR addr, CORE_ADDR offset));

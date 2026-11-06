#ifndef CORE_LIBS_PRX_LIBSCEJXR_SRC_GUESTHEAP_H
#define CORE_LIBS_PRX_LIBSCEJXR_SRC_GUESTHEAP_H

#include <stddef.h>
#include <stdlib.h>

void* JxrGuestCalloc_nid_no_patch(size_t count, size_t size);
void JxrGuestFree_nid_no_patch(void* memory);

#define calloc JxrGuestCalloc_nid_no_patch
#define free JxrGuestFree_nid_no_patch

#endif

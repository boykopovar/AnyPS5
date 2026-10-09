#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_APPLICATIONHEAP_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_APPLICATIONHEAP_HPP

#include <cstddef>

extern "C" {

const void* ApplicationProcessParameters_nid_no_patch();
void ApplicationHeapInitialize_nid_no_patch(const void* processParameters);
void ApplicationHeapRegister_nid_no_patch(void* const* api);
void* ApplicationHeapAllocate_nid_no_patch(std::size_t bytes);
void ApplicationHeapFree_nid_no_patch(void* pointer);
void* ApplicationHeapReallocate_nid_no_patch(void* pointer, std::size_t bytes);
void* ApplicationHeapAlign_nid_no_patch(std::size_t alignment, std::size_t bytes);
void* ApplicationHeapRealign_nid_no_patch(void* pointer, std::size_t bytes, std::size_t alignment);
void* ApplicationHeapCalloc_nid_no_patch(std::size_t count, std::size_t bytes);
int ApplicationHeapPosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t bytes);
int ApplicationHeapStatsFast_nid_no_patch(void* stats);

}

#endif

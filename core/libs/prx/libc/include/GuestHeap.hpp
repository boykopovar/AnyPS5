#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAP_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAP_HPP

#include <cstddef>

namespace GuestHeap {

extern "C" {

std::size_t GuestHeapUsableSize_nid_postfix(const void* pointer);
void* GuestHeapAllocate_nid_postfix(std::size_t bytes);
void GuestHeapFree_nid_postfix(void* pointer);
void* GuestHeapReallocate_nid_postfix(void* pointer, std::size_t bytes);
void* GuestHeapAlign_nid_postfix(std::size_t alignment, std::size_t bytes);
void* GuestHeapRealign_nid_postfix(void* pointer, std::size_t bytes, std::size_t alignment);
void* GuestHeapAlignForGpu_nid_postfix(std::size_t alignment, std::size_t bytes);
int GuestHeapStatsFast_nid_postfix(void* stats);

}

}

#endif

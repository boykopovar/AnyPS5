#ifndef CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_KSEM_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_KSEM_HPP

#include <cstdint>

void DuplicateKsemDescriptor(int from, int to);

#ifdef ANYPS5_KSEM_TESTING
extern "C" unsigned KsemWaiterCountForTests_nid_no_patch(std::intptr_t id);
#endif

#endif

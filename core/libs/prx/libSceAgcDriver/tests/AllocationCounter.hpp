#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_ALLOCATIONCOUNTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_ALLOCATIONCOUNTER_HPP

#include <cstddef>

extern "C" void BeginAllocationCount_nid_no_patch() noexcept;
extern "C" std::size_t EndAllocationCount_nid_no_patch() noexcept;
extern "C" void AllocateForCount_nid_no_patch(std::size_t bytes);

#endif

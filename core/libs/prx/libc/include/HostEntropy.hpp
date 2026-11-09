#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTENTROPY_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTENTROPY_HPP

#include <cstddef>

extern "C" void HostEntropy_nid_no_patch(void* buffer, std::size_t size);

#endif

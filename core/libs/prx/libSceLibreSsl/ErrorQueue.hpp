#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_ERRORQUEUE_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_ERRORQUEUE_HPP

#include <cstddef>
#include <cstdint>
#include "include/Error.hpp"
#include "libSceLibreSsl.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
std::uint64_t LibreSslPeekLastError_nid_no_patch();

}

#endif

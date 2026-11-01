#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CREATIONMASK_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CREATIONMASK_HPP

#include <cstdint>

namespace File {

extern "C" std::uint16_t ExchangeCreationMask_nid_no_patch(std::uint16_t mask);
extern "C" std::uint16_t GetCreationMask_nid_no_patch();

}

#endif

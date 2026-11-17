#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"

namespace {

struct BaseTag;
using BaseSlot = HostThreadSlot<std::uint64_t, BaseTag>;

}

extern "C" std::uint64_t GuestGsGet_nid_no_patch() { return BaseSlot::Get(); }
extern "C" void GuestGsSet_nid_no_patch(std::uint64_t base) { BaseSlot::Set(base); }
#endif

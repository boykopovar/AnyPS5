#ifndef PRX_LIBC_INCLUDE_SPECIFICS_MACOS_GUESTFAULTCOPY_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_MACOS_GUESTFAULTCOPY_HPP

#include <csignal>
#include <cstddef>

namespace GuestFaultCopy {

bool Ready();
bool Copy(void* destination, const void* source, std::size_t bytes);
bool Resume(const siginfo_t* info, void* context);
void Reinstall();

}

#endif

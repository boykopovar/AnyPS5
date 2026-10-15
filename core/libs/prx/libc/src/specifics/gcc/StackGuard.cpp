#include <cstdint>
#include <cstdlib>

#ifdef __APPLE__
std::uintptr_t GuestStackChkGuard __asm__("___stack_chk_guard_nid_postfix") = 0xDEADBEEFCAFEBABEull;
#else
std::uintptr_t __stack_chk_guard = 0xDEADBEEFCAFEBABEull;
#endif

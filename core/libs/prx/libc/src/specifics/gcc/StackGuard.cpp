#include <cstdint>
#include <cstdlib>

#ifdef __APPLE__
extern "C" std::uintptr_t __stack_chk_guard_nid_postfix = 0xDEADBEEFCAFEBABEull;
#else
std::uintptr_t __stack_chk_guard = 0xDEADBEEFCAFEBABEull;
#endif

#include <cstdint>

static thread_local std::uint64_t base = 0;

extern "C" std::uint64_t GuestGsGet_nid_no_patch() { return base; }
extern "C" void GuestGsSet_nid_no_patch(std::uint64_t value) { base = value; }
#ifdef GUEST_GS_REPLAY_ONLY
extern "C" bool GuestGsIsReplay_nid_no_patch(std::uintptr_t) { return false; }
#endif

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* p) {
    if (p == nullptr) APS5_INVALID_ARG_EX;
    constexpr Audio3dOpenParameters defaults{0x20, 256, 0, 512, 2, 2, 0, 0};
    static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);
    std::memcpy(p, &defaults, defaults.size_this);
}

int APS5_VABI sceAudio3dInitialize(int64_t reserved) {
 (void)reserved;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudio3dPortAdvance(uint32_t port_id) {
 (void)port_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level, uint32_t* queue_available) {
 (void)port_id;
 (void)queue_level;
 (void)queue_available;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, uint32_t* id) {
 (void)user_id;
 (void)parameters;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudio3dPortPush(uint32_t port_id, uint32_t blocking) {
 (void)port_id;
 (void)blocking;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAudio3dPortSetAttribute(uint32_t port_id, uint32_t attribute_id, const void* attribute, size_t attribute_size) {
 (void)port_id;
 (void)attribute_id;
 (void)attribute;
 (void)attribute_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

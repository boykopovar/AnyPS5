#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI scePthreadAttrGetaffinity(void* const* attr, KernelCpumask* mask);
int APS5_VABI scePthreadAttrGetsolosched(void* const* attr, int* solosched);
int APS5_VABI scePthreadAttrSetaffinity(void** attr, KernelCpumask mask);
int APS5_VABI scePthreadAttrSetstackaddr(void** attr, void* stackAddress);

static int PthreadAttrErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

static constexpr std::size_t GuestCpusetBytes = 32;
static constexpr int GuestEfault = 14;
static constexpr int GuestEinval = 22;
static constexpr int GuestErange = 34;

int APS5_VABI pthread_attr_getaffinity_np_nid_postfix(void* const* attr, std::size_t cpusetSize, void* cpuset) {
    if (!attr || !*attr) return GuestEinval;
    if (cpusetSize < GuestCpusetBytes) return GuestErange;
    if (!cpuset) return GuestEfault;
    KernelCpumask mask = 0;
    if (const int result = scePthreadAttrGetaffinity(attr, &mask); result != 0) return PthreadAttrErrno(result);
    auto* bytes = static_cast<unsigned char*>(cpuset);
    for (std::size_t index = 0; index < cpusetSize; ++index) bytes[index] = index < sizeof(mask) ? static_cast<unsigned char>(mask >> (8u * index)) : 0u;
    return 0;
}

int APS5_VABI pthread_attr_getsolosched_np_nid_postfix(void* const* attr, int* solosched) {
    if (!attr || !*attr || !solosched) return GuestEinval;
    return PthreadAttrErrno(scePthreadAttrGetsolosched(attr, solosched));
}

int APS5_VABI pthread_attr_setaffinity_np_nid_postfix(void** attr, std::size_t cpusetSize, const void* cpuset) {
    if (!attr || !*attr) return GuestEinval;
    if (cpusetSize == 0 || !cpuset) NotImplemented_nid_no_patch("pthread_attr_setaffinity_np clearing the attribute's CPU set");
    const auto* bytes = static_cast<const unsigned char*>(cpuset);
    for (std::size_t index = sizeof(KernelCpumask); index < cpusetSize; ++index) {
        if (bytes[index] != 0) return GuestEinval;
    }
    KernelCpumask mask = 0;
    for (std::size_t index = 0; index < cpusetSize && index < sizeof(mask); ++index) mask |= static_cast<KernelCpumask>(bytes[index]) << (8u * index);
    return PthreadAttrErrno(scePthreadAttrSetaffinity(attr, mask));
}

int APS5_VABI pthread_attr_setstackaddr_nid_postfix(void** attr, void* stackAddress) {
    if (!attr || !*attr) return GuestEinval;
    if (!stackAddress) NotImplemented_nid_no_patch("pthread_attr_setstackaddr with a null stack address");
    return PthreadAttrErrno(scePthreadAttrSetstackaddr(attr, stackAddress));
}

}

#include "prx/libSceSulphaDrv/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x804D0002u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x804D0003u);

template <typename TValue>
bool ReadConfigField(const void* config, std::size_t offset, TValue& value) {
    const int savedErrno = errno;
    const bool copied =
        GuestCopyIn_nid_no_patch(&value, static_cast<const unsigned char*>(config) + offset, sizeof(value));
    errno = savedErrno;
    return copied;
}

bool ConfigKnownInvalid(const void* config) {
    std::uint32_t setting = 0;
    if (!ReadConfigField(config, 0x10, setting)) return false;
    if (setting > 3) return true;
    std::uint64_t size = 0;
    if (!ReadConfigField(config, 0x0, size)) return false;
    if (size != 0x230 && size != 0x18) return true;
    std::uint64_t capacity = 0;
    if (!ReadConfigField(config, 0x8, capacity)) return false;
    if (capacity < 0x400 || capacity == 0xFFFFFFFFu) return true;
    std::uint32_t port = 0;
    if (!ReadConfigField(config, 0x14, port)) return false;
    return port > 0xFFFF;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceSulphaGetClientInfo(std::uint32_t client, void* info) {
    if (client > 1 || info == nullptr) return ErrorInvalidArgument;
    return ErrorNotInitialized;
}

int APS5_VABI sceSulphaGetConnectedClientCount(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceSulphaGetDefaultConfig(void* config) {
    if (config == nullptr) return static_cast<int>(0x804D0003u);
    static constexpr std::uint64_t defaults[]{24, 0x400000, 0xFA100000003};
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(config, defaults, sizeof(defaults));
    errno = savedErrno;
    if (!copied) return static_cast<int>(0x804D0003u);
    return 0;
}

int APS5_VABI sceSulphaGetNeededMemory(const void* config, void* neededSize) {
    if (config == nullptr || neededSize == nullptr || ConfigKnownInvalid(config)) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaInit(const void* config) {
    if (config == nullptr || ConfigKnownInvalid(config)) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSulphaMessage(std::uint32_t type, const void* message) {
    if (type > 2 || message == nullptr) return ErrorInvalidArgument;
    return ErrorNotInitialized;
}

int APS5_VABI sceSulphaSetBookmark(const void* bookmark) {
    if (bookmark == nullptr) return ErrorInvalidArgument;
    return ErrorNotInitialized;
}

int APS5_VABI sceSulphaShutdown(void) {
    return ErrorNotInitialized;
}

}

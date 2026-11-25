#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVideoNativeExtEssential/include/InventoryExports.hpp"
#include "prx/libSceVideoNativeExtEssential/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0xF0000000u);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0xF0000002u);
constexpr int ErrorInvalidArgument = static_cast<int>(0xF0000003u);
constexpr int ErrorUnknownType = static_cast<int>(0xF0000008u);

struct AllocatorCallbacks {
    std::uint64_t allocFn;
    std::uint64_t freeFn;
    std::uint64_t reallocFn;
};

std::atomic<bool> initialized{false};

bool IsInitialized() {
    return initialized.load(std::memory_order_acquire);
}

bool IsKnownDecoderType(std::uint32_t type) {
    switch (type) {
        case 2:
        case 4:
        case 0x2715:
        case 0x2718:
        case 0x271A:
        case 0x271C:
            return true;
        default:
            return false;
    }
}

bool IsValidType2ProfileAndLevel(std::uint16_t profile, std::uint16_t level) {
    if (profile != 4 && profile != 5) return false;
    return level == 4 || level == 6 || level == 8 || level == 10;
}

bool IsValidType4ProfileAndLevel(std::uint16_t profile, std::uint16_t level) {
    if (profile != 0x42 && profile != 0x4D && profile != 0x64 && profile != 0x80) return false;
    switch (level) {
        case 10:
        case 11:
        case 12:
        case 13:
        case 20:
        case 21:
        case 22:
        case 30:
        case 31:
        case 32:
        case 40:
        case 41:
        case 42:
        case 50:
        case 51:
        case 111:
            return true;
        default:
            return false;
    }
}

template <typename T>
T ReadGuest(const void* address, const char* what) {
    T value{};
    if (address == nullptr) {
        throw std::invalid_argument(what);
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, address, sizeof(value));
    errno = savedErrno;
    if (!copied) {
        throw std::invalid_argument(what);
    }
    return value;
}

} // namespace

extern "C" {

int APS5_VABI sceVideoNativeExtEssential_CreateDecInstance(const void* param, void** instance) {
    const std::uint64_t none = 0;
    if (instance == nullptr) {
        throw std::invalid_argument("sceVideoNativeExtEssential_CreateDecInstance: unwritable instance");
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(instance, &none, sizeof(none));
    errno = savedErrno;
    if (!copied) {
        throw std::invalid_argument("sceVideoNativeExtEssential_CreateDecInstance: unwritable instance");
    }
    if (!IsInitialized()) return ErrorNotInitialized;
    const auto type = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param));
    if (!IsKnownDecoderType(type)) return ErrorUnknownType;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_DestroyDecInstance(void* const* instance) {
    if (!IsInitialized()) return ErrorNotInitialized;
    if (instance == nullptr) return ErrorInvalidArgument;
    if (ReadGuest<void*>(instance, "sceVideoNativeExtEssential_DestroyDecInstance: unreadable instance") == nullptr) {
        return ErrorInvalidArgument;
    }
    return ErrorInvalidArgument;
}

int APS5_VABI sceVideoNativeExtEssential_Finalize() {
    initialized.store(false, std::memory_order_release);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_Initialize(const void* param) {
    if (param == nullptr) return ErrorInvalidArgument;
    const auto allocatorAddress = ReadGuest<std::uint64_t>(param, "sceVideoNativeExtEssential_Initialize: unreadable parameter");
    if (allocatorAddress == 0) return ErrorInvalidArgument;
    if (ReadGuest<std::uint32_t>(static_cast<const std::byte*>(param) + 8, "sceVideoNativeExtEssential_Initialize: unreadable parameter") != 1) {
        return ErrorInvalidArgument;
    }
    if (IsInitialized()) return ErrorAlreadyInitialized;
    const auto* allocatorBytes = reinterpret_cast<const std::byte*>(static_cast<std::uintptr_t>(allocatorAddress));
    (void)ReadGuest<AllocatorCallbacks>(allocatorBytes + 0x40, "sceVideoNativeExtEssential_Initialize: unreadable allocator");
    bool expected = false;
    if (!initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return ErrorAlreadyInitialized;
    }
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_IsInit() {
    return IsInitialized() ? 1 : 0;
}

}

extern "C" {

int APS5_VABI libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(std::int32_t type, std::uint32_t argument1, std::uint32_t argument2,
    std::uint32_t argument3, std::uint32_t argument4, std::uint32_t argument5, void* output) {
    (void)argument1;
    (void)argument2;
    (void)argument3;
    if (output == nullptr) return ErrorInvalidArgument;
    if (type != 2 && type != 3 && type != 4) return ErrorUnknownType;
    const std::uint64_t zero = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(output, &zero, sizeof(zero));
    errno = savedErrno;
    if (!copied) {
        throw std::invalid_argument("libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch: unwritable output");
    }
    const auto profile = static_cast<std::uint16_t>(argument4);
    const auto level = static_cast<std::uint16_t>(argument5);
    if (type == 2 && !IsValidType2ProfileAndLevel(profile, level)) return ErrorInvalidArgument;
    if (type == 4 && !IsValidType4ProfileAndLevel(profile, level)) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iwAQhvb9BvY", libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch);

}

#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVideoNativeExtEssential/include/InventoryExports.hpp"
#include "prx/libSceVideoNativeExtEssential/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0xF0000000u);
constexpr int ErrorInvalidArgument = static_cast<int>(0xF0000003u);
constexpr int ErrorUnknownType = static_cast<int>(0xF0000008u);

bool IsInitialized() {
    return false;
}

template <typename T>
T ReadGuest(const void* address, const char* what) {
    T value{};
    if (address == nullptr || !GuestCopyIn_nid_no_patch(&value, address, sizeof(value))) {
        throw std::invalid_argument(what);
    }
    return value;
}

} // namespace

extern "C" {

int APS5_VABI sceVideoNativeExtEssential_CreateDecInstance(const void* param, void** instance) {
    (void)param;
    const std::uint64_t none = 0;
    if (instance == nullptr || !GuestCopyOut_nid_no_patch(instance, &none, sizeof(none))) {
        throw std::invalid_argument("sceVideoNativeExtEssential_CreateDecInstance: unwritable instance");
    }
    if (!IsInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_DestroyDecInstance(void* const* instance) {
    (void)instance;
    if (!IsInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_Finalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_Initialize(const void* param) {
    if (param == nullptr) return ErrorInvalidArgument;
    if (ReadGuest<std::uint64_t>(param, "sceVideoNativeExtEssential_Initialize: unreadable parameter") == 0) return ErrorInvalidArgument;
    if (ReadGuest<std::uint32_t>(static_cast<const std::byte*>(param) + 8, "sceVideoNativeExtEssential_Initialize: unreadable parameter") != 1) {
        return ErrorInvalidArgument;
    }
    NotImplemented_nid_no_patch(__func__);
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
    (void)argument4;
    (void)argument5;
    if (output == nullptr) return ErrorInvalidArgument;
    if (type != 2 && type != 3 && type != 4) return ErrorUnknownType;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iwAQhvb9BvY", libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch);

}

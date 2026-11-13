#include "prx/libSceEditMp4/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidParam = static_cast<int>(0x814B0001u);
constexpr int ErrorNullArgument = static_cast<int>(0x814B0002u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x814B0003u);

bool IsOpenHandle(const void* handle) {
    (void)handle;
    return false;
}

} // namespace

extern "C" {

int APS5_VABI sceEditMp4Abort(const void* handle) {
    if (handle == nullptr || !IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEditMp4Edit(const void* handle, const void* argument2, const void* argument3, const void* argument4) {
    if (handle == nullptr) return ErrorInvalidHandle;
    if (argument2 == nullptr || argument3 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEditMp4GetStatus(const void* handle, void* status) {
    if (handle == nullptr) return ErrorInvalidHandle;
    if (status == nullptr) return ErrorNullArgument;
    if (!IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEditMp4Initialize(const void* param, void* handle) {
    if (param == nullptr || handle == nullptr) return ErrorNullArgument;
    std::uint64_t first = 0;
    if (!GuestCopyIn_nid_no_patch(&first, param, sizeof(first))) throw std::invalid_argument("libSceEditMp4: unreadable param");
    if (first == 0) return ErrorInvalidParam;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI sceEditMp4InitializeParam(void* param) {
    if (param == nullptr) return;
    std::array<std::uint8_t, 0x1c> initial{};
    const std::uint64_t at10 = 0x3f;
    const std::uint32_t at18 = 700;
    std::memcpy(initial.data() + 0x10, &at10, sizeof(at10));
    std::memcpy(initial.data() + 0x18, &at18, sizeof(at18));
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(param, initial.data(), initial.size());
    errno = savedErrno;
}

std::uint64_t APS5_VABI sceEditMp4QueryMemSize() {
    return 0x387d30;
}

int APS5_VABI sceEditMp4SetFinishCallback(const void* handle) {
    if (handle == nullptr || !IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEditMp4Terminate(const void* handle) {
    if (handle == nullptr || !IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEditMp4Unite(const void* handle, const void* argument2, const void* argument3, const void* argument4) {
    if (handle == nullptr) return ErrorInvalidHandle;
    if (argument2 == nullptr || argument3 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!IsOpenHandle(handle)) return ErrorInvalidHandle;
    NotImplemented_nid_no_patch(__func__);
}

}

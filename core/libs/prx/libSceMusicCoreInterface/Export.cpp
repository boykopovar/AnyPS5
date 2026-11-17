#include "prx/libSceMusicCoreInterface/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81795002u);
constexpr int ErrorNotInitialized = static_cast<int>(0x81795003u);
constexpr std::uint64_t InterfaceParamSize = 0x10;

bool HasInterface() {
    return false;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceMusicCoreIfInitializeInterface(const void* argument1, const void* param) {
    if (argument1 == nullptr || param == nullptr) return ErrorInvalidArgument;
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, param, sizeof(size))) {
        throw std::invalid_argument("sceMusicCoreIfInitializeInterface: unreadable parameter");
    }
    if (size != InterfaceParamSize) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfSetFunctionTable(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!HasInterface()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfMainLoop() {
    if (!HasInterface()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfSetInternalFunctionTable(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!HasInterface()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfSetFunctionTableExt2(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!HasInterface()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfSetFunctionTableUnlimited(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!HasInterface()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicCoreIfInternalInitializeInterface(const char* name, const void* argument2) {
    if (name == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

}

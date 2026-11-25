#include "prx/libSceVideoCoreInterface/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81793001u);
constexpr int ErrorNotInitialized = static_cast<int>(0x81793002u);

bool IsInitialized() {
    return false;
}

} // namespace

extern "C" {

int APS5_VABI sceVideoCoreIfSetFunctionTableUnlimited(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!IsInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreIfInitializeInterface(void* argument1, void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreIfMainLoop() {
    if (!IsInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreIfSetFunctionTable(const void* table) {
    if (table == nullptr) return ErrorInvalidArgument;
    if (!IsInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

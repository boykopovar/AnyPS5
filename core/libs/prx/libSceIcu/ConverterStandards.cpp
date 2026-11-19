#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno guard;
    return GuestCopyIn_nid_no_patch(destination, source, bytes);
}

bool CopyOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno guard;
    return GuestCopyOut_nid_no_patch(destination, source, bytes);
}

bool WritablePreservingErrno(void* destination, std::size_t bytes) {
    PreservedErrno guard;
    return GuestWritable_nid_no_patch(destination, bytes);
}

}

extern "C" {

const char* APS5_VABI ucnv_getStandard_nid_postfix(std::uint16_t index, std::int32_t* errorCode) {
    if (!errorCode) APS5_INVALID_ARG_EX;
    std::int32_t previous = 0;
    if (!CopyInPreservingErrno(&previous, errorCode, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > 0) return nullptr;
    if (!WritablePreservingErrno(errorCode, sizeof(*errorCode))) NotImplemented_nid_no_patch(__func__);
    auto status = static_cast<UErrorCode>(previous);
    const char* result = ucnv_getStandard(index, &status);
    const auto updated = static_cast<std::int32_t>(status);
    if (updated != previous && !CopyOutPreservingErrno(errorCode, &updated, sizeof(updated)))
        NotImplemented_nid_no_patch(__func__);
    return result;
}

}

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno preserved;
    return GuestCopyIn_nid_no_patch(destination, source, bytes);
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno preserved;
    return GuestCopyOut_nid_no_patch(destination, source, bytes);
}

bool Writable(void* destination, std::size_t bytes) {
    PreservedErrno preserved;
    return GuestWritable_nid_no_patch(destination, bytes);
}

std::string ReadAlias(const char* alias, const char* caller) {
    std::string copy;
    auto address = reinterpret_cast<std::uintptr_t>(alias);
    for (;;) {
        char value = 0;
        if (!CopyIn(&value, reinterpret_cast<const void*>(address), sizeof(value)))
            NotImplemented_nid_no_patch(caller);
        if (value == 0) return copy;
        if (copy.size() < UCNV_MAX_CONVERTER_NAME_LENGTH) copy.push_back(value);
        if (address == std::numeric_limits<std::uintptr_t>::max()) NotImplemented_nid_no_patch(caller);
        ++address;
    }
}

void WriteStatus(std::int32_t* error, UErrorCode status, std::int32_t initial, const char* caller) {
    const auto result = static_cast<std::int32_t>(status);
    if (result != initial && !CopyOut(error, &result, sizeof(result))) NotImplemented_nid_no_patch(caller);
}

}

extern "C" void APS5_VABI ucnv_getAliases_nid_postfix(const char* alias, const char** aliases,
                                                       std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t initial = 0;
    if (!CopyIn(&initial, error, sizeof(initial))) NotImplemented_nid_no_patch(__func__);
    UErrorCode status = static_cast<UErrorCode>(initial);
    if (U_FAILURE(status)) return;
    if (!Writable(error, sizeof(*error))) NotImplemented_nid_no_patch(__func__);

    ucnv_countAliases("", &status);
    if (U_FAILURE(status)) {
        WriteStatus(error, status, initial, __func__);
        return;
    }

    std::string aliasCopy;
    const char* nativeAlias = nullptr;
    if (alias) {
        aliasCopy = ReadAlias(alias, __func__);
        nativeAlias = aliasCopy.c_str();
    }
    const auto count = ucnv_countAliases(nativeAlias, &status);
    if (U_FAILURE(status) || count == 0) {
        WriteStatus(error, status, initial, __func__);
        return;
    }
    const std::size_t bytes = static_cast<std::size_t>(count) * sizeof(*aliases);
    if (!aliases || !Writable(aliases, bytes)) NotImplemented_nid_no_patch(__func__);
    std::vector<const char*> nativeAliases(count);
    ucnv_getAliases(nativeAlias, nativeAliases.data(), &status);
    if (!CopyOut(aliases, nativeAliases.data(), bytes)) NotImplemented_nid_no_patch(__func__);
    WriteStatus(error, status, initial, __func__);
}

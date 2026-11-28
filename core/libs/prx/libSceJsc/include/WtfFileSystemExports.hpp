#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfBase64Exports.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct CStringBuffer {
    unsigned refCount;
    std::size_t length;
};

struct CString {
    CStringBuffer* buffer;
};

struct OptionalInt32 {
    bool hasValue{};
    std::int32_t value{};

    OptionalInt32() = default;
    OptionalInt32(bool engaged, std::int32_t number) : hasValue(engaged), value(number) {}
    OptionalInt32(const OptionalInt32& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalInt32& operator=(const OptionalInt32& other) = default;
};

static_assert(sizeof(CStringBuffer) == 16);
static_assert(sizeof(CString) == 8);
static_assert(sizeof(OptionalInt32) == 8);
static_assert(offsetof(OptionalInt32, hasValue) == 0);
static_assert(offsetof(OptionalInt32, value) == 4);

} // namespace Jsc::WtfAbi

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl12readFromFileEiPci(int handle, char* data, int length);
Jsc::WtfAbi::OptionalInt32 APS5_VABI _ZN3WTF14FileSystemImpl15getFileDeviceIdERKNS_7CStringE(const Jsc::WtfAbi::CString* fsFile);
Jsc::WtfAbi::StringImpl** APS5_VABI _ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_(Jsc::WtfAbi::StringImpl** result, const Jsc::WtfAbi::String* path, const Jsc::WtfAbi::String* component);

}

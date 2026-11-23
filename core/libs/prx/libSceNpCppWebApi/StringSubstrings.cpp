#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int StringOutOfMemory = static_cast<int>(0x80553504);
constexpr int StringPositionError = static_cast<int>(0x8055351a);
constexpr int StringLengthError = static_cast<int>(0x8055351c);

std::string_view StringText(const CommonString* value) {
    return value->buffer ? value->buffer : "";
}

}

extern "C" {

std::size_t _ZN3sce2Np9CppWebApi6Common6String4nposE = std::string_view::npos;

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String4findERKS3_(const CommonString* self, const CommonString* needle) {
    return _ZNK3sce2Np9CppWebApi6Common6String4findERKS3_m(self, needle, 0);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6String4findERKS3_m(const CommonString* self, const CommonString* needle, std::size_t position) {
    const auto found = StringText(self).find(StringText(needle), position);
    return found == std::string_view::npos ? _ZN3sce2Np9CppWebApi6Common6String4nposE : found;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6String7replaceEmmRS3_(CommonString* self, std::size_t position, std::size_t count, CommonString* replacement) {
    const auto source = StringText(self);
    if (position > source.size()) return StringPositionError;
    if (!self->buffer && !replacement->buffer) return 0;
    const auto* context = static_cast<const std::uint64_t*>(self->libContext);
    if (!context || !*context) return StringOutOfMemory;
    const auto inserted = StringText(replacement);
    const auto removed = std::min(count, source.size() - position);
    const auto retained = source.size() - removed;
    if (retained == 0) return _ZN3sce2Np9CppWebApi6Common6String8copyFromEPKc(self, inserted.data());
    if (inserted.size() >= std::numeric_limits<std::size_t>::max() - retained) return StringLengthError;
    try {
        std::string result;
        result.reserve(retained + inserted.size());
        result.append(source.substr(0, position));
        result.append(inserted);
        result.append(source.substr(position + removed));
        return _ZN3sce2Np9CppWebApi6Common6String8copyFromEPKc(self, result.c_str());
    } catch (const std::length_error&) {
        return StringLengthError;
    } catch (const std::bad_alloc&) {
        return StringOutOfMemory;
    }
}

}

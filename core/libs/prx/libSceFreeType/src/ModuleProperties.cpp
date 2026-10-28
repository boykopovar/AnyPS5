#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include FT_MODULE_H

#include <cstdint>
#include <cstring>

using namespace FreeType;

static_assert(sizeof(FT_UInt) == sizeof(std::uint32_t));
static_assert(sizeof(FT_Int32) == sizeof(std::int32_t));
static_assert(sizeof(FT_Int) == sizeof(std::int32_t));
static_assert(sizeof(FT_Bool) == sizeof(std::uint8_t));

namespace {

enum class PropertyType { Unsigned, Signed, Boolean, Darkening, Unsupported };
using DarkeningParameters = FT_Int[8];

PropertyType TypeOf(const char* moduleName, const char* propertyName) {
    const bool postScript = std::strcmp(moduleName, "cff") == 0 || std::strcmp(moduleName, "type1") == 0 ||
        std::strcmp(moduleName, "t1cid") == 0;
    const bool autoHinter = std::strcmp(moduleName, "autofitter") == 0;
    if (std::strcmp(moduleName, "truetype") == 0 && std::strcmp(propertyName, "interpreter-version") == 0) {
        return PropertyType::Unsigned;
    }
    if (postScript && std::strcmp(propertyName, "hinting-engine") == 0) return PropertyType::Unsigned;
    if (postScript && std::strcmp(propertyName, "random-seed") == 0) return PropertyType::Signed;
    if (postScript || autoHinter) {
        if (std::strcmp(propertyName, "no-stem-darkening") == 0) return PropertyType::Boolean;
        if (std::strcmp(propertyName, "darkening-parameters") == 0) return PropertyType::Darkening;
    }
    return PropertyType::Unsupported;
}

template <typename TValue>
int Read(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    TValue value{};
    const auto error = FT_Property_Get(library, moduleName, propertyName, &value);
    if (!error) std::memcpy(output, &value, sizeof(value));
    return error;
}

template <typename TValue>
int Write(FT_Library library, const char* moduleName, const char* propertyName, const void* input) {
    TValue value{};
    std::memcpy(&value, input, sizeof(value));
    return FT_Property_Set(library, moduleName, propertyName, &value);
}

}

extern "C" int APS5_VABI FT_Property_Get_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, void* value) {
    const auto host = library ? library->host : nullptr;
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Get(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName)) {
        case PropertyType::Unsigned: return Read<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Read<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Read<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Read<DarkeningParameters>(host, moduleName, propertyName, value);
        default: NotImplemented_nid_no_patch(__func__);
    }
}

extern "C" int APS5_VABI FT_Property_Set_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, const void* value) {
    const auto host = library ? library->host : nullptr;
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Set(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName)) {
        case PropertyType::Unsigned: return Write<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Write<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Write<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Write<DarkeningParameters>(host, moduleName, propertyName, value);
        default: NotImplemented_nid_no_patch(__func__);
    }
}

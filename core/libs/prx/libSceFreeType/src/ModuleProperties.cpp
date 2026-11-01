#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include FT_MODULE_H
#include FT_DRIVER_H

#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace FreeType;

struct GuestGlyphToScriptMap {
    GuestFaceRec* face;
    std::uint16_t* map;
};

static int ReadGlyphToScriptMap(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    GuestFaceRec* face = nullptr;
    std::memcpy(&face, output, sizeof(face));
    const GuestFace* owner = FaceOf(face);
    FT_Prop_GlyphToScriptMap property{owner ? owner->host : nullptr, nullptr};
    const int error = FT_Property_Get(library, moduleName, propertyName, &property);
    if (!error) std::memcpy(static_cast<char*>(output) + offsetof(GuestGlyphToScriptMap, map), &property.map, sizeof(property.map));
    return error;
}

static bool IsGlyphToScriptMap(const char* moduleName, const char* propertyName) {
    return moduleName && propertyName && std::strcmp(moduleName, "autofitter") == 0 && std::strcmp(propertyName, "glyph-to-script-map") == 0;
}

static_assert(sizeof(FT_UInt) == sizeof(std::uint32_t));
static_assert(sizeof(FT_Int32) == sizeof(std::int32_t));
static_assert(sizeof(FT_Int) == sizeof(std::int32_t));
static_assert(sizeof(FT_Bool) == sizeof(std::uint8_t));

namespace {

enum class PropertyType { Unsigned, Signed, Boolean, Darkening, IncreaseXHeight, Unsupported };
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
    if (autoHinter && (std::strcmp(propertyName, "fallback-script") == 0 ||
        std::strcmp(propertyName, "default-script") == 0)) {
        return PropertyType::Unsigned;
    }
    if (autoHinter && std::strcmp(propertyName, "increase-x-height") == 0) return PropertyType::IncreaseXHeight;
    return PropertyType::Unsupported;
}

template <typename TValue>
int Read(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    TValue value{};
    const auto error = FT_Property_Get(library, moduleName, propertyName, &value);
    if (!error) std::memcpy(output, &value, sizeof(value));
    return error;
}

int ReadIncreaseXHeight(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    GuestFaceRec* guestFace = nullptr;
    std::memcpy(&guestFace, output, sizeof(guestFace));
    const auto face = FaceOf(guestFace);
    FT_Prop_IncreaseXHeight hostProp{};
    hostProp.face = face ? face->host : nullptr;
    const auto error = FT_Property_Get(library, moduleName, propertyName, &hostProp);
    if (!error) {
        const std::uint32_t limit = hostProp.limit;
        std::memcpy(static_cast<char*>(output) + offsetof(GuestPropIncreaseXHeight, limit), &limit, sizeof(limit));
    }
    return error;
}

template <typename TValue>
int Write(FT_Library library, const char* moduleName, const char* propertyName, const void* input) {
    TValue value{};
    std::memcpy(&value, input, sizeof(value));
    return FT_Property_Set(library, moduleName, propertyName, &value);
}

int WriteIncreaseXHeight(FT_Library library, const char* moduleName, const char* propertyName, const void* input) {
    GuestPropIncreaseXHeight guestProp{};
    std::memcpy(&guestProp, input, sizeof(guestProp));
    const auto face = FaceOf(guestProp.face);
    FT_Prop_IncreaseXHeight hostProp{};
    hostProp.face = face ? face->host : nullptr;
    hostProp.limit = guestProp.limit;
    return FT_Property_Set(library, moduleName, propertyName, &hostProp);
}

}

extern "C" int APS5_VABI FT_Property_Get_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, void* value) {
    if (library && library->host && value && IsGlyphToScriptMap(moduleName, propertyName))
        return ReadGlyphToScriptMap(library->host, moduleName, propertyName, value);
    const auto host = library ? library->host : nullptr;
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Get(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName)) {
        case PropertyType::Unsigned: return Read<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Read<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Read<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Read<DarkeningParameters>(host, moduleName, propertyName, value);
        case PropertyType::IncreaseXHeight: return ReadIncreaseXHeight(host, moduleName, propertyName, value);
        default: NotImplemented_nid_no_patch(__func__);
    }
}

extern "C" int APS5_VABI FT_Property_Set_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, const void* value) {
    if (library && library->host && value && IsGlyphToScriptMap(moduleName, propertyName)) {
        FT_Prop_GlyphToScriptMap property{};
        return FT_Property_Set(library->host, moduleName, propertyName, &property);
    }
    const auto host = library ? library->host : nullptr;
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Set(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName)) {
        case PropertyType::Unsigned: return Write<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Write<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Write<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Write<DarkeningParameters>(host, moduleName, propertyName, value);
        case PropertyType::IncreaseXHeight: return WriteIncreaseXHeight(host, moduleName, propertyName, value);
        default: NotImplemented_nid_no_patch(__func__);
    }
}

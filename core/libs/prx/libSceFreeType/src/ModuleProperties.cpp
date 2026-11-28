#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include FT_MODULE_H
#include FT_DRIVER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace FreeType;

struct GuestGlyphToScriptMap {
    GuestFaceRec* face;
    std::uint16_t* map;
};

static bool CopyGuestIn(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    return copied;
}

static bool CopyGuestOut(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = saved;
    return copied;
}

static int ReadGlyphToScriptMap(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    GuestFaceRec* face = nullptr;
    if (!CopyGuestIn(&face, output, sizeof(face))) return FT_Err_Invalid_Argument;
    const GuestFace* owner = FaceOf(face);
    FT_Prop_GlyphToScriptMap property{owner ? owner->host : nullptr, nullptr};
    const int error = FT_Property_Get(library, moduleName, propertyName, &property);
    if (error) return error;
    if (!CopyGuestOut(static_cast<char*>(output) + offsetof(GuestGlyphToScriptMap, map), &property.map, sizeof(property.map)))
        return FT_Err_Invalid_Argument;
    return FT_Err_Ok;
}

static bool IsGlyphToScriptMap(const char* moduleName, const char* propertyName) {
    return moduleName && propertyName && std::strcmp(moduleName, "autofitter") == 0 && std::strcmp(propertyName, "glyph-to-script-map") == 0;
}

static FT_Library ReadGuestLibrary(GuestLibrary* library, const char* function) {
    if (!library) return nullptr;
    FT_Library host = nullptr;
    const bool read = reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) == 0 && GuestCopyIn_nid_no_patch(&host, &library->host, sizeof(host));
    if (!read) throw std::invalid_argument(std::string(function) + ": unreadable library handle");
    return host;
}

static_assert(sizeof(FT_UInt) == sizeof(std::uint32_t));
static_assert(sizeof(FT_Int32) == sizeof(std::int32_t));
static_assert(sizeof(FT_Int) == sizeof(std::int32_t));
static_assert(sizeof(FT_Bool) == sizeof(std::uint8_t));

namespace {

enum class PropertyType { Unsigned, Signed, Boolean, Darkening, IncreaseXHeight, RendererHooks, Forwarded };
using DarkeningParameters = FT_Int[8];
using HookTable = std::array<std::uintptr_t, 4>;

PropertyType TypeOf(const char* moduleName, const char* propertyName, bool set) {
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
    if (std::strcmp(moduleName, "sdf") == 0 || std::strcmp(moduleName, "bsdf") == 0) {
        if (std::strcmp(propertyName, "spread") == 0) return set ? PropertyType::Signed : PropertyType::Unsigned;
        if (std::strcmp(propertyName, "flip_sign") == 0 || std::strcmp(propertyName, "flip_y") == 0) return PropertyType::Signed;
        if (std::strcmp(propertyName, "overlaps") == 0) return set ? PropertyType::Boolean : PropertyType::Signed;
    }
    if (std::strcmp(moduleName, "pcf") == 0 && std::strcmp(propertyName, "no-long-family-names") == 0) return PropertyType::Boolean;
    if (std::strcmp(moduleName, "ot-svg") == 0 && std::strcmp(propertyName, "svg-hooks") == 0) return PropertyType::RendererHooks;
    return PropertyType::Forwarded;
}

template <typename TValue>
int Read(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    TValue value{};
    const auto error = FT_Property_Get(library, moduleName, propertyName, &value);
    if (error) return error;
    return CopyGuestOut(output, &value, sizeof(value)) ? FT_Err_Ok : FT_Err_Invalid_Argument;
}

int ReadIncreaseXHeight(FT_Library library, const char* moduleName, const char* propertyName, void* output) {
    GuestFaceRec* guestFace = nullptr;
    if (!CopyGuestIn(&guestFace, output, sizeof(guestFace))) return FT_Err_Invalid_Argument;
    const auto face = FaceOf(guestFace);
    FT_Prop_IncreaseXHeight hostProp{};
    hostProp.face = face ? face->host : nullptr;
    const auto error = FT_Property_Get(library, moduleName, propertyName, &hostProp);
    if (!error) {
        const std::uint32_t limit = hostProp.limit;
        if (!CopyGuestOut(static_cast<char*>(output) + offsetof(GuestPropIncreaseXHeight, limit), &limit, sizeof(limit)))
            return FT_Err_Invalid_Argument;
    }
    return error;
}

template <typename TValue>
int Write(FT_Library library, const char* moduleName, const char* propertyName, const void* input) {
    TValue value{};
    if (!CopyGuestIn(&value, input, sizeof(value))) return FT_Err_Invalid_Argument;
    return FT_Property_Set(library, moduleName, propertyName, &value);
}

int WriteIncreaseXHeight(FT_Library library, const char* moduleName, const char* propertyName, const void* input) {
    GuestPropIncreaseXHeight guestProp{};
    if (!CopyGuestIn(&guestProp, input, sizeof(guestProp))) return FT_Err_Invalid_Argument;
    const auto face = FaceOf(guestProp.face);
    FT_Prop_IncreaseXHeight hostProp{};
    hostProp.face = face ? face->host : nullptr;
    hostProp.limit = guestProp.limit;
    return FT_Property_Set(library, moduleName, propertyName, &hostProp);
}

int WriteRendererHooks(FT_Library library, const char* moduleName, const char* propertyName, const void* input, const char* function) {
    HookTable hooks{};
    if (!CopyGuestIn(&hooks, input, sizeof(hooks))) return FT_Err_Invalid_Argument;
    if (std::find(hooks.begin(), hooks.end(), std::uintptr_t{0}) == hooks.end()) {
        HookTable current{};
        if (FT_Property_Get(library, moduleName, propertyName, &current) == FT_Err_Ok) NotImplemented_nid_no_patch(function);
    }
    return FT_Property_Set(library, moduleName, propertyName, &hooks);
}

}

extern "C" int APS5_VABI FT_Property_Get_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, void* value) {
    const FT_Library host = ReadGuestLibrary(library, __func__);
    if (host && value && IsGlyphToScriptMap(moduleName, propertyName))
        return ReadGlyphToScriptMap(host, moduleName, propertyName, value);
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Get(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName, false)) {
        case PropertyType::Unsigned: return Read<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Read<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Read<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Read<DarkeningParameters>(host, moduleName, propertyName, value);
        case PropertyType::IncreaseXHeight: return ReadIncreaseXHeight(host, moduleName, propertyName, value);
        case PropertyType::RendererHooks: return Read<HookTable>(host, moduleName, propertyName, value);
        case PropertyType::Forwarded: break;
    }
    return FT_Property_Get(host, moduleName, propertyName, value);
}

extern "C" int APS5_VABI FT_Property_Set_nid_postfix(GuestLibrary* library, const char* moduleName,
    const char* propertyName, const void* value) {
    const FT_Library host = ReadGuestLibrary(library, __func__);
    if (host && value && IsGlyphToScriptMap(moduleName, propertyName)) {
        FT_Prop_GlyphToScriptMap property{};
        return FT_Property_Set(host, moduleName, propertyName, &property);
    }
    if (!host || !moduleName || !propertyName || !value) return FT_Property_Set(host, moduleName, propertyName, value);
    switch (TypeOf(moduleName, propertyName, true)) {
        case PropertyType::Unsigned: return Write<FT_UInt>(host, moduleName, propertyName, value);
        case PropertyType::Signed: return Write<FT_Int32>(host, moduleName, propertyName, value);
        case PropertyType::Boolean: return Write<FT_Bool>(host, moduleName, propertyName, value);
        case PropertyType::Darkening: return Write<DarkeningParameters>(host, moduleName, propertyName, value);
        case PropertyType::IncreaseXHeight: return WriteIncreaseXHeight(host, moduleName, propertyName, value);
        case PropertyType::RendererHooks: return WriteRendererHooks(host, moduleName, propertyName, value, __func__);
        case PropertyType::Forwarded: break;
    }
    return FT_Property_Set(host, moduleName, propertyName, value);
}

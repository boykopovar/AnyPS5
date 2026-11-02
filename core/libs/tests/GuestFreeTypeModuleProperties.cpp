#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_DRIVER_H

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestLibrary;

struct Api {
    int (APS5_VABI *initialize)(GuestLibrary**);
    int (APS5_VABI *done)(GuestLibrary*);
    int (APS5_VABI *get)(GuestLibrary*, const char*, const char*, void*);
    int (APS5_VABI *set)(GuestLibrary*, const char*, const char*, const void*);
    int (APS5_VABI *newMemoryFace)(GuestLibrary*, const unsigned char*, std::int64_t, std::int64_t, FreeType::GuestFaceRec**);
    int (APS5_VABI *doneFace)(FreeType::GuestFaceRec*);
};

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Module property check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TValue>
static auto NativePointer(TValue& value) {
    if constexpr (requires { value.data(); }) return value.data();
    else return &value;
}

template <typename TValue>
static void Query(const Api& api, GuestLibrary* library, FT_Library native, const char* module, const char* property) {
    TValue expected{};
    std::memset(&expected, 0xA5, sizeof(expected));
    const int expectedError = FT_Property_Get(native, module, property, NativePointer(expected));
    std::array<unsigned char, 64> storage{};
    storage.fill(0xA5);
    const auto before = storage;
    auto* output = storage.data() + 7;
    Require(api.get(library, module, property, output) == expectedError);
    if (expectedError) Require(storage == before);
    else {
        Require(std::memcmp(output, &expected, sizeof(expected)) == 0);
        Require(std::all_of(storage.begin(), storage.begin() + 7, [](auto byte) { return byte == 0xA5; }));
        Require(std::all_of(storage.begin() + 7 + sizeof(expected), storage.end(), [](auto byte) { return byte == 0xA5; }));
    }
}

template <typename TValue>
static void Change(const Api& api, GuestLibrary* library, FT_Library native, GuestLibrary* other,
    FT_Library nativeOther, const char* module, const char* property, TValue value) {
    std::array<unsigned char, 64> input{};
    input.fill(0xCC);
    std::memcpy(input.data() + 5, &value, sizeof(value));
    const auto before = input;
    const int expectedError = FT_Property_Set(native, module, property, NativePointer(value));
    Require(api.set(library, module, property, input.data() + 5) == expectedError);
    Require(input == before);
    Query<TValue>(api, library, native, module, property);
    Query<TValue>(api, other, nativeOther, module, property);
}

static void QueryIncreaseXHeight(const Api& api, GuestLibrary* library, FT_Library native,
    FreeType::GuestFaceRec* guestFace, FT_Face nativeFace) {
    FT_Prop_IncreaseXHeight expected{};
    expected.face = nativeFace;
    expected.limit = 0xA5A5A5A5u;
    const int expectedError = FT_Property_Get(native, "autofitter", "increase-x-height", &expected);
    std::array<unsigned char, 64> storage{};
    storage.fill(0xA5);
    auto* carrier = storage.data() + 8;
    std::memcpy(carrier, &guestFace, sizeof(guestFace));
    auto expectedStorage = storage;
    if (!expectedError) {
        std::memcpy(expectedStorage.data() + 8 + offsetof(FreeType::GuestPropIncreaseXHeight, limit),
            &expected.limit, sizeof(expected.limit));
    }
    const int actualError = api.get(library, "autofitter", "increase-x-height", carrier);
    Require(actualError == expectedError);
    Require(storage == expectedStorage);
}

static void ChangeIncreaseXHeight(const Api& api, GuestLibrary* library, FT_Library native,
    FreeType::GuestFaceRec* guestFace, FT_Face nativeFace, std::uint32_t limit) {
    std::array<unsigned char, 64> input{};
    input.fill(0xCC);
    auto* carrier = input.data() + 8;
    std::memcpy(carrier, &guestFace, sizeof(guestFace));
    std::memcpy(carrier + offsetof(FreeType::GuestPropIncreaseXHeight, limit), &limit, sizeof(limit));
    const auto before = input;
    FT_Prop_IncreaseXHeight nativeProp{};
    nativeProp.face = nativeFace;
    nativeProp.limit = limit;
    const int expectedError = FT_Property_Set(native, "autofitter", "increase-x-height", &nativeProp);
    const int actualError = api.set(library, "autofitter", "increase-x-height", carrier);
    Require(actualError == expectedError);
    Require(input == before);
}

static void Run(const Api& api) {
    GuestLibrary* library = nullptr;
    GuestLibrary* other = nullptr;
    FT_Library native = nullptr;
    FT_Library nativeOther = nullptr;
    Require(api.initialize(&library) == 0 && api.initialize(&other) == 0);
    Require(FT_Init_FreeType(&native) == 0 && FT_Init_FreeType(&nativeOther) == 0);
    Query<std::uint32_t>(api, library, native, "truetype", "interpreter-version");
    for (const auto version : {35u, 40u, 38u, 0u, 0xFFFFFFFFu}) {
        Change(api, library, native, other, nativeOther, "truetype", "interpreter-version", version);
    }
    const std::array<std::array<std::int32_t, 8>, 5> parameters{{
        {500, 300, 1000, 200, 1500, 100, 2000, 0},
        {600, 250, 1100, 150, 1600, 50, 2500, 0},
        {-1, 0, 1000, 0, 1500, 0, 2000, 0},
        {1000, 0, 500, 0, 1500, 0, 2000, 0},
        {500, 501, 1000, 0, 1500, 0, 2000, 0}
    }};
    for (const char* module : {"cff", "type1", "t1cid", "autofitter"}) {
        Query<std::uint8_t>(api, library, native, module, "no-stem-darkening");
        for (const auto value : {0u, 1u, 2u, 255u}) {
            Change(api, library, native, other, nativeOther, module, "no-stem-darkening", static_cast<std::uint8_t>(value));
        }
        Query<std::array<std::int32_t, 8>>(api, library, native, module, "darkening-parameters");
        for (const auto& value : parameters) Change(api, library, native, other, nativeOther, module, "darkening-parameters", value);
        if (std::strcmp(module, "autofitter") == 0) continue;
        for (const auto value : {1u, 0u, 2u, 0xFFFFFFFFu}) {
            Change(api, library, native, other, nativeOther, module, "hinting-engine", value);
        }
        for (const auto value : {std::numeric_limits<std::int32_t>::min(), -1, 0, 1, std::numeric_limits<std::int32_t>::max()}) {
            Change(api, library, native, other, nativeOther, module, "random-seed", value);
        }
    }
    for (const auto* property : {"fallback-script", "default-script"}) {
        Query<std::uint32_t>(api, library, native, "autofitter", property);
        for (const auto script : {
            999u,
            0xFFFFFFFFu,
            static_cast<std::uint32_t>(FT_AUTOHINTER_SCRIPT_NONE),
            static_cast<std::uint32_t>(FT_AUTOHINTER_SCRIPT_LATIN),
            static_cast<std::uint32_t>(FT_AUTOHINTER_SCRIPT_CJK),
            static_cast<std::uint32_t>(FT_AUTOHINTER_SCRIPT_INDIC)
        }) {
            Change(api, library, native, other, nativeOther, "autofitter", property, script);
        }
    }
    std::array<unsigned char, 64> output{};
    output.fill(0xA5);
    const auto before = output;
    Require(api.get(nullptr, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), output.data()) ==
        FT_Property_Get(nullptr, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), output.data()));
    Require(api.set(nullptr, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), reinterpret_cast<const void*>(1)) ==
        FT_Property_Set(nullptr, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), reinterpret_cast<const void*>(1)));
    Require(api.get(library, nullptr, "interpreter-version", output.data()) == FT_Property_Get(native, nullptr, "interpreter-version", output.data()));
    Require(api.get(library, "truetype", nullptr, output.data()) == FT_Property_Get(native, "truetype", nullptr, output.data()));
    Require(api.get(library, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), nullptr) ==
        FT_Property_Get(native, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), nullptr));
    Require(api.set(library, nullptr, "interpreter-version", output.data()) == FT_Property_Set(native, nullptr, "interpreter-version", output.data()));
    Require(api.set(library, "truetype", nullptr, output.data()) == FT_Property_Set(native, "truetype", nullptr, output.data()));
    Require(api.set(library, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), nullptr) ==
        FT_Property_Set(native, reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), nullptr));
    Require(output == before);
    Query<std::uint32_t>(api, library, native, "truetype", "interpreter-version");
    Query<std::uint32_t>(api, other, nativeOther, "truetype", "interpreter-version");
    const auto fontBytes = SquareGlyphFont(65, 600);
    FreeType::GuestFaceRec* face1 = nullptr;
    FT_Face nativeFace1 = nullptr;
    Require(api.newMemoryFace(library, fontBytes.data(), static_cast<std::int64_t>(fontBytes.size()), 0, &face1) == 0);
    Require(FT_New_Memory_Face(native, fontBytes.data(), static_cast<FT_Long>(fontBytes.size()), 0, &nativeFace1) == 0);
    FreeType::GuestFaceRec* face2 = nullptr;
    FT_Face nativeFace2 = nullptr;
    Require(api.newMemoryFace(library, fontBytes.data(), static_cast<std::int64_t>(fontBytes.size()), 0, &face2) == 0);
    Require(FT_New_Memory_Face(native, fontBytes.data(), static_cast<FT_Long>(fontBytes.size()), 0, &nativeFace2) == 0);
    FreeType::GuestFaceRec* face3 = nullptr;
    FT_Face nativeFace3 = nullptr;
    Require(api.newMemoryFace(other, fontBytes.data(), static_cast<std::int64_t>(fontBytes.size()), 0, &face3) == 0);
    Require(FT_New_Memory_Face(nativeOther, fontBytes.data(), static_cast<FT_Long>(fontBytes.size()), 0, &nativeFace3) == 0);
    QueryIncreaseXHeight(api, library, native, face1, nativeFace1);
    QueryIncreaseXHeight(api, library, native, face2, nativeFace2);
    QueryIncreaseXHeight(api, other, nativeOther, face3, nativeFace3);
    for (const auto limit : {0u, 6u, 20u, 42u, std::numeric_limits<std::uint32_t>::max()}) {
        ChangeIncreaseXHeight(api, library, native, face1, nativeFace1, limit);
        QueryIncreaseXHeight(api, library, native, face1, nativeFace1);
        QueryIncreaseXHeight(api, library, native, face2, nativeFace2);
    }
    ChangeIncreaseXHeight(api, library, native, face2, nativeFace2, 20u);
    QueryIncreaseXHeight(api, library, native, face2, nativeFace2);
    QueryIncreaseXHeight(api, library, native, face1, nativeFace1);
    ChangeIncreaseXHeight(api, library, native, face3, nativeFace3, 42u);
    QueryIncreaseXHeight(api, library, native, face3, nativeFace3);
    QueryIncreaseXHeight(api, other, nativeOther, face3, nativeFace3);
    QueryIncreaseXHeight(api, library, native, face1, nativeFace1);
    QueryIncreaseXHeight(api, library, native, face2, nativeFace2);
    ChangeIncreaseXHeight(api, library, native, nullptr, nullptr, 99u);
    QueryIncreaseXHeight(api, library, native, nullptr, nullptr);
    Require(api.doneFace(face1) == 0 && FT_Done_Face(nativeFace1) == 0);
    Require(api.doneFace(face2) == 0 && FT_Done_Face(nativeFace2) == 0);
    Require(api.doneFace(face3) == 0 && FT_Done_Face(nativeFace3) == 0);
    Require(api.done(library) == 0 && api.done(other) == 0);
    Require(FT_Done_FreeType(native) == 0 && FT_Done_FreeType(nativeOther) == 0);
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name, const char* nid, bool patched) {
    const std::string symbol = patched ? nid : std::string(name) + "_nid_postfix";
#ifdef _WIN32
    const auto address = GetProcAddress(module, symbol.c_str());
#else
    const auto address = dlsym(module, symbol.c_str());
#endif
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    Api api{};
    api.initialize = Import<decltype(api.initialize)>(module, "FT_Init_FreeType", "GNsiLzm4SH8", patched);
    api.done = Import<decltype(api.done)>(module, "FT_Done_FreeType", "dH02CpEEcYk", patched);
    api.get = Import<decltype(api.get)>(module, "FT_Property_Get", "8fAMdZCnewA", patched);
    api.set = Import<decltype(api.set)>(module, "FT_Property_Set", "LsB538+IPrE", patched);
    api.newMemoryFace = Import<decltype(api.newMemoryFace)>(module, "FT_New_Memory_Face", "40Wa2neSLM0", patched);
    api.doneFace = Import<decltype(api.doneFace)>(module, "FT_Done_Face", "OgFczHko0w8", patched);
    Run(api);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}

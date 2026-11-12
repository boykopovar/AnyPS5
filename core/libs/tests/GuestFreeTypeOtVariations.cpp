#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

struct Exports {
    decltype(&FT_Init_FreeType_nid_postfix) init;
    decltype(&FT_Done_FreeType_nid_postfix) finish;
    decltype(&FT_New_Memory_Face_nid_postfix) newMemoryFace;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&FT_Get_Var_Design_Coordinates_nid_postfix) getDesign;
    decltype(&FT_Get_Var_Blend_Coordinates_nid_postfix) getBlend;
    decltype(&FT_Set_Var_Design_Coordinates_nid_postfix) setDesign;
    decltype(&FT_Set_Var_Blend_Coordinates_nid_postfix) setBlend;
    decltype(&FT_Set_MM_Design_Coordinates_nid_postfix) setMMDesign;
    decltype(&FT_Set_MM_Blend_Coordinates_nid_postfix) setMMBlend;
};

Exports ot;
FreeType::GuestLibrary* guestLibrary = nullptr;
FT_Library nativeLibrary = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt variations: %s\n", message);
    std::exit(1);
}

std::vector<unsigned char> VariableFont() {
    const std::vector<unsigned char> fvar = FontFixture::Words({1, 0, 16, 2, 1, 20, 0, 8, 0x7767, 0x6874, 0x0064, 0, 0x0190, 0, 0x0384, 0, 0, 256});
    const std::vector<unsigned char> gvar = FontFixture::Words({1, 0, 1, 0, 0, 0, 3, 0, 0, 28, 0, 0, 0, 0});
    return FontFixture::BuildFont(3, {{"fvar", fvar}, {"gvar", gvar}});
}

struct Faces {
    std::vector<unsigned char> bytes;
    FreeType::GuestFaceRec* guest = nullptr;
    FT_Face native = nullptr;

    explicit Faces(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(ot.newMemoryFace(guestLibrary, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0 && guest, "open guest face");
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0, "open native face");
    }

    ~Faces() {
        Require(ot.doneFace(guest) == 0, "close guest face");
        Require(FT_Done_Face(native) == 0, "close native face");
    }

    void SameCoordinates() const {
        std::int64_t design[2] = {7, 7};
        std::int64_t blend[2] = {7, 7};
        FT_Fixed nativeDesign[2] = {7, 7};
        FT_Fixed nativeBlend[2] = {7, 7};
        Require(ot.getDesign(guest, 1, design) == FT_Get_Var_Design_Coordinates(native, 1, nativeDesign), "design query result");
        Require(ot.getBlend(guest, 1, blend) == FT_Get_Var_Blend_Coordinates(native, 1, nativeBlend), "blend query result");
        Require(design[0] == nativeDesign[0] && design[1] == nativeDesign[1], "design coordinates");
        Require(blend[0] == nativeBlend[0] && blend[1] == nativeBlend[1], "blend coordinates");
        Require(guest->face_flags == native->face_flags, "face flags");
    }
};

void VariationDesign() {
    for (const std::int64_t value : {0LL, 100LL << 16, 500LL << 16, 900LL << 16, 2000LL << 16, -(5LL << 16)}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Faces faces(VariableFont());
            const std::int64_t guest[2] = {value, value};
            FT_Fixed native[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(ot.setDesign(faces.guest, count, guest) == FT_Set_Var_Design_Coordinates(faces.native, count, native), "FT_Set_Var_Design_Coordinates result");
            faces.SameCoordinates();
        }
    }
    Faces plain(FontFixture::BuildFont(3, {}));
    const std::int64_t zero[1] = {0};
    FT_Fixed nativeZero[1] = {0};
    Require(ot.setDesign(plain.guest, 1, zero) == FT_Set_Var_Design_Coordinates(plain.native, 1, nativeZero), "FT_Set_Var_Design_Coordinates on a static font");
    Faces faces(VariableFont());
    Require(ot.setDesign(faces.guest, 1, nullptr) == FT_Set_Var_Design_Coordinates(faces.native, 1, nullptr), "FT_Set_Var_Design_Coordinates without coordinates");
    Require(ot.setDesign(nullptr, 0, nullptr) == FT_Err_Invalid_Face_Handle, "FT_Set_Var_Design_Coordinates without a face");
}

void VariationBlend() {
    for (const std::int64_t value : {0LL, 0x4000LL, -0x4000LL, 0x10000LL, -0x10000LL, 0x20000LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Faces faces(VariableFont());
            const std::int64_t guest[2] = {value, value};
            FT_Fixed native[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(ot.setBlend(faces.guest, count, guest) == FT_Set_Var_Blend_Coordinates(faces.native, count, native), "FT_Set_Var_Blend_Coordinates result");
            faces.SameCoordinates();
        }
    }
    Faces plain(FontFixture::BuildFont(3, {}));
    const std::int64_t zero[1] = {0};
    FT_Fixed nativeZero[1] = {0};
    Require(ot.setBlend(plain.guest, 1, zero) == FT_Set_Var_Blend_Coordinates(plain.native, 1, nativeZero), "FT_Set_Var_Blend_Coordinates on a static font");
    Faces faces(VariableFont());
    Require(ot.setBlend(faces.guest, 1, nullptr) == FT_Set_Var_Blend_Coordinates(faces.native, 1, nullptr), "FT_Set_Var_Blend_Coordinates without coordinates");
    Require(ot.setBlend(nullptr, 0, nullptr) == FT_Set_Var_Blend_Coordinates(nullptr, 0, nullptr), "FT_Set_Var_Blend_Coordinates without a face");
}

void MultipleMasterBlend() {
    for (const std::int64_t value : {0LL, 0x4000LL, -0x4000LL, 0x10000LL, 0x20000LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Faces faces(VariableFont());
            const std::int64_t guest[2] = {value, value};
            FT_Fixed native[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(ot.setMMBlend(faces.guest, count, guest) == FT_Set_MM_Blend_Coordinates(faces.native, count, native), "FT_Set_MM_Blend_Coordinates result");
            faces.SameCoordinates();
        }
    }
    Faces plain(FontFixture::BuildFont(3, {}));
    const std::int64_t zero[1] = {0};
    FT_Fixed nativeZero[1] = {0};
    Require(ot.setMMBlend(plain.guest, 1, zero) == FT_Set_MM_Blend_Coordinates(plain.native, 1, nativeZero), "FT_Set_MM_Blend_Coordinates on a static font");
    Faces faces(VariableFont());
    Require(ot.setMMBlend(faces.guest, 1, nullptr) == FT_Set_MM_Blend_Coordinates(faces.native, 1, nullptr), "FT_Set_MM_Blend_Coordinates without coordinates");
    Require(ot.setMMBlend(nullptr, 0, nullptr) == FT_Set_MM_Blend_Coordinates(nullptr, 0, nullptr), "FT_Set_MM_Blend_Coordinates without a face");
}

void MultipleMasterDesign() {
    for (const std::int64_t value : {0LL, 100LL, 400LL, 900LL, 100LL << 16, -5LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Faces faces(VariableFont());
            const std::int64_t guest[2] = {value, value};
            FT_Long native[2] = {static_cast<FT_Long>(value), static_cast<FT_Long>(value)};
            Require(ot.setMMDesign(faces.guest, count, guest) == FT_Set_MM_Design_Coordinates(faces.native, count, native), "FT_Set_MM_Design_Coordinates result");
            faces.SameCoordinates();
        }
    }
    Faces plain(FontFixture::BuildFont(3, {}));
    const std::int64_t zero[1] = {0};
    FT_Long nativeZero[1] = {0};
    Require(ot.setMMDesign(plain.guest, 1, zero) == FT_Set_MM_Design_Coordinates(plain.native, 1, nativeZero), "FT_Set_MM_Design_Coordinates on a static font");
    Faces faces(VariableFont());
    Require(ot.setMMDesign(faces.guest, 1, nullptr) == FT_Set_MM_Design_Coordinates(faces.native, 1, nullptr), "FT_Set_MM_Design_Coordinates without coordinates");
    Require(ot.setMMDesign(nullptr, 0, nullptr) == FT_Set_MM_Design_Coordinates(nullptr, 0, nullptr), "FT_Set_MM_Design_Coordinates without a face");
}

template<class Function>
void Resolve(Function& function, void* library, bool patched, const char* nid, const char* name) {
    const char* symbol = patched ? nid : name;
#ifdef _WIN32
    function = reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), symbol));
#else
    function = reinterpret_cast<Function>(dlsym(library, symbol));
#endif
    if (!function) std::fprintf(stderr, "libSceFreeTypeOt variations: missing %s\n", symbol);
    Require(function != nullptr, "missing export");
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "expected library and export mode");
    const bool patched = std::strcmp(argv[2], "patched") == 0;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    void* library = LoadLibraryA(argv[1]);
    if (!patched) {
        std::puts("libSceFreeTypeOt variations: forwarded exports are checked through the patched module on Windows");
        return 0;
    }
    void* dependency = GetModuleHandleA("libSceFreeType.prx");
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    void* dependency = library;
#endif
    Require(library != nullptr && dependency != nullptr, "load requested library");
    Resolve(ot.init, library, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
    Resolve(ot.finish, library, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
    Resolve(ot.newMemoryFace, library, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
    Resolve(ot.doneFace, library, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");
    Resolve(ot.getDesign, dependency, patched, "tcLDimriUlI", "FT_Get_Var_Design_Coordinates_nid_postfix");
    Resolve(ot.getBlend, dependency, patched, "b5ETsuf4VH4", "FT_Get_Var_Blend_Coordinates_nid_postfix");
    Resolve(ot.setDesign, library, patched, "yCEJIsjIOO8", "FT_Set_Var_Design_Coordinates_nid_postfix");
    Resolve(ot.setBlend, library, patched, "ihIHD8BTNxI", "FT_Set_Var_Blend_Coordinates_nid_postfix");
    Resolve(ot.setMMDesign, library, patched, "DAmwG6U7YIc", "FT_Set_MM_Design_Coordinates_nid_postfix");
    Resolve(ot.setMMBlend, library, patched, "zetC8Tp1S8w", "FT_Set_MM_Blend_Coordinates_nid_postfix");
    Require(ot.init(&guestLibrary) == 0 && guestLibrary && FT_Init_FreeType(&nativeLibrary) == 0, "initialize libraries");
    VariationDesign();
    VariationBlend();
    MultipleMasterBlend();
    MultipleMasterDesign();
    Require(FT_Done_FreeType(nativeLibrary) == 0 && ot.finish(guestLibrary) == 0, "release libraries");
    std::puts("libSceFreeTypeOt: variation coordinate setters match FreeType");
    return 0;
}

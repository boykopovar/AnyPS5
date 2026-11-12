#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include <nid/NidCompute.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_PFR_H
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace FreeType;

static void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "PFR check failed at line %d\n", line);
    std::exit(1);
}
#define Require(value) Check((value), __LINE__)

static void CheckNullFace(int error) {
    if (error != FT_Err_Invalid_Argument) std::fprintf(stderr, "PFR null-face returned %d, expected %d\n", error, FT_Err_Invalid_Argument);
    Require(error == FT_Err_Invalid_Argument);
}

static void Put24(std::vector<unsigned char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    Put16(bytes, value);
}

static void Field(std::vector<unsigned char>& bytes, unsigned offset, unsigned width, std::uint32_t value) {
    for (unsigned index = 0; index < width; ++index) bytes[offset + index] = static_cast<unsigned char>(value >> (8 * (width - index - 1)));
}

static std::vector<unsigned char> PfrFont() {
    std::vector<unsigned char> physical;
    PutAll16(physical, {1, 1000, 2000, 0, -200, 1000, 800});
    physical.push_back(0x84);
    physical.insert(physical.end(), {1, 7, 4, 1, 0xFF, 0x9C, 0, 65, 66, 20});
    Put24(physical, 0);
    physical.push_back(0);
    physical.insert(physical.end(), {0, 0});
    PutAll16(physical, {0, 0, 2});
    for (const auto [code, advance] : {std::pair{65, 600}, std::pair{66, -120}}) {
        physical.push_back(static_cast<unsigned char>(code));
        Put16(physical, static_cast<std::uint16_t>(advance));
        physical.push_back(3);
        Put16(physical, 1);
    }
    std::vector<unsigned char> bytes(58);
    Field(bytes, 0, 4, 0x50465230);
    Field(bytes, 4, 2, 4);
    Field(bytes, 6, 2, 0x0D0A);
    Field(bytes, 8, 2, 58);
    Field(bytes, 10, 2, 7);
    Field(bytes, 12, 2, 58);
    Field(bytes, 14, 2, 18);
    Field(bytes, 16, 3, 18);
    Field(bytes, 19, 3, 65);
    Field(bytes, 22, 2, static_cast<std::uint32_t>(physical.size()));
    Field(bytes, 24, 3, static_cast<std::uint32_t>(physical.size()));
    Field(bytes, 27, 3, 83);
    Field(bytes, 30, 2, 3);
    Field(bytes, 32, 3, 4);
    Field(bytes, 35, 3, 83 + static_cast<std::uint32_t>(physical.size()));
    Field(bytes, 52, 2, 1);
    Field(bytes, 56, 2, 2);
    PutAll16(bytes, {1, 18});
    Put24(bytes, 65);
    for (auto value : {0x10000u, 0u, 0u, 0x10000u}) Put24(bytes, value);
    bytes.push_back(0);
    Put16(bytes, static_cast<std::uint32_t>(physical.size()));
    Put24(bytes, 83);
    bytes.insert(bytes.end(), physical.begin(), physical.end());
    bytes.insert(bytes.end(), 12, 0);
    return bytes;
}

template<typename TValue>
struct Guarded {
    std::array<std::uint8_t, 8> before{0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    TValue value;
    std::array<std::uint8_t, 8> after{0xD3, 0xD3, 0xD3, 0xD3, 0xD3, 0xD3, 0xD3, 0xD3};

    void Verify() const {
        for (auto byte : before) Require(byte == 0xA5);
        for (auto byte : after) Require(byte == 0xD3);
    }
};

static_assert(offsetof(Guarded<std::uint32_t>, after) == offsetof(Guarded<std::uint32_t>, value) + sizeof(std::uint32_t));
static_assert(offsetof(Guarded<std::int64_t>, after) == offsetof(Guarded<std::int64_t>, value) + sizeof(std::int64_t));
static_assert(offsetof(Guarded<GuestVector>, after) == offsetof(Guarded<GuestVector>, value) + sizeof(GuestVector));

static void Reference(const std::vector<unsigned char>& font) {
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    Require(FT_Init_FreeType(&library) == 0);
    Require(FT_New_Memory_Face(library, font.data(), static_cast<FT_Long>(font.size()), 0, &face) == 0);
    Require(face->num_glyphs == 3 && FT_Get_Char_Index(face, 65) == 1 && FT_Get_Char_Index(face, 66) == 2);
    Require(FT_Set_Pixel_Sizes(face, 20, 30) == 0);
    FT_UInt outline = 0, metrics = 0;
    FT_Fixed x = 0, y = 0;
    Require(FT_Get_PFR_Metrics(face, &outline, &metrics, &x, &y) == 0);
    Require(outline == 1000 && metrics == 2000 && x == FT_DivFix(20 * 64, 2000) && y == FT_DivFix(30 * 64, 2000));
    FT_Pos advance = 0;
    Require(FT_Get_PFR_Advance(face, 1, &advance) == 0 && advance == 600);
    Require(FT_Get_PFR_Advance(face, 2, &advance) == 0 && advance == -120);
    FT_Vector kerning{};
    Require(FT_Get_PFR_Kerning(face, 1, 2, &kerning) == 0 && kerning.x == -80 && kerning.y == 0);
    Require(FT_Get_Kerning(face, 1, 2, FT_KERNING_UNSCALED, &kerning) == 0 && kerning.x == -40);
    Require(FT_Load_Glyph(face, 1, FT_LOAD_NO_SCALE) == 0);
    Require(FT_Done_Face(face) == 0 && FT_Done_FreeType(library) == 0);
}

#ifdef _WIN32
using Module = HMODULE;
#else
using Module = void*;
#endif

static Module Open(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(module != nullptr);
    std::array<wchar_t, 32768> selected{};
    const auto length = GetModuleFileNameW(module, selected.data(), static_cast<DWORD>(selected.size()));
    Require(length != 0 && length < selected.size());
    Require(std::filesystem::equivalent(path, std::filesystem::path(selected.data())));
#else
    const auto module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
#endif
    return module;
}

template<typename TFunction>
static TFunction Resolve(Module module, const std::string& name, bool patched) {
    const auto symbol = patched ? Nid::ComputeNid(name, "") : name + "_nid_postfix";
#ifdef _WIN32
    const auto address = GetProcAddress(module, symbol.c_str());
#else
    const auto address = dlsym(module, symbol.c_str());
#endif
    if (!address) std::fprintf(stderr, "Missing %s (%s)\n", name.c_str(), symbol.c_str());
    Require(address != nullptr);
    return reinterpret_cast<TFunction>(address);
}

using Metrics = decltype(&FT_Get_PFR_Metrics_nid_postfix);
using Advance = decltype(&FT_Get_PFR_Advance_nid_postfix);
using Kerning = decltype(&FT_Get_PFR_Kerning_nid_postfix);

static void MetricsCases(Metrics call, GuestFaceRec* face, bool pfr) {
    Guarded<std::uint32_t> outline{.value = 0xDEADBEEFu}, metrics{.value = 0xDEADBEEFu};
    Guarded<std::int64_t> x{.value = 0x123456789ABCll}, y{.value = 0x123456789ABCll};
    for (unsigned mask = 0; mask < 16; ++mask) {
        CheckNullFace(call(nullptr, mask & 1 ? &outline.value : nullptr, mask & 2 ? &metrics.value : nullptr,
                     mask & 4 ? &x.value : nullptr, mask & 8 ? &y.value : nullptr));
        Require(outline.value == 0xDEADBEEFu && metrics.value == 0xDEADBEEFu && x.value == 0x123456789ABCll && y.value == 0x123456789ABCll);
        outline.Verify(); metrics.Verify(); x.Verify(); y.Verify();
    }
    const auto expectedX = pfr ? FT_DivFix(20 * 64, 2000) : face->size->metrics.x_scale;
    const auto expectedY = pfr ? FT_DivFix(30 * 64, 2000) : face->size->metrics.y_scale;
    for (unsigned mask = 0; mask < 16; ++mask) {
        outline.value = metrics.value = 0xDEADBEEFu;
        x.value = y.value = 0x123456789ABCll;
        Require(call(face, mask & 1 ? &outline.value : nullptr, mask & 2 ? &metrics.value : nullptr,
                     mask & 4 ? &x.value : nullptr, mask & 8 ? &y.value : nullptr) == (pfr ? 0 : FT_Err_Unknown_File_Format));
        Require(outline.value == (mask & 1 ? 1000u : 0xDEADBEEFu));
        Require(metrics.value == (mask & 2 ? pfr ? 2000u : 1000u : 0xDEADBEEFu));
        Require(x.value == (mask & 4 ? expectedX : 0x123456789ABCll));
        Require(y.value == (mask & 8 ? expectedY : 0x123456789ABCll));
        outline.Verify(); metrics.Verify(); x.Verify(); y.Verify();
    }
    Require(call(face, &outline.value, &outline.value, &x.value, &x.value) == (pfr ? 0 : FT_Err_Unknown_File_Format));
    Require(outline.value == (pfr ? 2000u : 1000u) && x.value == expectedY);
}

static void AdvanceCases(Advance call, GuestFaceRec* face, bool pfr) {
    Guarded<std::int64_t> value{.value = 0x123456789ABCll};
    CheckNullFace(call(nullptr, 1, nullptr));
    CheckNullFace(call(nullptr, 1, &value.value));
    Require(value.value == 0x123456789ABCll);
    value.Verify();
    Require(call(face, 1, nullptr) == FT_Err_Invalid_Argument);
    for (const auto index : {0u, 1u, 2u, 3u, std::numeric_limits<std::uint32_t>::max()}) {
        value.value = 0x123456789ABCll;
        const auto error = call(face, index, &value.value);
        Require(error == (pfr && (index == 1 || index == 2) ? 0 : FT_Err_Invalid_Argument));
        Require(value.value == (pfr ? index == 1 ? 600 : index == 2 ? -120 : 0 : 0x123456789ABCll));
        value.Verify();
    }
}

static void KerningCases(Kerning call, GuestFaceRec* face, bool pfr) {
    Guarded<GuestVector> value{.value = {0x123456789ABCll, -0x123456789ABCll}};
    CheckNullFace(call(nullptr, 1, 2, nullptr));
    CheckNullFace(call(nullptr, 1, 2, &value.value));
    Require(value.value.x == 0x123456789ABCll && value.value.y == -0x123456789ABCll);
    value.Verify();
    Require(call(face, 1, 2, nullptr) == FT_Err_Invalid_Argument);
    for (const auto [left, right] : {std::pair{1u, 2u}, std::pair{2u, 1u}, std::pair{0u, 2u}, std::pair{0xFFFFFFFFu, 1u}}) {
        value.value = {0x123456789ABCll, -0x123456789ABCll};
        Require(call(face, left, right, &value.value) == 0);
        Require(value.value.x == (pfr && left == 1 && right == 2 ? -80 : 0) && value.value.y == 0);
        value.Verify();
    }
}

int main(int argc, char** argv) {
    const auto pfrFont = PfrFont();
    Reference(pfrFont);
    if (argc == 2 && std::string(argv[1]) == "reference") return 0;
    Require(argc == 4);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::string(argv[2]) == "patched";
    const std::string operation = argv[3];
    Require(operation == "metrics" || operation == "advance" || operation == "kerning");
    const auto base = Open(directory / "libSceFreeType.prx");
    const auto optional = Open(directory / "libSceFreeTypeOptOl.prx");
    const auto init = Resolve<int (APS5_VABI*)(void**)>(base, "FT_Init_FreeType", patched);
    const auto done = Resolve<int (APS5_VABI*)(void*)>(base, "FT_Done_FreeType", patched);
    const auto create = Resolve<int (APS5_VABI*)(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**)>(base, "FT_New_Memory_Face", patched);
    const auto destroy = Resolve<int (APS5_VABI*)(GuestFaceRec*)>(base, "FT_Done_Face", patched);
    const auto resize = Resolve<int (APS5_VABI*)(GuestFaceRec*, std::uint32_t, std::uint32_t)>(base, "FT_Set_Pixel_Sizes", patched);
    Metrics metrics = nullptr;
    Advance advance = nullptr;
    Kerning kerning = nullptr;
    if (operation == "metrics") metrics = Resolve<Metrics>(optional, "FT_Get_PFR_Metrics", patched);
    if (operation == "advance") advance = Resolve<Advance>(optional, "FT_Get_PFR_Advance", patched);
    if (operation == "kerning") kerning = Resolve<Kerning>(optional, "FT_Get_PFR_Kerning", patched);
    void* library = nullptr;
    Require(init(&library) == 0);
    const auto ttfFont = SquareGlyphFont(65, 700);
    for (bool pfr : {true, false, true}) {
        const auto& bytes = pfr ? pfrFont : ttfFont;
        GuestFaceRec* face = nullptr;
        Require(create(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0);
        Require(resize(face, 20, 30) == 0);
        std::array<unsigned char, sizeof(GuestFaceRec)> snapshot;
        std::memcpy(snapshot.data(), face, snapshot.size());
        if (metrics) MetricsCases(metrics, face, pfr);
        if (advance) AdvanceCases(advance, face, pfr);
        if (kerning) KerningCases(kerning, face, pfr);
        Require(std::memcmp(snapshot.data(), face, snapshot.size()) == 0);
        Require(destroy(face) == 0);
    }
    Require(done(library) == 0);
    std::printf("PFR %s %s: positive/non-PFR, LP64, bounds, errors, optional outputs and lifetime passed\n", operation.c_str(), argv[2]);
}

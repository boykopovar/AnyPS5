#include "prx/libSceFreeType/include/GuestSfntTables.hpp"
#include "FreeTypeFontFixture.hpp"
#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_TRIGONOMETRY_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_RENDER_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H
#include FT_SYNTHESIS_H
#include "prx/libc/include/general/AsmMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <exception>
#include <random>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestOutline;
using FreeType::GuestOutlineFuncs;
using FreeType::GuestVector;

extern "C" {

int (APS5_VABI *FreeTypeOtValidatorSetjmpTarget)(std::uint64_t*) = nullptr;
[[gnu::returns_twice]] int APS5_VABI FreeTypeOtValidatorSetjmp(std::uint64_t* buffer);

}

asm(".text\n"
    APS5_ASM_FUNCTION("FreeTypeOtValidatorSetjmp")
    "    jmp *" APS5_ASM_SYMBOL("FreeTypeOtValidatorSetjmpTarget") "(%rip)\n"
    APS5_ASM_FUNCTION_END("FreeTypeOtValidatorSetjmp"));

namespace {

struct Exports {
    decltype(&FT_Sin_nid_postfix) sin;
    decltype(&FT_Tan_nid_postfix) tan;
    decltype(&FT_Vector_Unit_nid_postfix) vectorUnit;
    decltype(&FT_Outline_Get_Orientation_nid_postfix) orientation;
    decltype(&FT_Outline_Copy_nid_postfix) copy;
    decltype(&FT_Outline_Decompose_nid_postfix) decompose;
    decltype(&FT_Outline_Get_Bitmap_nid_postfix) outlineBitmap;
    decltype(&FT_Set_Renderer_nid_postfix) setRenderer;
};

using SetCoordinates = decltype(&FT_Set_Var_Design_Coordinates_nid_postfix);
struct VariationExports {
    SetCoordinates mmBlend;
    SetCoordinates mmDesign;
    SetCoordinates varBlend;
    SetCoordinates varDesign;
} variations;

void AttachMetrics(void* library, void* dependency, bool patched);

struct StrokerExports {
    decltype(&FT_Init_FreeType_nid_postfix) init;
    decltype(&FT_Done_FreeType_nid_postfix) finish;
    decltype(&FT_Stroker_New_nid_postfix) create;
    decltype(&FT_Stroker_Done_nid_postfix) done;
    decltype(&FT_Stroker_Set_nid_postfix) set;
    decltype(&FT_Stroker_ParseOutline_nid_postfix) parse;
    decltype(&FT_Stroker_GetBorderCounts_nid_postfix) borderCounts;
    decltype(&FT_Stroker_ExportBorder_nid_postfix) exportBorder;
};

Exports ot;
StrokerExports strokers;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt: %s\n", message);
    std::exit(1);
}

struct ForwardedExports {
    decltype(&FT_Init_FreeType_nid_postfix) init;
    decltype(&FT_Done_FreeType_nid_postfix) finish;
    decltype(&FT_Library_Version_nid_postfix) version;
    decltype(&FT_New_Memory_Face_nid_postfix) newMemoryFace;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&FT_Set_Pixel_Sizes_nid_postfix) pixelSizes;
    decltype(&FT_Load_Glyph_nid_postfix) load;
    decltype(&FT_Has_PS_Glyph_Names_nid_postfix) hasGlyphNames;
    decltype(&FT_Get_Glyph_nid_postfix) getGlyph;
    decltype(&FT_Glyph_Copy_nid_postfix) copyGlyph;
    decltype(&FT_Done_Glyph_nid_postfix) doneGlyph;
    decltype(&FT_Outline_New_nid_postfix) outlineNew;
    decltype(&FT_Outline_Done_nid_postfix) outlineDone;
    decltype(&FT_Outline_Render_nid_postfix) render;
    decltype(&FT_Property_Get_nid_postfix) propertyGet;
    decltype(&FT_Property_Set_nid_postfix) propertySet;
    decltype(&FT_QAlloc_nid_postfix) qalloc;
    decltype(&FT_Realloc_nid_postfix) realloc;
    decltype(&FT_QRealloc_nid_postfix) qrealloc;
    decltype(&FT_Get_Module_nid_postfix) getModule;
    decltype(&FT_Remove_Module_nid_postfix) removeModule;
    decltype(&FT_Open_Face_nid_postfix) open;
    decltype(&FT_GlyphSlot_Oblique_nid_postfix) oblique;
    decltype(&FT_GlyphSlot_Embolden_nid_postfix) embolden;
};

ForwardedExports forwarded;

struct ValidatorFixture {
    const std::uint8_t* base;
    const std::uint8_t* limit;
    std::int32_t level;
    std::int32_t error;
    std::uint64_t jumpBuffer[12];
};

static_assert(offsetof(ValidatorFixture, error) == 20 && offsetof(ValidatorFixture, jumpBuffer) == 24);

void JumpWithValidatorError(decltype(&ft_validator_error_nid_postfix) report, ValidatorFixture& validator,
                            std::int32_t error) {
    const int result = FreeTypeOtValidatorSetjmp(validator.jumpBuffer);
    if (result == 0) {
        report(&validator, error);
        Require(false, "validator error must jump to the saved context");
    }
    Require(result == 1, "validator error resumes with value 1");
    Require(validator.error == error, "validator error stores the supplied error");
}

void ValidatorErrors(decltype(&ft_validator_error_nid_postfix) report) {
    const std::array<std::uint8_t, 4> table{};
    const auto* const end = table.data() + table.size();
    constexpr std::uint64_t canary = 0xa5a5a5a5a5a5a5a5ull;
    struct {
        std::uint64_t before;
        ValidatorFixture validator;
        std::uint64_t after;
    } fixture{canary, {table.data(), end, 2, 0, {}}, canary};

    for (const std::int32_t error : {7, -23, 0}) {
        fixture.validator.error = 42;
        JumpWithValidatorError(report, fixture.validator, error);
        Require(fixture.validator.base == table.data() && fixture.validator.limit == end &&
                fixture.validator.level == 2, "validator error preserves the table bounds and level");
        Require(fixture.before == canary && fixture.after == canary, "validator error preserves surrounding storage");
    }
    for (auto* bad : {static_cast<void*>(nullptr), reinterpret_cast<void*>(1)}) {
        try {
            report(bad, 3);
        } catch (const std::exception&) {
            continue;
        }
        Require(false, "validator error rejects an inaccessible record");
    }
}

void* APS5_VABI Allocate(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    ++*static_cast<int*>(memory->user);
    void* block = std::malloc(static_cast<std::size_t>(size));
    if (block) std::memset(block, 0xA5, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI Release(FreeType::GuestMemoryRec*, void* block) { std::free(block); }

bool failReallocation = false;

void* APS5_VABI Resize(FreeType::GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    ++*static_cast<int*>(memory->user);
    if (failReallocation) return nullptr;
    auto* grown = static_cast<unsigned char*>(std::realloc(block, static_cast<std::size_t>(size)));
    if (grown && size > current) std::memset(grown + current, 0xA5, static_cast<std::size_t>(size - current));
    return grown;
}

bool Filled(const void* block, std::int64_t from, std::int64_t to, unsigned char value) {
    const auto* bytes = static_cast<const unsigned char*>(block);
    return std::all_of(bytes + from, bytes + to, [value](unsigned char byte) { return byte == value; });
}

void Reallocations() {
    int calls = 0;
    FreeType::GuestMemoryRec memory{&calls, Allocate, Release, Resize};
    for (const bool cleared : {true, false}) {
        const auto resize = cleared ? forwarded.realloc : forwarded.qrealloc;
        calls = 0;
        void* block = std::malloc(16);
        std::memset(block, 0x11, 16);
        Require(resize(&memory, 16, 48, &block) == 0 && block && calls == 1, "grow allocation");
        Require(Filled(block, 0, 16, 0x11) && Filled(block, 16, 48, cleared ? 0 : 0xA5), "grown region");
        Require(resize(&memory, 48, 8, &block) == 0 && block && Filled(block, 0, 8, 0x11) && calls == 2, "shrink allocation");
        failReallocation = true;
        void* kept = block;
        Require(resize(&memory, 8, 64, &block) == FT_Err_Out_Of_Memory && block == kept && calls == 3, "failed growth keeps the block");
        failReallocation = false;
        Require(resize(&memory, -1, 8, &block) == FT_Err_Invalid_Argument && block == kept, "negative current size");
        Require(resize(&memory, 8, -1, &block) == FT_Err_Invalid_Argument && block == kept, "negative new size");
        Require(resize(&memory, 8, 0x80000000ll, &block) == FT_Err_Array_Too_Large && block == kept && calls == 3, "oversized allocation");
        Require(resize(&memory, 8, 0, &block) == 0 && block == nullptr && calls == 3, "release through zero size");
        Require(resize(&memory, 0, 0, &block) == 0 && block == nullptr, "zero size without a block");
        Require(resize(&memory, 0, 24, &block) == 0 && block && Filled(block, 0, 24, 0) && calls == 4, "empty current allocates cleared");
        Release(&memory, block);
    }
}

void Libraries() {
    FreeType::GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    Require(forwarded.init(&library) == 0 && library && FT_Init_FreeType(&native) == 0, "initialize libraries");
    auto* renderer = forwarded.getModule(library, "smooth");
    auto* nativeRenderer = reinterpret_cast<FT_Renderer>(FT_Get_Module(native, "smooth"));
    Require(renderer != nullptr && nativeRenderer != nullptr, "find the smooth renderer");
    Require(ot.setRenderer(library, renderer, 0, nullptr) == FT_Set_Renderer(native, nativeRenderer, 0, nullptr),
            "select the smooth renderer");
    Require(FT_Get_Renderer(native, FT_GLYPH_FORMAT_OUTLINE) == nativeRenderer, "native renderer selected");
    Require(ot.setRenderer(library, nullptr, 0, nullptr) == FT_Set_Renderer(native, nullptr, 0, nullptr),
            "reject a missing renderer");
    for (FreeType::GuestLibrary* queried : {library, static_cast<FreeType::GuestLibrary*>(nullptr)}) {
        std::array<std::int32_t, 3> actual{-1, -1, -1};
        std::array<FT_Int, 3> expected{-1, -1, -1};
        forwarded.version(queried, &actual[0], &actual[1], &actual[2]);
        FT_Library_Version(queried ? native : nullptr, &expected[0], &expected[1], &expected[2]);
        Require(actual[0] == expected[0] && actual[1] == expected[1] && actual[2] == expected[2], "library version");
        std::int32_t minor = -1;
        forwarded.version(queried, nullptr, &minor, nullptr);
        Require(minor == expected[1], "library minor version alone");
    }

    GuestOutline outline{1, 2, nullptr, nullptr, nullptr, 3};
    FT_Outline nativeOutline{1, 2, nullptr, nullptr, nullptr, 3};
    Require(forwarded.outlineNew(library, 9, 2, &outline) == 0 && FT_Outline_New(native, 9, 2, &nativeOutline) == 0, "new outline");
    Require(outline.n_points == 9 && outline.n_contours == 2 && outline.flags == nativeOutline.flags, "new outline counts");
    Require(outline.points && outline.tags && outline.contours, "new outline storage");
    for (unsigned index = 0; index < 9; ++index) Require(outline.points[index].x == 0 && outline.points[index].y == 0 && outline.tags[index] == 0, "new outline is zeroed");
    Require(outline.contours[0] == 0 && outline.contours[1] == 0, "new outline contours are zeroed");
    Require(forwarded.outlineDone(library, &outline) == FT_Outline_Done(native, &nativeOutline), "done outline");
    Require(outline.n_points == 0 && outline.n_contours == 0 && outline.flags == 0 && !outline.points && !outline.tags && !outline.contours, "done outline is empty");
    Require(forwarded.outlineNew(library, 3, 5, &outline) == FT_Outline_New(native, 3, 5, &nativeOutline), "outline with more contours than points");
    Require(forwarded.outlineNew(nullptr, 3, 1, &outline) == FT_Outline_New(nullptr, 3, 1, &nativeOutline), "new outline without library");
    Require(forwarded.outlineNew(library, 3, 1, nullptr) == FT_Outline_New(native, 3, 1, nullptr), "new outline without target");
    Require(forwarded.outlineDone(nullptr, &outline) == FT_Outline_Done(nullptr, &nativeOutline), "done outline without library");
    Require(forwarded.outlineDone(library, nullptr) == FT_Outline_Done(native, nullptr), "done null outline");

    std::vector<GuestVector> square{{64, 64}, {576, 64}, {576, 576}, {64, 576}};
    std::vector<FT_Vector> nativeSquare{{64, 64}, {576, 64}, {576, 576}, {64, 576}};
    std::vector<unsigned char> tags{1, 1, 1, 1};
    std::vector<std::uint16_t> contours{3};
    GuestOutline shape{1, 4, square.data(), tags.data(), contours.data(), 0};
    FT_Outline nativeShape{1, 4, nativeSquare.data(), tags.data(), contours.data(), 0};
    for (const bool mono : {false, true}) {
        const unsigned char mode = mono ? FT_PIXEL_MODE_MONO : FT_PIXEL_MODE_GRAY;
        const std::int32_t pitch = mono ? 2 : 12;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(pitch) * 12, 0);
        std::vector<unsigned char> nativePixels = pixels;
        const FreeType::GuestBitmap bitmap{12, 12, pitch, pixels.data(), static_cast<std::uint16_t>(mono ? 2 : 256), mode, 0, nullptr};
        FT_Bitmap nativeBitmap{12, 12, pitch, nativePixels.data(), bitmap.num_grays, mode, 0, nullptr};
        FreeType::GuestRasterParams params{};
        params.target = &bitmap;
        params.flags = mono ? 0 : FT_RASTER_FLAG_AA;
        FT_Raster_Params nativeParams{};
        nativeParams.target = &nativeBitmap;
        nativeParams.flags = params.flags;
        Require(forwarded.render(library, &shape, &params) == FT_Outline_Render(native, &nativeShape, &nativeParams), "render result");
        Require(std::any_of(pixels.begin(), pixels.end(), [](unsigned char pixel) { return pixel != 0; }) && pixels == nativePixels, "rendered pixels");
        Require(params.source == &shape, "render source");
        std::fill(pixels.begin(), pixels.end(), 0);
        std::fill(nativePixels.begin(), nativePixels.end(), 0);
        Require(ot.outlineBitmap(library, &shape, &bitmap) == FT_Outline_Get_Bitmap(native, &nativeShape, &nativeBitmap),
                "outline bitmap result");
        Require(std::any_of(pixels.begin(), pixels.end(), [](unsigned char pixel) { return pixel != 0; }) &&
                pixels == nativePixels, "outline bitmap pixels");
    }
    Require(forwarded.render(nullptr, &shape, nullptr) == FT_Outline_Render(nullptr, &nativeShape, nullptr), "render without library");
    Require(forwarded.render(library, nullptr, nullptr) == FT_Outline_Render(native, nullptr, nullptr), "render without outline");

    std::uint32_t initial = 0;
    FT_UInt nativeInitial = 0;
    Require(forwarded.propertyGet(library, "truetype", "interpreter-version", &initial) == 0, "get interpreter version");
    Require(FT_Property_Get(native, "truetype", "interpreter-version", &nativeInitial) == 0 && initial == nativeInitial, "interpreter version matches");
    for (const std::uint32_t requested : {35u, 36u, initial}) {
        const FT_UInt nativeRequested = requested;
        std::uint32_t version = 0;
        FT_UInt nativeVersion = 0;
        Require(forwarded.propertySet(library, "truetype", "interpreter-version", &requested) ==
                FT_Property_Set(native, "truetype", "interpreter-version", &nativeRequested), "set interpreter version");
        Require(forwarded.propertyGet(library, "truetype", "interpreter-version", &version) == 0 &&
                FT_Property_Get(native, "truetype", "interpreter-version", &nativeVersion) == 0 && version == nativeVersion, "interpreter version after set");
        Require(requested == 36u || version == requested, "interpreter version value");
    }
    Require(forwarded.propertyGet(nullptr, "truetype", "interpreter-version", &initial) == FT_Property_Get(nullptr, "truetype", "interpreter-version", &nativeInitial), "get property without library");
    Require(forwarded.propertyGet(library, "truetype", "interpreter-version", nullptr) == FT_Property_Get(native, "truetype", "interpreter-version", nullptr), "get property without value");
    Require(forwarded.propertySet(library, "truetype", "missing", &initial) == FT_Property_Set(native, "truetype", "missing", &nativeInitial), "set missing property");

    int allocations = 0;
    FreeType::GuestMemoryRec memory{&allocations, Allocate, Release, nullptr};
    void* block = reinterpret_cast<void*>(1);
    Require(forwarded.qalloc(&memory, 48, &block) == 0 && block && allocations == 1, "quick allocation");
    Require(std::all_of(static_cast<unsigned char*>(block), static_cast<unsigned char*>(block) + 48, [](unsigned char byte) { return byte == 0xA5; }), "quick allocation is not cleared");
    Release(&memory, block);
    block = reinterpret_cast<void*>(1);
    Require(forwarded.qalloc(&memory, 0, &block) == 0 && block == nullptr, "empty quick allocation");
    block = reinterpret_cast<void*>(1);
    Require(forwarded.qalloc(&memory, -1, &block) == FT_Err_Invalid_Argument && block == nullptr && allocations == 1, "negative quick allocation");

    Require(forwarded.removeModule(library, nullptr) == FT_Remove_Module(native, nullptr), "remove null module");
    Require(forwarded.removeModule(nullptr, nullptr) == FT_Remove_Module(nullptr, nullptr), "remove module without library");
    void* module = forwarded.getModule(library, "truetype");
    FT_Module nativeModule = FT_Get_Module(native, "truetype");
    Require(module && nativeModule, "truetype module");
    Require(forwarded.removeModule(library, module) == FT_Remove_Module(native, nativeModule), "remove truetype module");
    Require(forwarded.getModule(library, "truetype") == nullptr && FT_Get_Module(native, "truetype") == nullptr, "truetype module removed");
    Require(forwarded.finish(library) == 0 && FT_Done_FreeType(native) == 0, "release libraries");
}

void Glyphs() {
    const std::vector<unsigned char> bytes = SquareGlyphFont('A', 700);
    FreeType::GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    Require(forwarded.init(&library) == 0 && library && FT_Init_FreeType(&native) == 0, "initialize glyph libraries");
    FreeType::GuestFaceRec* face = nullptr;
    FT_Face oracle = nullptr;
    Require(forwarded.newMemoryFace(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0 && face && face->glyph, "open memory face");
    Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0, "open oracle face");
    Require(forwarded.hasGlyphNames(face) == FT_Has_PS_Glyph_Names(oracle), "PostScript glyph names");
    Require(forwarded.hasGlyphNames(nullptr) == FT_Has_PS_Glyph_Names(nullptr), "PostScript glyph names without face");
    Require(forwarded.pixelSizes(face, 18, 18) == 0 && FT_Set_Pixel_Sizes(oracle, 18, 18) == 0, "pixel sizes");
    Require(forwarded.load(face, 1, FT_LOAD_NO_HINTING) == 0 && FT_Load_Glyph(oracle, 1, FT_LOAD_NO_HINTING) == 0, "load glyph");
    Require(oracle->glyph->outline.n_points > 0, "glyph has an outline");

    FreeType::GuestGlyphRec* glyph = nullptr;
    FT_Glyph nativeGlyph = nullptr;
    Require(forwarded.getGlyph(face->glyph, &glyph) == 0 && glyph && FT_Get_Glyph(oracle->glyph, &nativeGlyph) == 0, "get glyph");
    FreeType::GuestGlyphRec* copy = glyph;
    FT_Glyph nativeCopy = nativeGlyph;
    Require(forwarded.copyGlyph(nullptr, &copy) == FT_Glyph_Copy(nullptr, &nativeCopy) && copy == glyph, "copy null glyph");
    Require(forwarded.copyGlyph(glyph, nullptr) == FT_Glyph_Copy(nativeGlyph, nullptr), "copy glyph without target");
    Require(forwarded.copyGlyph(glyph, &copy) == 0 && copy && copy != glyph && FT_Glyph_Copy(nativeGlyph, &nativeCopy) == 0, "copy glyph");
    Require(copy->library == glyph->library && copy->clazz == glyph->clazz && copy->format == static_cast<std::uint32_t>(nativeCopy->format), "copied glyph header");
    Require(copy->advance.x == nativeCopy->advance.x && copy->advance.y == nativeCopy->advance.y, "copied advance");
    const GuestOutline& source = reinterpret_cast<FreeType::GuestOutlineGlyphRec*>(glyph)->outline;
    const GuestOutline& actual = reinterpret_cast<FreeType::GuestOutlineGlyphRec*>(copy)->outline;
    const FT_Outline& expected = reinterpret_cast<FT_OutlineGlyph>(nativeCopy)->outline;
    Require(actual.n_points == expected.n_points && actual.n_contours == expected.n_contours && actual.flags == expected.flags, "copied outline size");
    Require(actual.points != source.points && actual.tags != source.tags && actual.contours != source.contours, "copied outline storage");
    forwarded.doneGlyph(glyph);
    FT_Done_Glyph(nativeGlyph);
    for (unsigned index = 0; index < actual.n_points; ++index) {
        Require(actual.points[index].x == expected.points[index].x && actual.points[index].y == expected.points[index].y, "copied outline points");
        Require(actual.tags[index] == static_cast<unsigned char>(expected.tags[index]), "copied outline tags");
    }
    for (unsigned index = 0; index < actual.n_contours; ++index) Require(actual.contours[index] == expected.contours[index], "copied outline contours");
    forwarded.doneGlyph(copy);
    FT_Done_Glyph(nativeCopy);
    forwarded.doneGlyph(nullptr);

    Require(forwarded.doneFace(face) == 0 && FT_Done_Face(oracle) == 0, "release faces");
    Require(forwarded.finish(library) == 0 && FT_Done_FreeType(native) == 0, "release glyph libraries");
}

void Synthesis() {
    const std::vector<unsigned char> bytes = SquareGlyphFont('A', 700);
    FreeType::GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    Require(forwarded.init(&library) == 0 && library && FT_Init_FreeType(&native) == 0, "initialize synthesis libraries");
    FreeType::GuestOpenArgs args{};
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = bytes.data();
    args.memory_size = static_cast<std::int64_t>(bytes.size());
    FreeType::GuestFaceRec* face = nullptr;
    Require(forwarded.open(library, nullptr, 0, &face) == FT_Err_Invalid_Argument && face == nullptr, "open face without arguments");
    Require(forwarded.open(nullptr, &args, 0, &face) == FT_Err_Invalid_Library_Handle && face == nullptr, "open face without library");
    for (const bool embolden : {false, true}) {
        Require(forwarded.open(library, &args, 0, &face) == 0 && face && face->glyph, "open memory face through FT_Open_Face");
        FT_Face oracle = nullptr;
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0, "open synthesis oracle face");
        Require(face->num_glyphs == oracle->num_glyphs, "glyph count");
        Require(forwarded.pixelSizes(face, 18, 18) == 0 && FT_Set_Pixel_Sizes(oracle, 18, 18) == 0, "synthesis pixel sizes");
        Require(forwarded.load(face, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(oracle, 1, FT_LOAD_DEFAULT) == 0, "load synthesis glyph");
        Require(oracle->glyph->outline.n_points > 0, "synthesis glyph has an outline");
        std::vector<FT_Vector> before(oracle->glyph->outline.points, oracle->glyph->outline.points + oracle->glyph->outline.n_points);
        if (embolden) {
            forwarded.embolden(face->glyph);
            FT_GlyphSlot_Embolden(oracle->glyph);
        } else {
            forwarded.oblique(face->glyph);
            FT_GlyphSlot_Oblique(oracle->glyph);
        }
        const FreeType::GuestGlyphSlotRec& actual = *face->glyph;
        const FT_GlyphSlot expected = oracle->glyph;
        Require(actual.advance.x == expected->advance.x && actual.advance.y == expected->advance.y, "synthesized advance");
        Require(actual.metrics.width == expected->metrics.width && actual.metrics.height == expected->metrics.height, "synthesized metrics");
        Require(actual.outline.n_points == expected->outline.n_points && actual.outline.n_contours == expected->outline.n_contours, "synthesized outline size");
        for (int index = 0; index < expected->outline.n_points; ++index) {
            Require(actual.outline.points[index].x == expected->outline.points[index].x && actual.outline.points[index].y == expected->outline.points[index].y, "synthesized outline points");
        }
        bool changed = expected->outline.n_points != static_cast<int>(before.size());
        for (std::size_t index = 0; !changed && index < before.size(); ++index) {
            changed = before[index].x != expected->outline.points[index].x || before[index].y != expected->outline.points[index].y;
        }
        Require(changed, "synthesis changed the outline");
        Require(forwarded.doneFace(face) == 0 && FT_Done_Face(oracle) == 0, "release synthesis faces");
        face = nullptr;
    }
    forwarded.oblique(nullptr);
    forwarded.embolden(nullptr);
    Require(forwarded.finish(library) == 0 && FT_Done_FreeType(native) == 0, "release synthesis libraries");
}

void Variations() {
    const auto fvar = FontFixture::Words({1, 0, 16, 2, 1, 20, 0, 8, 0x7767, 0x6874, 0x0064, 0, 0x0190, 0, 0x0384, 0, 0, 256});
    const auto gvar = FontFixture::Words({1, 0, 1, 0, 0, 0, 3, 0, 0, 28, 0, 0, 0, 0});
    const auto bytes = FontFixture::BuildFont(3, {{"fvar", fvar}, {"gvar", gvar}});
    FreeType::GuestLibrary* library = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    Require(forwarded.init(&library) == 0, "initialize variation library");
    Require(forwarded.newMemoryFace(library, bytes.data(), bytes.size(), 0, &face) == 0, "open variable face");
    std::int64_t design = 650 * 65536;
    std::int64_t blend = 32768;
    Require(variations.varDesign(face, 1, &design) == 0 && (face->face_flags & FT_FACE_FLAG_VARIATION), "apply design coordinates");
    Require(variations.varBlend(face, 1, &blend) == 0, "apply blend coordinates");
    Require(variations.varDesign(face, 0, nullptr) == 0 && !(face->face_flags & FT_FACE_FLAG_VARIATION), "reset design coordinates");
    FT_Library native = nullptr;
    FT_Face oracle = nullptr;
    Require(FT_Init_FreeType(&native) == 0, "initialize native variation library");
    Require(FT_New_Memory_Face(native, bytes.data(), bytes.size(), 0, &oracle) == 0, "open native variable face");
    FT_Long nativeDesign = design;
    Require(variations.mmDesign(face, 1, &design) == FT_Set_MM_Design_Coordinates(oracle, 1, &nativeDesign), "MM design result");
    FT_Fixed nativeBlend = blend;
    Require(variations.mmBlend(face, 1, &blend) == FT_Set_MM_Blend_Coordinates(oracle, 1, &nativeBlend), "MM blend result");
    Require(FT_Done_Face(oracle) == 0 && FT_Done_FreeType(native) == 0, "release native variation face and library");
    Require(forwarded.doneFace(face) == 0 && forwarded.finish(library) == 0, "release variation face and library");
}

using Events = std::vector<std::array<std::int64_t, 7>>;

int Add(void* user, int kind, GuestVector first, GuestVector second = {}, GuestVector third = {}) {
    static_cast<Events*>(user)->push_back({kind, first.x, first.y, second.x, second.y, third.x, third.y});
    return 0;
}

GuestVector Guest(const FT_Vector* point) { return {point->x, point->y}; }

int NativeMove(const FT_Vector* to, void* user) { return Add(user, 1, Guest(to)); }
int NativeLine(const FT_Vector* to, void* user) { return Add(user, 2, Guest(to)); }
int NativeConic(const FT_Vector* control, const FT_Vector* to, void* user) { return Add(user, 3, Guest(control), Guest(to)); }
int NativeCubic(const FT_Vector* first, const FT_Vector* second, const FT_Vector* to, void* user) {
    return Add(user, 4, Guest(first), Guest(second), Guest(to));
}
int APS5_VABI GuestMove(const GuestVector* to, void* user) { return Add(user, 1, *to); }
int APS5_VABI GuestLine(const GuestVector* to, void* user) { return Add(user, 2, *to); }
int APS5_VABI GuestConic(const GuestVector* control, const GuestVector* to, void* user) { return Add(user, 3, *control, *to); }
int APS5_VABI GuestCubic(const GuestVector* first, const GuestVector* second, const GuestVector* to, void* user) {
    return Add(user, 4, *first, *second, *to);
}

void Trigonometry() {
    std::mt19937 random(0x0F7u);
    std::uniform_int_distribution<std::int32_t> angles(INT32_MIN, INT32_MAX);
    for (int sample = 0; sample < 4096; ++sample) {
        const std::int32_t angle = angles(random);
        Require(ot.sin(angle) == FT_Sin(angle), "FT_Sin");
        Require(static_cast<std::int32_t>(ot.tan(angle)) == FT_Tan(angle), "FT_Tan");
        FT_Vector expected{};
        GuestVector actual{};
        FT_Vector_Unit(&expected, angle);
        ot.vectorUnit(&actual, angle);
        Require(actual.x == expected.x && actual.y == expected.y, "FT_Vector_Unit");
    }
}

void MultiMaster(decltype(&FT_Get_Multi_Master_nid_postfix) query) {
    struct Axis {
        const char* name;
        std::int64_t minimum;
        std::int64_t maximum;
    };
    struct Master {
        std::uint32_t numAxis;
        std::uint32_t numDesigns;
        std::array<Axis, T1_MAX_MM_AXIS> axes;
    };
    static_assert(sizeof(Axis) == 24 && sizeof(Master) == 104);

    FreeType::GuestLibrary* guestLibrary = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(forwarded.init(&guestLibrary) == 0 && FT_Init_FreeType(&nativeLibrary) == 0, "init multi-master libraries");
    for (const auto& bytes : {Type1Fixture::MultipleMasterFont(), Type1Fixture::Font({})}) {
        FreeType::GuestFaceRec* guest = nullptr;
        FT_Face native = nullptr;
        Require(forwarded.newMemoryFace(guestLibrary, bytes.data(), bytes.size(), 0, &guest) == 0, "open guest Type 1 face");
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), bytes.size(), 0, &native) == 0, "open native Type 1 face");

        Master actual{0x1234, 0x5678, {}};
        actual.axes.fill({nullptr, -12345, 12345});
        const Master before = actual;
        FT_Multi_Master expected{};
        const int error = FT_Get_Multi_Master(native, &expected);
        Require(query(guest, &actual) == error, "multi-master result");
        if (error == 0) {
            Require(actual.numAxis == expected.num_axis && actual.numDesigns == expected.num_designs && actual.numAxis == 1,
                    "multi-master counts");
            const auto& axis = actual.axes[0];
            Require(axis.name && std::strcmp(axis.name, expected.axis[0].name) == 0, "multi-master axis name");
            Require(axis.minimum == expected.axis[0].minimum && axis.maximum == expected.axis[0].maximum, "multi-master axis bounds");
            for (std::size_t index = actual.numAxis; index < actual.axes.size(); ++index) {
                Require(std::memcmp(&actual.axes[index], &before.axes[index], sizeof(Axis)) == 0, "preserve inactive axes");
            }
        } else {
            Require(std::memcmp(&actual, &before, sizeof(Master)) == 0, "preserve multi-master output on failure");
        }
        Require(query(guest, nullptr) == FT_Get_Multi_Master(native, nullptr), "multi-master null output");
        Require(forwarded.doneFace(guest) == 0 && FT_Done_Face(native) == 0, "release Type 1 faces");
    }
    Master output{};
    Require(query(nullptr, &output) == FT_Err_Invalid_Face_Handle, "multi-master null face");
    Require(query(nullptr, nullptr) == FT_Get_Multi_Master(nullptr, nullptr), "multi-master null face and output");
    Require(forwarded.finish(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0, "release multi-master libraries");
}

void VariantSelectors(decltype(&FT_Face_GetCharsOfVariant_nid_postfix) getChars,
                      decltype(&FT_Face_GetVariantSelectors_nid_postfix) getSelectors,
                      decltype(&FT_Face_GetVariantsOfChar_nid_postfix) getVariants) {
    Require(getChars(nullptr, 0xFE0F) == nullptr, "null face chars of variant");
    Require(getSelectors(nullptr) == nullptr, "null face variant selectors");
    Require(getVariants(nullptr, 'A') == nullptr, "null face variants of char");
    FreeType::GuestLibrary* library = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    Require(forwarded.init(&library) == 0, "init variant selectors library");
    const auto font = SquareGlyphFont('A', 800);
    Require(forwarded.newMemoryFace(library, font.data(), font.size(), 0, &face) == 0, "open font for variant selectors");
    Require(getChars(face, 0xFE0F) == nullptr, "font chars of variant");
    Require(getSelectors(face) == nullptr, "font variant selectors");
    Require(getVariants(face, 'A') == nullptr, "font variants of char");
    Require(forwarded.doneFace(face) == 0 && forwarded.finish(library) == 0, "release variant selectors face and library");
}

void CharVariantIndex(decltype(&FT_Face_GetCharVariantIndex_nid_postfix) query) {
    auto cmap = FontFixture::Words({0, 2, 3, 1, 0, 20, 0, 5, 0, 52,
        4, 32, 0, 4, 4, 1, 0, 'A', 0xFFFF, 0, 'A', 0xFFFF, 1 - 'A', 1, 0, 0});
    constexpr std::array<unsigned char, 49> format14{
        0, 14, 0, 0, 0, 49, 0, 0, 0, 2,
        0, 0xFE, 0x0F, 0, 0, 0, 32, 0, 0, 0, 0,
        0x0E, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 40,
        0, 0, 0, 1, 0, 0, 'A', 0,
        0, 0, 0, 1, 0, 0, 'A', 0, 2
    };
    cmap.insert(cmap.end(), format14.begin(), format14.end());
    const auto bytes = FontFixture::BuildFont(3, {{"cmap", cmap}});
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FreeType::GuestFaceRec* guest = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(forwarded.init(&guestLibrary) == 0 && FT_Init_FreeType(&nativeLibrary) == 0, "init selector libraries");
    Require(forwarded.newMemoryFace(guestLibrary, bytes.data(), bytes.size(), 0, &guest) == 0, "open guest format 14 face");
    Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), bytes.size(), 0, &native) == 0, "open native format 14 face");
    Require(FT_Get_Char_Index(native, 'A') == 1, "base Unicode glyph");
    Require(query(nullptr, 'A', 0xFE0F) == 0, "missing selector face");
    const auto* selected = guest->charmap;
    struct Case {
        std::uint64_t character;
        std::uint64_t selector;
        std::uint32_t glyph;
    };
    constexpr Case cases[]{{'A', 0xFE0F, 1}, {'A', 0xE0100, 2}, {'B', 0xFE0F, 0}, {'A', 0xE0101, 0},
                           {0x100000041ull, 0x1000E0100ull, 2}};
    for (const auto& test : cases) {
        const auto expected = FT_Face_GetCharVariantIndex(native, static_cast<std::uint32_t>(test.character),
                                                         static_cast<std::uint32_t>(test.selector));
        Require(expected == test.glyph && query(guest, test.character, test.selector) == expected, "format 14 glyph index");
    }
    Require(guest->charmap == selected, "preserve selected Unicode charmap");
    Require(forwarded.doneFace(guest) == 0 && FT_Done_Face(native) == 0, "release selector faces");
    Require(forwarded.finish(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0, "release selector libraries");
}

void Outlines() {
    std::vector<GuestVector> points{{0, 0}, {640, 0}, {640, 640}, {0, 640}, {64, 64}, {320, 576}, {576, 64}, {320, 32}, {200, 100}};
    std::vector<unsigned char> tags{FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_CONIC, FT_CURVE_TAG_ON,
                                    FT_CURVE_TAG_ON, FT_CURVE_TAG_CUBIC, FT_CURVE_TAG_CUBIC, FT_CURVE_TAG_ON, FT_CURVE_TAG_CONIC};
    std::vector<std::uint16_t> contours{3, 8};
    std::vector<FT_Vector> nativePoints;
    for (const auto& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    GuestOutline outline{2, 9, points.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL};
    FT_Outline native{2, 9, nativePoints.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL};

    Require(ot.orientation(&outline) == FT_Outline_Get_Orientation(&native), "orientation");
    Require(ot.orientation(&outline) == FT_ORIENTATION_POSTSCRIPT, "counter-clockwise outline");
    Require(ot.orientation(nullptr) == FT_ORIENTATION_TRUETYPE, "null outline orientation");

    for (const auto [shift, delta] : {std::pair{0, std::int64_t{0}}, std::pair{2, std::int64_t{-7}}}) {
        Events actual;
        Events expected;
        const GuestOutlineFuncs functions{GuestMove, GuestLine, GuestConic, GuestCubic, shift, delta};
        const FT_Outline_Funcs nativeFunctions{NativeMove, NativeLine, NativeConic, NativeCubic, shift, static_cast<FT_Pos>(delta)};
        Require(ot.decompose(&outline, &functions, &actual) == FT_Outline_Decompose(&native, &nativeFunctions, &expected), "decompose result");
        Require(!actual.empty() && actual == expected, "decompose events");
    }
    Require(ot.decompose(nullptr, nullptr, nullptr) == FT_Err_Invalid_Outline, "decompose null outline");

    std::vector<GuestVector> copiedPoints(points.size());
    std::vector<unsigned char> copiedTags(tags.size());
    std::vector<std::uint16_t> copiedContours(contours.size());
    GuestOutline copy{2, 9, copiedPoints.data(), copiedTags.data(), copiedContours.data(), FT_OUTLINE_OWNER};
    Require(ot.copy(&outline, &copy) == FT_Err_Ok, "copy");
    for (std::size_t index = 0; index < points.size(); ++index) {
        Require(copiedPoints[index].x == points[index].x && copiedPoints[index].y == points[index].y && copiedTags[index] == tags[index], "copied points");
    }
    Require(copiedContours == contours && copy.flags == (FT_OUTLINE_EVEN_ODD_FILL | FT_OUTLINE_OWNER), "copied contours and flags");
    copy.n_points = 8;
    Require(ot.copy(&outline, &copy) == FT_Err_Invalid_Argument, "copy size mismatch");
    Require(ot.copy(nullptr, &copy) == FT_Err_Invalid_Outline, "copy null source");
}

void CharVariantIsDefault(decltype(&FT_Face_GetCharVariantIsDefault_nid_postfix) query) {
    auto cmap = FontFixture::Words({0, 2, 3, 1, 0, 20, 0, 5, 0, 52,
        4, 32, 0, 4, 4, 1, 0, 'A', 0xFFFF, 0, 'A', 0xFFFF, 1 - 'A', 1, 0, 0});
    constexpr std::array<unsigned char, 49> format14{
        0, 14, 0, 0, 0, 49, 0, 0, 0, 2,
        0, 0xFE, 0x0F, 0, 0, 0, 32, 0, 0, 0, 0,
        0x0E, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 40,
        0, 0, 0, 1, 0, 0, 'A', 0,
        0, 0, 0, 1, 0, 0, 'A', 0, 2
    };
    cmap.insert(cmap.end(), format14.begin(), format14.end());
    const auto bytes = FontFixture::BuildFont(3, {{"cmap", cmap}});
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FreeType::GuestFaceRec* guest = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(forwarded.init(&guestLibrary) == 0 && FT_Init_FreeType(&nativeLibrary) == 0, "init selector libraries");
    Require(forwarded.newMemoryFace(guestLibrary, bytes.data(), bytes.size(), 0, &guest) == 0, "open guest format 14 face");
    Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), bytes.size(), 0, &native) == 0, "open native format 14 face");
    Require(query(nullptr, 'A', 0xFE0F) == -1, "missing selector face");
    const auto* selected = guest->charmap;
    struct Case {
        std::uint64_t character;
        std::uint64_t selector;
        int status;
    };
    constexpr Case cases[]{{'A', 0xFE0F, 1}, {'A', 0xE0100, 0}, {'B', 0xFE0F, -1}, {'A', 0xE0101, -1},
                           {0x100000041ull, 0x1000E0100ull, 0}};
    for (const auto& test : cases) {
        const auto expected = FT_Face_GetCharVariantIsDefault(native, static_cast<std::uint32_t>(test.character),
                                                             static_cast<std::uint32_t>(test.selector));
        Require(expected == test.status && query(guest, test.character, test.selector) == expected, "format 14 default selector status");
    }
    Require(guest->charmap == selected, "preserve selected Unicode charmap");
    Require(forwarded.doneFace(guest) == 0 && FT_Done_Face(native) == 0, "release selector faces");
    Require(forwarded.finish(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0, "release selector libraries");
}

void Strokers() {
    FreeType::GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    Require(strokers.init(&library) == 0 && library && FT_Init_FreeType(&native) == 0, "initialize FreeType");
    std::vector<GuestVector> points{{0, 0}, {640, 0}, {640, 640}, {320, 960}, {0, 640}, {100, 100}, {300, 100}, {200, 300}};
    std::vector<FT_Vector> nativePoints;
    for (const GuestVector& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    std::vector<unsigned char> tags{1, 1, 1, 0, 1, 1, 1, 1};
    std::vector<std::uint16_t> contours{4, 7};
    for (const std::uint8_t opened : {std::uint8_t{0}, std::uint8_t{1}}) {
        void* guest = nullptr;
        FT_Stroker host = nullptr;
        Require(strokers.create(library, &guest) == 0 && FT_Stroker_New(native, &host) == 0, "create strokers");
        strokers.set(guest, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_MITER, 0x20000);
        FT_Stroker_Set(host, 48, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_MITER, 0x20000);
        GuestOutline outline{2, 8, points.data(), tags.data(), contours.data(), 0};
        FT_Outline nativeOutline{2, 8, nativePoints.data(), tags.data(), contours.data(), 0};
        Require(strokers.parse(guest, &outline, opened) == FT_Stroker_ParseOutline(host, &nativeOutline, opened), "parse outline");
        Require(strokers.parse(guest, nullptr, opened) == FT_Stroker_ParseOutline(host, nullptr, opened), "parse null outline");
        Require(strokers.parse(guest, &outline, opened) == FT_Stroker_ParseOutline(host, &nativeOutline, opened), "parse outline again");
        for (const std::int32_t border : {FT_STROKER_BORDER_LEFT, FT_STROKER_BORDER_RIGHT}) {
            std::uint32_t pointCount = 0, contourCount = 0;
            FT_UInt nativePointCount = 0, nativeContourCount = 0;
            Require(strokers.borderCounts(guest, border, &pointCount, &contourCount) ==
                    FT_Stroker_GetBorderCounts(host, static_cast<FT_StrokerBorder>(border), &nativePointCount, &nativeContourCount), "border counts");
            Require(pointCount == nativePointCount && contourCount == nativeContourCount, "border count values");
            Require(border == FT_STROKER_BORDER_RIGHT || pointCount > 0, "left border has points");
            std::vector<GuestVector> exportedPoints(pointCount);
            std::vector<unsigned char> exportedTags(pointCount);
            std::vector<std::uint16_t> exportedContours(contourCount);
            GuestOutline exported{0, 0, exportedPoints.data(), exportedTags.data(), exportedContours.data(), 0};
            std::vector<FT_Vector> expectedPoints(pointCount);
            std::vector<unsigned char> expectedTags(pointCount);
            std::vector<unsigned short> expectedContours(contourCount);
            FT_Outline expected{0, 0, expectedPoints.data(), expectedTags.data(), expectedContours.data(), 0};
            strokers.exportBorder(guest, border, &exported);
            FT_Stroker_ExportBorder(host, static_cast<FT_StrokerBorder>(border), &expected);
            Require(exported.n_points == expected.n_points && exported.n_contours == expected.n_contours && exported.n_points == pointCount, "exported counts");
            for (std::uint32_t index = 0; index < pointCount; ++index) {
                Require(exportedPoints[index].x == expectedPoints[index].x && exportedPoints[index].y == expectedPoints[index].y, "exported points");
                Require(exportedTags[index] == expectedTags[index], "exported tags");
            }
            for (std::uint32_t index = 0; index < contourCount; ++index) Require(exportedContours[index] == expectedContours[index], "exported contours");
            strokers.exportBorder(guest, border, nullptr);
        }
        strokers.done(guest);
        FT_Stroker_Done(host);
    }
    FT_Done_FreeType(native);
    Require(strokers.finish(library) == 0, "release guest library");
}

template<class Function>
void Resolve(Function& function, void* library, bool patched, const char* nid, const char* name) {
    const char* symbol = patched ? nid : name;
#ifdef _WIN32
    function = reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), symbol));
#else
    function = reinterpret_cast<Function>(dlsym(library, symbol));
#endif
    if (!function) std::fprintf(stderr, "libSceFreeTypeOt: missing %s\n", symbol);
    Require(function != nullptr, "missing export");
}

void ValidatorInitialization(decltype(&ft_validator_init_nid_postfix) initialize) {
    struct Validator {
        const std::uint8_t* base;
        const std::uint8_t* limit;
        std::int32_t level;
        std::int32_t error;
        std::array<std::uint8_t, 96> jumpBuffer;
    };
    const std::array<std::uint8_t, 8> table{1, 2, 3, 4, 5, 6, 7, 8};
    Validator validator{};
    validator.jumpBuffer.fill(0xA5);
    const auto originalJumpBuffer = validator.jumpBuffer;
    for (const std::int32_t level : {0, 1, 2, -1}) {
        validator.error = 0x08;
        initialize(&validator, table.data(), table.data() + table.size(), level);
        Require(validator.base == table.data() && validator.limit == table.data() + table.size(), "validator table bounds");
        Require(validator.level == level && validator.error == 0, "validator level and cleared error");
        Require(validator.jumpBuffer == originalJumpBuffer, "validator jump buffer left alone");
    }
    initialize(&validator, nullptr, nullptr, 0);
    Require(validator.base == nullptr && validator.limit == nullptr && validator.level == 0 && validator.error == 0,
            "validator stores null bounds");
    Require(validator.jumpBuffer == originalJumpBuffer, "reinitialization preserves jump buffer");
    try {
        initialize(nullptr, table.data(), table.data() + table.size(), 0);
    } catch (const std::exception&) {
        return;
    }
    Require(false, "validator rejects an inaccessible destination");
}

}

static void SfntTables(void* library, void* dependency, bool patched) {
    decltype(&FT_Get_Sfnt_Table_nid_postfix) query = nullptr;
#ifdef _WIN32
    Resolve(query, library, patched, "omDz0x8lyHk", "FT_Get_Sfnt_TableForward_nid_no_patch");
#else
    Resolve(query, library, patched, "omDz0x8lyHk", "FT_Get_Sfnt_Table_nid_postfix");
#endif
    Require(query(nullptr, FT_SFNT_HEAD) == nullptr, "SFNT query without a face");
    decltype(&FT_Init_FreeType_nid_postfix) initialize = nullptr;
    decltype(&FT_Done_FreeType_nid_postfix) finish = nullptr;
    decltype(&FT_New_Memory_Face_nid_postfix) open = nullptr;
    decltype(&FT_Reference_Face_nid_postfix) retain = nullptr;
    decltype(&FT_Done_Face_nid_postfix) release = nullptr;
    Resolve(initialize, dependency, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
    Resolve(finish, dependency, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
    Resolve(open, dependency, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
    Resolve(retain, dependency, patched, "HkSJXCHCQCU", "FT_Reference_Face_nid_postfix");
    Resolve(release, dependency, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");

    const auto firstFont = FontFixture::BuildFont(3, {});
    const auto secondFont = FontFixture::BuildFont(4, {});
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FreeType::GuestFaceRec* first = nullptr;
    FreeType::GuestFaceRec* second = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(initialize(&guestLibrary) == 0 && FT_Init_FreeType(&nativeLibrary) == 0, "init SFNT libraries");
    Require(open(guestLibrary, firstFont.data(), firstFont.size(), 0, &first) == 0, "open first guest SFNT face");
    Require(open(guestLibrary, secondFont.data(), secondFont.size(), 0, &second) == 0, "open second guest SFNT face");
    Require(FT_New_Memory_Face(nativeLibrary, firstFont.data(), firstFont.size(), 0, &native) == 0, "open native SFNT face");
    for (int tag = FT_SFNT_HEAD; tag <= FT_SFNT_MAX; ++tag) {
        const void* result = query(first, tag);
        const void* expected = FT_Get_Sfnt_Table(native, static_cast<FT_Sfnt_Tag>(tag));
        Require((result != nullptr) == (expected != nullptr), "SFNT table availability");
        Require(query(first, tag) == result, "stable SFNT pointer on repeated lookup");
    }
    const auto* firstMaxp = static_cast<const FreeType::GuestTtMaxProfile*>(query(first, FT_SFNT_MAXP));
    const auto* secondMaxp = static_cast<const FreeType::GuestTtMaxProfile*>(query(second, FT_SFNT_MAXP));
    const auto* expected = static_cast<const TT_MaxProfile*>(FT_Get_Sfnt_Table(native, FT_SFNT_MAXP));
    Require(firstMaxp && secondMaxp && expected && firstMaxp != secondMaxp, "distinct SFNT storage for each face");
    Require(firstMaxp->version == expected->version && firstMaxp->numGlyphs == expected->numGlyphs && secondMaxp->numGlyphs == 4,
            "SFNT maximum profiles");
    Require(FT_Reference_Face(native) == 0 && FT_Done_Face(native) == 0, "drop one native face reference");
    Require(FT_Get_Sfnt_Table(native, FT_SFNT_MAXP) == expected, "native SFNT pointer lives with retained face");
    Require(retain(first) == 0 && release(first) == 0, "drop one face reference");
    Require(firstMaxp->numGlyphs == expected->numGlyphs && query(first, FT_SFNT_MAXP) == firstMaxp, "SFNT pointer lives with retained face");
    Require(release(first) == 0, "release first SFNT face");
    Require(secondMaxp->numGlyphs == 4 && query(second, FT_SFNT_MAXP) == secondMaxp, "second SFNT face keeps its table");
    Require(release(second) == 0 && FT_Done_Face(native) == 0, "release remaining SFNT faces");
    Require(finish(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0, "release SFNT libraries");
}

int main(int argc, char** argv) {
    Require(argc == 3, "expected library and export mode");
    const bool patched = std::strcmp(argv[2], "patched") == 0;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    void* library = LoadLibraryA(argv[1]);
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(library != nullptr, "load requested library");
    decltype(&ft_validator_init_nid_postfix) initializeValidator = nullptr;
    Resolve(initializeValidator, library, patched, "2WLueaP8Qyc", "ft_validator_init_nid_postfix");
    ValidatorInitialization(initializeValidator);
    Resolve(ot.outlineBitmap, library, patched, "4RRjK-Wd1lU", "sceFreeTypeOtOutlineGetBitmap_nid_no_patch");
    Require(ot.outlineBitmap(nullptr, nullptr, nullptr) == FT_Outline_Get_Bitmap(nullptr, nullptr, nullptr),
            "outline bitmap without library");
#ifdef _WIN32
    const bool forwards = patched;
    void* dependency = GetModuleHandleA("libSceFreeType.prx");
#else
    const bool forwards = true;
    void* dependency = library;
#endif
    Require(dependency != nullptr, "locate libSceFreeType");
    void* faceProvider = forwards ? library : dependency;
    Resolve(forwarded.init, faceProvider, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
    Resolve(forwarded.finish, faceProvider, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
    Resolve(forwarded.newMemoryFace, faceProvider, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
    Resolve(forwarded.doneFace, faceProvider, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");
    Resolve(ot.setRenderer, library, patched, "Yu9oXRVmTTo", "sceFreeTypeOtSetRenderer_nid_no_patch");
    Require(ot.setRenderer(nullptr, nullptr, 0, nullptr) == FT_Set_Renderer(nullptr, nullptr, 0, nullptr),
            "reject a missing library");
#ifdef _WIN32
    void* guestLibc = GetModuleHandleA("libc.prx");
#else
    void* guestLibc = library;
#endif
    Require(guestLibc != nullptr, "locate the guest libc");
    Resolve(FreeTypeOtValidatorSetjmpTarget, guestLibc, patched, "gNQ1V2vfXDE", "setjmp_nid_postfix");
    decltype(&ft_validator_error_nid_postfix) validatorError = nullptr;
    Resolve(validatorError, library, patched, "CfkNVHnT78w", "ft_validator_error_nid_postfix");
    ValidatorErrors(validatorError);
    if (forwards) {
        Resolve(forwarded.version, library, patched, "5qqbXOGlHw4", "FT_Library_Version_nid_postfix");
        Resolve(forwarded.pixelSizes, library, patched, "lJxMIwj81SQ", "FT_Set_Pixel_Sizes_nid_postfix");
        Resolve(forwarded.load, library, patched, "M156RuYoUbU", "FT_Load_Glyph_nid_postfix");
        Resolve(forwarded.hasGlyphNames, library, patched, "x8y0qzLiYOU", "FT_Has_PS_Glyph_Names_nid_postfix");
        Resolve(forwarded.getGlyph, dependency, patched, "eMKHL+Cj50k", "FT_Get_Glyph_nid_postfix");
        Resolve(forwarded.copyGlyph, library, patched, "Dr3F3qrSQ2c", "FT_Glyph_Copy_nid_postfix");
        Resolve(forwarded.doneGlyph, dependency, patched, "RdrrejUMiJ0", "FT_Done_Glyph_nid_postfix");
        Resolve(forwarded.outlineNew, library, patched, "QH2emcgeFck", "FT_Outline_New_nid_postfix");
        Resolve(forwarded.outlineDone, library, patched, "BqGJsJcFYaM", "FT_Outline_Done_nid_postfix");
        Resolve(forwarded.render, library, patched, "9km02mPqPJE", "FT_Outline_Render_nid_postfix");
        Resolve(forwarded.propertyGet, library, patched, "8fAMdZCnewA", "FT_Property_Get_nid_postfix");
        Resolve(forwarded.propertySet, library, patched, "LsB538+IPrE", "FT_Property_Set_nid_postfix");
        Resolve(forwarded.qalloc, library, patched, "hz-J3hohXW8", "FT_QAlloc_nid_postfix");
        Resolve(forwarded.realloc, library, patched, "HVSaZbxYi2s", "FT_Realloc_nid_postfix");
        Resolve(forwarded.qrealloc, library, patched, "KVbmKCGGowg", "FT_QRealloc_nid_postfix");
        Resolve(forwarded.getModule, dependency, patched, "swrIPzOxcmM", "FT_Get_Module_nid_postfix");
        Resolve(forwarded.removeModule, library, patched, "5ipBcLYVc2k", "FT_Remove_Module_nid_postfix");
        Resolve(forwarded.open, library, patched, "OeNH41MA2Vs", "FT_Open_Face_nid_postfix");
        Resolve(forwarded.oblique, library, patched, "wBKAYO7yEXw", "FT_GlyphSlot_Oblique_nid_postfix");
        Resolve(forwarded.embolden, library, patched, "73vg+nRH68g", "FT_GlyphSlot_Embolden_nid_postfix");
        Libraries();
        Reallocations();
        Glyphs();
        Synthesis();
    }
    AttachMetrics(library, dependency, patched);
    Resolve(variations.mmBlend, library, patched, "zetC8Tp1S8w", "sceFreeTypeOtSetMMBlendCoordinates_nid_no_patch");
    Resolve(variations.mmDesign, library, patched, "DAmwG6U7YIc", "sceFreeTypeOtSetMMDesignCoordinates_nid_no_patch");
    Resolve(variations.varBlend, library, patched, "ihIHD8BTNxI", "sceFreeTypeOtSetVarBlendCoordinates_nid_no_patch");
    Resolve(variations.varDesign, library, patched, "yCEJIsjIOO8", "sceFreeTypeOtSetVarDesignCoordinates_nid_no_patch");
    for (auto setter : {variations.mmBlend, variations.mmDesign, variations.varBlend, variations.varDesign})
        Require(setter(nullptr, 0, nullptr) == FT_Err_Invalid_Face_Handle, "reject missing variation face");
    if (forwards) Variations();
    Resolve(ot.sin, library, patched, "L1uHkWTazkI", "FT_Sin_nid_postfix");
    Resolve(ot.tan, library, patched, "dojyaLKzKN0", "FT_Tan_nid_postfix");
    Resolve(ot.vectorUnit, library, patched, "3WpTQrvGUcU", "FT_Vector_Unit_nid_postfix");
    decltype(&FT_Face_GetCharVariantIndex_nid_postfix) charVariantIndex = nullptr;
#ifdef _WIN32
    Resolve(charVariantIndex, library, patched, "dPcZ76eZb64", "FT_Face_GetCharVariantIndexForward_nid_no_patch");
#else
    Resolve(charVariantIndex, library, patched, "dPcZ76eZb64", "FT_Face_GetCharVariantIndex_nid_postfix");
#endif
    CharVariantIndex(charVariantIndex);
    Resolve(ot.orientation, library, patched, "weVsBCT+TTQ", "FT_Outline_Get_Orientation_nid_postfix");
    Resolve(ot.copy, library, patched, "Bhm8Injl0KA", "FT_Outline_Copy_nid_postfix");
    decltype(&FT_Get_Multi_Master_nid_postfix) multiMaster = nullptr;
#ifdef _WIN32
    Resolve(multiMaster, library, patched, "pORpGtdlfqQ", "FT_Get_Multi_MasterForward_nid_no_patch");
#else
    Resolve(multiMaster, library, patched, "pORpGtdlfqQ", "FT_Get_Multi_Master_nid_postfix");
#endif
    MultiMaster(multiMaster);
    decltype(&FT_Face_GetCharsOfVariant_nid_postfix) getCharsOfVariant = nullptr;
    decltype(&FT_Face_GetVariantSelectors_nid_postfix) getVariantSelectors = nullptr;
    decltype(&FT_Face_GetVariantsOfChar_nid_postfix) getVariantsOfChar = nullptr;
#ifdef _WIN32
    Resolve(getCharsOfVariant, library, patched, "tl72PmqhXww", "FT_Face_GetCharsOfVariantForward_nid_no_patch");
    Resolve(getVariantSelectors, library, patched, "9Lm2mfsSmws", "FT_Face_GetVariantSelectorsForward_nid_no_patch");
    Resolve(getVariantsOfChar, library, patched, "mGMeRKWGTL4", "FT_Face_GetVariantsOfCharForward_nid_no_patch");
#else
    Resolve(getCharsOfVariant, library, patched, "tl72PmqhXww", "FT_Face_GetCharsOfVariant_nid_postfix");
    Resolve(getVariantSelectors, library, patched, "9Lm2mfsSmws", "FT_Face_GetVariantSelectors_nid_postfix");
    Resolve(getVariantsOfChar, library, patched, "mGMeRKWGTL4", "FT_Face_GetVariantsOfChar_nid_postfix");
#endif
    VariantSelectors(getCharsOfVariant, getVariantSelectors, getVariantsOfChar);
    Resolve(ot.decompose, library, patched, "++lgV9pRsO4", "FT_Outline_Decompose_nid_postfix");
    decltype(&FT_Face_GetCharVariantIsDefault_nid_postfix) charVariantIsDefault = nullptr;
#ifdef _WIN32
    Resolve(charVariantIsDefault, library, patched, "Vrab2WK1mgk", "FT_Face_GetCharVariantIsDefaultForward_nid_no_patch");
#else
    Resolve(charVariantIsDefault, library, patched, "Vrab2WK1mgk", "FT_Face_GetCharVariantIsDefault_nid_postfix");
#endif
    CharVariantIsDefault(charVariantIsDefault);
    if (forwards) {
        Resolve(strokers.init, library, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
        Resolve(strokers.finish, library, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
        Resolve(strokers.create, library, patched, "eqGCaeizvWM", "FT_Stroker_New_nid_postfix");
        Resolve(strokers.done, library, patched, "+RLa-6fbshA", "FT_Stroker_Done_nid_postfix");
        Resolve(strokers.set, library, patched, "B3uU5aDZn94", "FT_Stroker_Set_nid_postfix");
        Resolve(strokers.parse, library, patched, "RvFx7dv9ThU", "FT_Stroker_ParseOutline_nid_postfix");
        Resolve(strokers.borderCounts, library, patched, "LfyUi-LkdCk", "FT_Stroker_GetBorderCounts_nid_postfix");
        Resolve(strokers.exportBorder, library, patched, "7cIS2cFG3PE", "FT_Stroker_ExportBorder_nid_postfix");
    }
    SfntTables(library, dependency, patched);
    Trigonometry();
    Outlines();
    if (forwards) Strokers();
    std::puts("libSceFreeTypeOt: trigonometry, outline helpers and forwarded exports match FreeType");
    return 0;
}

namespace {

void AttachMetrics(void* library, void* dependency, bool patched) {
    decltype(&FT_Attach_File_nid_postfix) attach = nullptr;
#ifdef _WIN32
    Resolve(attach, library, patched, "21vkVuOP2lk", "FT_Attach_FileForward_nid_no_patch");
#else
    Resolve(attach, library, patched, "21vkVuOP2lk", "FT_Attach_File_nid_postfix");
#endif
    decltype(&FT_Init_FreeType_nid_postfix) initialize = nullptr;
    decltype(&FT_Done_FreeType_nid_postfix) finish = nullptr;
    decltype(&FT_New_Memory_Face_nid_postfix) open = nullptr;
    decltype(&FT_Done_Face_nid_postfix) release = nullptr;
    decltype(&FT_Get_Kerning_nid_postfix) kerning = nullptr;
    Resolve(initialize, dependency, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
    Resolve(finish, dependency, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
    Resolve(open, dependency, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
    Resolve(release, dependency, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");
    Resolve(kerning, dependency, patched, "LPtlB241JSg", "FT_Get_Kerning_nid_postfix");

    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-ot-attach-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root), "create attach fixture directory");
    const auto app0 = root / "app0";
    Require(std::filesystem::create_directory(app0), "create app0 for attach fixture");
    const auto path = app0 / "fixture.afm";
    {
        std::ofstream file(path, std::ios::binary);
        file << Type1Fixture::Afm();
        Require(static_cast<bool>(file), "write AFM fixture");
    }

    const auto bytes = Type1Fixture::Font();
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FreeType::GuestFaceRec* guestFace = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face nativeFace = nullptr;
    Require(initialize(&guestLibrary) == 0 && FT_Init_FreeType(&nativeLibrary) == 0, "init attach libraries");
    Require(open(guestLibrary, bytes.data(), bytes.size(), 0, &guestFace) == 0, "open guest Type 1 face");
    Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), bytes.size(), 0, &nativeFace) == 0, "open native Type 1 face");
    Require(!(guestFace->face_flags & FT_FACE_FLAG_KERNING) && !FT_HAS_KERNING(nativeFace), "Type 1 face starts without kerning");
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    const int error = attach(guestFace, "/app0/fixture.afm");
    std::filesystem::current_path(previous);
    Require(error == 0 && FT_Attach_File(nativeFace, path.string().c_str()) == 0, "attach AFM to guest and native faces");
    Require(guestFace->face_flags == nativeFace->face_flags && FT_HAS_KERNING(nativeFace), "publish attached kerning flag");
    Require(std::filesystem::remove(path), "remove attached metrics file");

    const auto glyph = FT_Get_Char_Index(nativeFace, 'A');
    GuestVector actual{};
    FT_Vector expected{};
    Require(glyph != 0 && FT_Get_Kerning(nativeFace, glyph, glyph, FT_KERNING_UNSCALED, &expected) == 0, "read native attached kerning");
    Require(kerning(guestFace, glyph, glyph, FT_KERNING_UNSCALED, &actual) == 0 && actual.x == expected.x && actual.y == expected.y &&
                expected.x == -50, "read guest attached kerning after file removal");
    Require(release(guestFace) == 0 && FT_Done_Face(nativeFace) == 0, "release attach faces");
    Require(finish(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0, "release attach libraries");
    std::filesystem::remove(app0);
    std::filesystem::remove(root);
}

}

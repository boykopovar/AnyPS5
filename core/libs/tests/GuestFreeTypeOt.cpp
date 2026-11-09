#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRIGONOMETRY_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_STROKER_H
#include FT_TYPE1_TABLES_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestOutline;
using FreeType::GuestOutlineFuncs;
using FreeType::GuestVector;

namespace {

struct Exports {
    decltype(&FT_Sin_nid_postfix) sin;
    decltype(&FT_Tan_nid_postfix) tan;
    decltype(&FT_Vector_Unit_nid_postfix) vectorUnit;
    decltype(&FT_Outline_Get_Orientation_nid_postfix) orientation;
    decltype(&FT_Outline_Copy_nid_postfix) copy;
    decltype(&FT_Outline_Decompose_nid_postfix) decompose;
};

Exports ot;

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
    decltype(&FT_Get_Module_nid_postfix) getModule;
    decltype(&FT_Remove_Module_nid_postfix) removeModule;
    decltype(&FT_Stroker_New_nid_postfix) strokerNew;
    decltype(&FT_Stroker_Set_nid_postfix) strokerSet;
    decltype(&FT_Stroker_BeginSubPath_nid_postfix) strokerBegin;
    decltype(&FT_Stroker_LineTo_nid_postfix) strokerLineTo;
    decltype(&FT_Stroker_EndSubPath_nid_postfix) strokerEnd;
    decltype(&FT_Stroker_GetCounts_nid_postfix) strokerCounts;
    decltype(&FT_Stroker_Export_nid_postfix) strokerExport;
    decltype(&FT_Stroker_Done_nid_postfix) strokerDone;
};

ForwardedExports forwarded;

void* APS5_VABI Allocate(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    ++*static_cast<int*>(memory->user);
    void* block = std::malloc(static_cast<std::size_t>(size));
    if (block) std::memset(block, 0xA5, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI Release(FreeType::GuestMemoryRec*, void* block) { std::free(block); }

void Libraries() {
    FreeType::GuestLibrary* library = nullptr;
    FT_Library native = nullptr;
    Require(forwarded.init(&library) == 0 && library && FT_Init_FreeType(&native) == 0, "initialize libraries");
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

    void* stroker = nullptr;
    FT_Stroker nativeStroker = nullptr;
    Require(forwarded.strokerNew(library, &stroker) == 0 && stroker && FT_Stroker_New(native, &nativeStroker) == 0, "new strokers");
    forwarded.strokerSet(stroker, 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Stroker_Set(nativeStroker, 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
    const std::array<GuestVector, 4> corners{{{0, 0}, {640, 0}, {640, 640}, {0, 640}}};
    FT_Vector origin{0, 0};
    Require(forwarded.strokerBegin(stroker, &corners[0], 0) == 0 && FT_Stroker_BeginSubPath(nativeStroker, &origin, 0) == 0, "begin stroke");
    for (std::size_t index = 1; index < corners.size(); ++index) {
        FT_Vector corner{static_cast<FT_Pos>(corners[index].x), static_cast<FT_Pos>(corners[index].y)};
        Require(forwarded.strokerLineTo(stroker, &corners[index]) == 0 && FT_Stroker_LineTo(nativeStroker, &corner) == 0, "stroke square");
    }
    Require(forwarded.strokerEnd(stroker) == 0 && FT_Stroker_EndSubPath(nativeStroker) == 0, "end stroke");
    std::uint32_t points = 0;
    std::uint32_t strokeContours = 0;
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    Require(forwarded.strokerCounts(stroker, &points, &strokeContours) == 0 && FT_Stroker_GetCounts(nativeStroker, &nativePoints, &nativeContours) == 0, "stroke counts");
    Require(points == nativePoints && strokeContours == nativeContours && points > 0 && strokeContours > 0, "stroke counts match");
    std::vector<GuestVector> strokePoints(points, GuestVector{-77, 99});
    std::vector<FT_Vector> nativeStrokePoints(points, FT_Vector{-77, 99});
    std::vector<unsigned char> strokeTags(points, 0xA5);
    std::vector<unsigned char> nativeStrokeTags = strokeTags;
    std::vector<std::uint16_t> strokeEnds(strokeContours, 0xBEEF);
    std::vector<std::uint16_t> nativeStrokeEnds = strokeEnds;
    GuestOutline stroke{0, 0, strokePoints.data(), strokeTags.data(), strokeEnds.data(), 0};
    FT_Outline nativeStroke{0, 0, nativeStrokePoints.data(), nativeStrokeTags.data(), nativeStrokeEnds.data(), 0};
    forwarded.strokerExport(nullptr, &stroke);
    forwarded.strokerExport(stroker, nullptr);
    Require(stroke.n_points == 0 && stroke.n_contours == 0 && strokeTags[0] == 0xA5, "export without stroker or outline");
    if constexpr (sizeof(FT_Pos) == sizeof(std::int64_t)) {
        forwarded.strokerExport(stroker, &stroke);
        FT_Stroker_Export(nativeStroker, &nativeStroke);
        Require(stroke.n_points == points && stroke.n_contours == strokeContours, "exported counts");
        Require(nativeStroke.n_points == points && nativeStroke.n_contours == strokeContours, "native exported counts");
        for (std::size_t index = 0; index < points; ++index) {
            Require(strokePoints[index].x == nativeStrokePoints[index].x && strokePoints[index].y == nativeStrokePoints[index].y, "exported points");
            Require(strokeTags[index] == nativeStrokeTags[index] && strokeTags[index] != 0xA5, "exported tags");
        }
        Require(strokeEnds == nativeStrokeEnds && strokeEnds.back() == points - 1, "exported contours");
    }
    forwarded.strokerDone(stroker);
    FT_Stroker_Done(nativeStroker);
    Require(forwarded.doneFace(face) == 0 && FT_Done_Face(oracle) == 0, "release faces");
    Require(forwarded.finish(library) == 0 && FT_Done_FreeType(native) == 0, "release glyph libraries");
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
#ifdef _WIN32
    const bool forwards = patched;
    void* dependency = GetModuleHandleA("libSceFreeType.prx");
#else
    const bool forwards = true;
    void* dependency = library;
#endif
    if (forwards) {
        Require(dependency != nullptr, "locate libSceFreeType");
        Resolve(forwarded.init, library, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
        Resolve(forwarded.finish, library, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
        Resolve(forwarded.version, library, patched, "5qqbXOGlHw4", "FT_Library_Version_nid_postfix");
        Resolve(forwarded.newMemoryFace, library, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
        Resolve(forwarded.doneFace, library, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");
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
        Resolve(forwarded.getModule, dependency, patched, "swrIPzOxcmM", "FT_Get_Module_nid_postfix");
        Resolve(forwarded.removeModule, library, patched, "5ipBcLYVc2k", "FT_Remove_Module_nid_postfix");
        Resolve(forwarded.strokerNew, library, patched, "eqGCaeizvWM", "FT_Stroker_New_nid_postfix");
        Resolve(forwarded.strokerSet, library, patched, "B3uU5aDZn94", "FT_Stroker_Set_nid_postfix");
        Resolve(forwarded.strokerBegin, library, patched, "xj3tNJptfLI", "FT_Stroker_BeginSubPath_nid_postfix");
        Resolve(forwarded.strokerLineTo, library, patched, "cKndiZnt0EA", "FT_Stroker_LineTo_nid_postfix");
        Resolve(forwarded.strokerEnd, library, patched, "pBqh-Uh-gns", "FT_Stroker_EndSubPath_nid_postfix");
        Resolve(forwarded.strokerCounts, library, patched, "rzIWh7FJnmU", "FT_Stroker_GetCounts_nid_postfix");
        Resolve(forwarded.strokerExport, library, patched, "2rcvKZfm5rI", "FT_Stroker_Export_nid_postfix");
        Resolve(forwarded.strokerDone, library, patched, "+RLa-6fbshA", "FT_Stroker_Done_nid_postfix");
        Libraries();
        Glyphs();
    }
    Resolve(ot.sin, library, patched, "L1uHkWTazkI", "FT_Sin_nid_postfix");
    Resolve(ot.tan, library, patched, "dojyaLKzKN0", "FT_Tan_nid_postfix");
    Resolve(ot.vectorUnit, library, patched, "3WpTQrvGUcU", "FT_Vector_Unit_nid_postfix");
    Resolve(ot.orientation, library, patched, "weVsBCT+TTQ", "FT_Outline_Get_Orientation_nid_postfix");
    Resolve(ot.copy, library, patched, "Bhm8Injl0KA", "FT_Outline_Copy_nid_postfix");
    Resolve(ot.decompose, library, patched, "++lgV9pRsO4", "FT_Outline_Decompose_nid_postfix");
    Trigonometry();
    Outlines();
    std::puts("libSceFreeTypeOt: trigonometry, outline helpers and forwarded exports match FreeType");
    return 0;
}

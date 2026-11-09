#include "prx/libharfbuzz/include/FontFunctions.hpp"

#include <hb.h>

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <source_location>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz font check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&hb_font_create_nid_postfix) create;
    decltype(&hb_font_destroy_nid_postfix) destroy;
    decltype(&hb_font_funcs_create_nid_postfix) createFunctions;
    decltype(&hb_font_funcs_make_immutable_nid_postfix) freezeFunctions;
    decltype(&hb_font_funcs_set_nominal_glyph_func_nid_postfix) nominal;
    decltype(&hb_font_funcs_set_variation_glyph_func_nid_postfix) variation;
    decltype(&hb_font_funcs_set_glyph_func_nid_postfix) glyph;
    decltype(&hb_font_funcs_set_glyph_h_advance_func_nid_postfix) advance;
    decltype(&hb_font_funcs_set_glyph_h_origin_func_nid_postfix) origin;
    decltype(&hb_font_funcs_set_glyph_extents_func_nid_postfix) extents;
    decltype(&hb_font_make_immutable_nid_postfix) freeze;
    decltype(&hb_font_set_funcs_nid_postfix) setFunctions;
    decltype(&hb_font_set_ppem_nid_postfix) ppem;
    decltype(&hb_font_set_scale_nid_postfix) scale;
};

struct FontState {
    hb_font_t* font;
    std::array<unsigned, 5> calls{};
    unsigned destroyed = 0;
};

struct CallbackState {
    unsigned identity;
    unsigned destroyed = 0;
};

static FontState& State(hb_font_t* font, void* fontData, void* callbackData, unsigned identity, unsigned index) {
    auto* state = static_cast<FontState*>(fontData);
    const auto* callback = static_cast<CallbackState*>(callbackData);
    Require(state && state->font == font && callback && callback->identity == identity);
    Require(state->destroyed == 0 && callback->destroyed == 0);
    ++state->calls[index];
    return *state;
}

static void APS5_VABI DestroyFont(void* opaque) {
    auto* state = static_cast<FontState*>(opaque);
    Require(state != nullptr && state->destroyed == 0);
    ++state->destroyed;
}

static void APS5_VABI DestroyCallback(void* opaque) {
    auto* state = static_cast<CallbackState*>(opaque);
    Require(state != nullptr && state->destroyed == 0);
    ++state->destroyed;
}

static int APS5_VABI Nominal(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t* glyph, void* callbackData) {
    State(font, fontData, callbackData, 1, 0);
    *glyph = unicode + 7;
    return unicode != 0x10FFFF;
}

static int APS5_VABI Variation(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t selector, std::uint32_t* glyph, void* callbackData) {
    State(font, fontData, callbackData, 2, 1);
    *glyph = unicode + selector;
    return 1;
}

static std::int32_t APS5_VABI Advance(hb_font_t* font, void* fontData, std::uint32_t glyph, void* callbackData) {
    State(font, fontData, callbackData, 3, 2);
    return 400 + static_cast<std::int32_t>(glyph);
}

static int APS5_VABI Origin(hb_font_t* font, void* fontData, std::uint32_t glyph, std::int32_t* x, std::int32_t* y, void* callbackData) {
    State(font, fontData, callbackData, 4, 3);
    *x = static_cast<std::int32_t>(glyph) * 2;
    *y = -3;
    return 1;
}

static int APS5_VABI Extents(hb_font_t* font, void* fontData, std::uint32_t glyph, hb_glyph_extents_t* extents, void* callbackData) {
    State(font, fontData, callbackData, 5, 4);
    *extents = {-static_cast<std::int32_t>(glyph), 700, 500, -700};
    return 1;
}

static int APS5_VABI LegacyGlyph(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t selector, std::uint32_t* glyph, void* callbackData) {
    State(font, fontData, callbackData, 6, selector ? 1 : 0);
    *glyph = unicode + selector + 100;
    return 1;
}

static void CheckProperties(const Api& api, hb_font_t* font) {
    hb_font_t* native = hb_font_create(nullptr);
    for (const auto [x, y] : std::array<std::pair<int, int>, 4>{{{0, 0}, {-123, 456}, {1000, -2000}, {std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max()}}}) {
        api.scale(font, x, y);
        hb_font_set_scale(native, x, y);
        int actualX, actualY, expectedX, expectedY;
        hb_font_get_scale(font, &actualX, &actualY);
        hb_font_get_scale(native, &expectedX, &expectedY);
        Require(actualX == expectedX && actualY == expectedY);
    }
    for (const auto [x, y] : std::array<std::pair<unsigned, unsigned>, 3>{{{0, 0}, {12, 34}, {0xFFFFFFFFu, 0x80000000u}}}) {
        api.ppem(font, x, y);
        hb_font_set_ppem(native, x, y);
        unsigned actualX, actualY, expectedX, expectedY;
        hb_font_get_ppem(font, &actualX, &actualY);
        hb_font_get_ppem(native, &expectedX, &expectedY);
        Require(actualX == expectedX && actualY == expectedY);
    }
    hb_font_destroy(native);
    api.scale(font, 1000, 1000);
    api.ppem(font, 0, 0);
}

static void CheckQueries(FontState& state) {
    auto* font = state.font;
    std::uint32_t glyph = 0;
    Require(hb_font_get_nominal_glyph(font, 'A', &glyph) && glyph == 72);
    Require(!hb_font_get_nominal_glyph(font, 0x10FFFF, &glyph));
    Require(hb_font_get_variation_glyph(font, 'A', 0xFE0F, &glyph) && glyph == 'A' + 0xFE0F);
    Require(hb_font_get_glyph_h_advance(font, 72) == 472);
    std::int32_t x = 0, y = 0;
    Require(hb_font_get_glyph_h_origin(font, 72, &x, &y) && x == 144 && y == -3);
    hb_glyph_extents_t extents{};
    Require(hb_font_get_glyph_extents(font, 72, &extents));
    Require(extents.x_bearing == -72 && extents.y_bearing == 700 && extents.width == 500 && extents.height == -700);
    for (auto calls : state.calls) Require(calls != 0);
}

static void CheckShape(FontState& state) {
    auto* buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, "AB", -1, 0, -1);
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
    const auto before = state.calls;
    Require(hb_shape_full(state.font, buffer, nullptr, 0, nullptr));
    unsigned length = 0;
    const auto* info = hb_buffer_get_glyph_infos(buffer, &length);
    Require(length == 2 && info[0].codepoint == 72 && info[1].codepoint == 73);
    Require(info[0].cluster == 0 && info[1].cluster == 1);
    const auto* positions = hb_buffer_get_glyph_positions(buffer, &length);
    Require(length == 2 && positions[0].x_advance == 472 && positions[1].x_advance == 473);
    Require(state.calls[0] > before[0] && state.calls[2] > before[2]);
    hb_buffer_destroy(buffer);
}

static hb_blob_t* EmptyTable(hb_face_t*, hb_tag_t, void*) {
    return hb_blob_get_empty();
}

static void CheckCallbacks(const Api& api) {
    auto* functions = api.createFunctions();
    Require(functions != nullptr && !hb_font_funcs_is_immutable(functions));
    std::array<CallbackState, 5> callbacks = {{{1}, {2}, {3}, {4}, {5}}};
    api.nominal(functions, Nominal, &callbacks[0], DestroyCallback);
    api.variation(functions, Variation, &callbacks[1], DestroyCallback);
    api.advance(functions, Advance, &callbacks[2], DestroyCallback);
    api.origin(functions, Origin, &callbacks[3], DestroyCallback);
    api.extents(functions, Extents, &callbacks[4], DestroyCallback);
    auto* face = hb_face_create_for_tables(EmptyTable, nullptr, nullptr);
    hb_face_set_glyph_count(face, 0x10000);
    FontState first{api.create(face)}, second{api.create(face)};
    hb_face_destroy(face);
    Require(first.font && second.font && first.font != second.font);
    CheckProperties(api, first.font);
    api.setFunctions(first.font, functions, &first, DestroyFont);
    api.setFunctions(second.font, functions, &second, DestroyFont);
    CheckQueries(first);
    CheckQueries(second);
    CheckShape(first);
    api.freezeFunctions(functions);
    Require(hb_font_funcs_is_immutable(functions));
    CallbackState rejected{1};
    api.nominal(functions, Nominal, &rejected, DestroyCallback);
    Require(rejected.destroyed == 1 && callbacks[0].destroyed == 0);
    api.freeze(first.font);
    Require(hb_font_is_immutable(first.font));
    api.scale(first.font, -77, 88);
    api.ppem(first.font, 12, 24);
    int x, y;
    unsigned px, py;
    hb_font_get_scale(first.font, &x, &y);
    hb_font_get_ppem(first.font, &px, &py);
    Require(x == 1000 && y == 1000 && px == 0 && py == 0);
    FontState rejectedFont{first.font};
    api.setFunctions(first.font, nullptr, &rejectedFont, DestroyFont);
    Require(rejectedFont.destroyed == 1 && first.destroyed == 0);
    hb_font_funcs_destroy(functions);
    api.destroy(first.font);
    Require(first.destroyed == 1 && second.destroyed == 0);
    for (const auto& callback : callbacks) Require(callback.destroyed == 0);
    CheckQueries(second);
    FontState replacement{second.font};
    api.setFunctions(second.font, nullptr, &replacement, DestroyFont);
    Require(second.destroyed == 1 && replacement.destroyed == 0);
    for (const auto& callback : callbacks) Require(callback.destroyed == 1);
    api.destroy(second.font);
    Require(replacement.destroyed == 1);
}

static void CheckLegacyAndReset(const Api& api) {
    auto* functions = api.createFunctions();
    FontState font{api.create(nullptr)};
    CallbackState legacy{6}, nominal{1}, ignored{1};
    api.glyph(functions, LegacyGlyph, &legacy, DestroyCallback);
    api.setFunctions(font.font, functions, &font, DestroyFont);
    std::uint32_t glyph = 0;
    Require(hb_font_get_nominal_glyph(font.font, 65, &glyph) && glyph == 165);
    Require(hb_font_get_variation_glyph(font.font, 65, 0xFE0F, &glyph) && glyph == 65 + 0xFE0F + 100);
    api.nominal(functions, Nominal, &nominal, DestroyCallback);
    Require(legacy.destroyed == 0);
    Require(hb_font_get_nominal_glyph(font.font, 65, &glyph) && glyph == 72);
    api.variation(functions, nullptr, &ignored, DestroyCallback);
    Require(legacy.destroyed == 1 && ignored.destroyed == 1 && nominal.destroyed == 0);
    Require(!hb_font_get_variation_glyph(font.font, 65, 0xFE0F, &glyph));
    CallbackState discarded{1};
    api.nominal(functions, nullptr, &discarded, DestroyCallback);
    Require(nominal.destroyed == 1 && discarded.destroyed == 1);
    Require(!hb_font_get_nominal_glyph(font.font, 65, &glyph));
    api.destroy(font.font);
    Require(font.destroyed == 1);
    hb_font_funcs_destroy(functions);
}

static void CheckNullLegacy(const Api& api) {
    for (const bool immutable : {false, true}) {
        auto* functions = api.createFunctions();
        FontState font{api.create(nullptr)};
        CallbackState original{6}, rejected{6};
        api.glyph(functions, LegacyGlyph, &original, DestroyCallback);
        api.setFunctions(font.font, functions, &font, DestroyFont);
        if (immutable) api.freezeFunctions(functions);
        try { api.glyph(functions, nullptr, &rejected, DestroyCallback); }
        catch (const std::exception&) {}
        Require(original.destroyed == 0 && rejected.destroyed == 0);
        std::uint32_t glyph = 0;
        Require(hb_font_get_nominal_glyph(font.font, 65, &glyph) && glyph == 165);
        Require(hb_font_get_variation_glyph(font.font, 65, 0xFE0F, &glyph) && glyph == 65 + 0xFE0F + 100);
        DestroyCallback(&rejected);
        api.destroy(font.font);
        hb_font_funcs_destroy(functions);
        Require(original.destroyed == 1 && rejected.destroyed == 1 && font.destroyed == 1);
    }
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
#define LoadExport(field, name, nid) api.field = Import<decltype(api.field)>(module, patched ? nid : #name "_nid_postfix")
    LoadExport(create, hb_font_create, "FVhYusaWqA4");
    LoadExport(destroy, hb_font_destroy, "-8t2aEtFws4");
    LoadExport(createFunctions, hb_font_funcs_create, "8ZddTpSAKIg");
    LoadExport(freezeFunctions, hb_font_funcs_make_immutable, "LyOypNLMoz0");
    LoadExport(nominal, hb_font_funcs_set_nominal_glyph_func, "88WaPmXp6x8");
    LoadExport(variation, hb_font_funcs_set_variation_glyph_func, "ik7nXsVUdMw");
    LoadExport(glyph, hb_font_funcs_set_glyph_func, "Mh4YoQ4Y57s");
    LoadExport(advance, hb_font_funcs_set_glyph_h_advance_func, "SfmbF51JL1Y");
    LoadExport(origin, hb_font_funcs_set_glyph_h_origin_func, "qv9cZX3xWQo");
    LoadExport(extents, hb_font_funcs_set_glyph_extents_func, "ReQKurupflQ");
    LoadExport(freeze, hb_font_make_immutable, "IThWGmihQUM");
    LoadExport(setFunctions, hb_font_set_funcs, "WH6phgWx6TI");
    LoadExport(ppem, hb_font_set_ppem, "ztxZdb1IY+M");
    LoadExport(scale, hb_font_set_scale, "eG5EI3x6twg");
#undef LoadExport
    CheckCallbacks(api);
    CheckLegacyAndReset(api);
    CheckNullLegacy(api);
    api.destroy(nullptr);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}

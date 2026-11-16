#include "prx/libharfbuzz/libharfbuzz.hpp"

#include <hb.h>

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <source_location>
#include <span>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz shaping check failed at line %u\n", location.line());
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
    decltype(&hb_shape_nid_postfix) shape;
    decltype(&hb_blob_create_nid_postfix) blob;
    decltype(&hb_face_create_for_tables_nid_postfix) face;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
    decltype(&hb_font_funcs_create_nid_postfix) functions;
    decltype(&hb_font_funcs_set_nominal_glyph_func_nid_postfix) nominal;
    decltype(&hb_font_funcs_set_glyph_h_advance_func_nid_postfix) advance;
    decltype(&hb_font_set_funcs_nid_postfix) setFunctions;
    decltype(&hb_buffer_create_nid_postfix) buffer;
    decltype(&hb_buffer_destroy_nid_postfix) destroyBuffer;
    decltype(&hb_buffer_add_utf16_nid_postfix) add;
    decltype(&hb_buffer_set_direction_nid_postfix) direction;
    decltype(&hb_buffer_set_script_nid_postfix) script;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) infos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) positions;
};

static int APS5_VABI Nominal(hb_font_t*, void*, std::uint32_t unicode, std::uint32_t* glyph, void*) {
    *glyph = unicode == 'A' ? 1 : 2;
    return 1;
}

static std::int32_t APS5_VABI Advance(hb_font_t*, void*, std::uint32_t glyph, void*) {
    return 100 + static_cast<std::int32_t>(glyph) * 10;
}

struct Tables {
    const Api* api;
    std::vector<char> gsub;
};

static hb_blob_t* APS5_VABI Table(hb_face_t*, std::uint32_t tag, void* opaque) {
    const auto& tables = *static_cast<Tables*>(opaque);
    if (tag == HB_TAG('m', 'a', 'x', 'p')) {
        const char maxp[] = {0, 0, 0x50, 0, 0, 4};
        return tables.api->blob(maxp, sizeof(maxp), 0, nullptr, nullptr);
    }
    if (tag != HB_TAG('G', 'S', 'U', 'B')) return tables.api->blob(nullptr, 0, 0, nullptr, nullptr);
    return tables.api->blob(tables.gsub.data(), static_cast<unsigned>(tables.gsub.size()), 0, nullptr, nullptr);
}

static void CheckShape(const Api& api, hb_font_t* font, std::uint32_t direction, std::span<const HarfBuzz::Feature> features, std::array<unsigned, 4> glyphs) {
    auto* buffer = api.buffer();
    const std::uint16_t text[] = {'A', 'B', 'A', 'B'};
    api.add(buffer, text, 4, 0, 4);
    api.direction(buffer, direction);
    api.script(buffer, HB_SCRIPT_LATIN);
    api.shape(font, buffer, features.data(), static_cast<unsigned>(features.size()));
    unsigned length = 0;
    const auto* infos = api.infos(buffer, &length);
    Require(length == 4);
    const auto* positions = api.positions(buffer, &length);
    Require(length == 4);
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned cluster = direction == HB_DIRECTION_RTL ? 3 - i : i;
        Require(infos[i].cluster == cluster && infos[i].codepoint == glyphs[cluster]);
        Require(positions[i].x_advance == 100 + static_cast<int>(glyphs[cluster]) * 10);
        Require(positions[i].y_advance == 0 && positions[i].x_offset == 0 && positions[i].y_offset == 0);
    }
    api.destroyBuffer(buffer);
}

static void Check(const Api& api) {
    Tables tables{&api, {}};
    for (unsigned word : {1, 0, 10, 30, 44, 1, 0x6C61, 0x746E, 8, 4, 0, 0, 0xFFFF, 1, 0,
                          1, 0x7361, 0x6C74, 8, 0, 1, 0,
                          1, 4, 1, 0, 1, 8, 2, 8, 1, 3, 1, 1, 1}) {
        tables.gsub.push_back(static_cast<char>(word >> 8));
        tables.gsub.push_back(static_cast<char>(word));
    }
    auto* face = api.face(Table, &tables, nullptr);
    auto* font = api.font(face);
    auto* functions = api.functions();
    api.nominal(functions, Nominal, nullptr, nullptr);
    api.advance(functions, Advance, nullptr, nullptr);
    api.setFunctions(font, functions, nullptr, nullptr);
    hb_font_funcs_destroy(functions);
    for (const auto direction : {HB_DIRECTION_LTR, HB_DIRECTION_RTL}) {
        CheckShape(api, font, direction, {}, {1, 2, 1, 2});
        const std::array<HarfBuzz::Feature, 1> global{{{HB_TAG('s', 'a', 'l', 't'), 1, 0, 0xFFFFFFFFu}}};
        const std::array<HarfBuzz::Feature, 1> range{{{HB_TAG('s', 'a', 'l', 't'), 1, 1, 3}}};
        CheckShape(api, font, direction, range, {1, 2, 3, 2});
        CheckShape(api, font, direction, global, {3, 2, 3, 2});
        const std::array<HarfBuzz::Feature, 2> overlap{{{global[0].tag, 1, 0, 4}, {global[0].tag, 0, 2, 3}}};
        CheckShape(api, font, direction, overlap, {3, 2, 1, 2});
        const std::array<HarfBuzz::Feature, 2> reverse{{overlap[1], overlap[0]}};
        CheckShape(api, font, direction, reverse, {3, 2, 3, 2});
    }
    auto* empty = api.buffer();
    api.shape(nullptr, empty, nullptr, 123);
    Require(hb_buffer_get_length(empty) == 0);
    api.destroyBuffer(empty);
    auto* buffer = api.buffer();
    const std::uint16_t text[] = {'A'};
    api.add(buffer, text, 1, 0, 1);
    unsigned throws = 0;
    try { api.shape(font, nullptr, nullptr, 0); } catch (const std::exception&) { ++throws; }
    try { api.shape(nullptr, buffer, nullptr, 0); } catch (const std::exception&) { ++throws; }
    try { api.shape(font, buffer, nullptr, 1); } catch (const std::exception&) { ++throws; }
    try { api.shape(font, buffer, reinterpret_cast<const HarfBuzz::Feature*>(0x10), 1); } catch (const std::exception&) { ++throws; }
    Require(throws == 4 && hb_buffer_get_length(buffer) == 1);
    api.destroyBuffer(buffer);
    api.destroyFont(font);
    api.destroyFace(face);
}

int main(int argc, char** argv) try {
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
    api.shape = Import<decltype(api.shape)>(module, patched ? "dM3hP4y99rM" : "hb_shape_nid_postfix");
    api.blob = Import<decltype(api.blob)>(module, patched ? "hj1KIIN5D54" : "hb_blob_create_nid_postfix");
    api.face = Import<decltype(api.face)>(module, patched ? "l7GEYs5PMVU" : "hb_face_create_for_tables_nid_postfix");
    api.destroyFace = Import<decltype(api.destroyFace)>(module, patched ? "ULNHQXciJ3s" : "hb_face_destroy_nid_postfix");
    api.font = Import<decltype(api.font)>(module, patched ? "FVhYusaWqA4" : "hb_font_create_nid_postfix");
    api.destroyFont = Import<decltype(api.destroyFont)>(module, patched ? "-8t2aEtFws4" : "hb_font_destroy_nid_postfix");
    api.functions = Import<decltype(api.functions)>(module, patched ? "8ZddTpSAKIg" : "hb_font_funcs_create_nid_postfix");
    api.nominal = Import<decltype(api.nominal)>(module, patched ? "88WaPmXp6x8" : "hb_font_funcs_set_nominal_glyph_func_nid_postfix");
    api.advance = Import<decltype(api.advance)>(module, patched ? "SfmbF51JL1Y" : "hb_font_funcs_set_glyph_h_advance_func_nid_postfix");
    api.setFunctions = Import<decltype(api.setFunctions)>(module, patched ? "WH6phgWx6TI" : "hb_font_set_funcs_nid_postfix");
    api.buffer = Import<decltype(api.buffer)>(module, patched ? "7f13XmbjTZo" : "hb_buffer_create_nid_postfix");
    api.destroyBuffer = Import<decltype(api.destroyBuffer)>(module, patched ? "K-y1p7Z9THQ" : "hb_buffer_destroy_nid_postfix");
    api.add = Import<decltype(api.add)>(module, patched ? "P+hgu937lFs" : "hb_buffer_add_utf16_nid_postfix");
    api.direction = Import<decltype(api.direction)>(module, patched ? "845S2XQdxhw" : "hb_buffer_set_direction_nid_postfix");
    api.script = Import<decltype(api.script)>(module, patched ? "eGbbvD8o7go" : "hb_buffer_set_script_nid_postfix");
    api.infos = Import<decltype(api.infos)>(module, patched ? "ENsHmJCNG4Q" : "hb_buffer_get_glyph_infos_nid_postfix");
    api.positions = Import<decltype(api.positions)>(module, patched ? "43mnfTQPCbw" : "hb_buffer_get_glyph_positions_nid_postfix");
    Check(api);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}

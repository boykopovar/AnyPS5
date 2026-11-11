#include "prx/libharfbuzz/include/Lifecycle.hpp"

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
        std::fprintf(stderr, "HarfBuzz lifecycle check failed at line %u\n", location.line());
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
    decltype(&hb_blob_create_nid_postfix) blob;
    decltype(&hb_face_create_for_tables_nid_postfix) face;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
};

struct Owner {
    unsigned destroyed = 0;
    const Api* api = nullptr;
    bool reenter = false;
};

static void APS5_VABI Destroy(void* opaque) {
    auto& owner = *static_cast<Owner*>(opaque);
    Require(owner.destroyed++ == 0);
    if (owner.reenter) {
        Owner nested;
        auto* face = owner.api->face(nullptr, &nested, Destroy);
        Require(face != nullptr && nested.destroyed == 1);
        owner.api->destroyFace(face);
    }
}

static void CheckData(hb_blob_t* blob, const char* expected, unsigned expectedLength) {
    unsigned length = 99;
    const auto* data = hb_blob_get_data(blob, &length);
    Require(length == expectedLength && hb_blob_get_length(blob) == expectedLength);
    Require(!length || std::memcmp(data, expected, length) == 0);
}

static void CheckBlobModes(const Api& api) {
    for (std::uint32_t mode = 0; mode != 4; ++mode) {
        std::array<char, 5> data{'a', '\0', 'c', '\xff', 'e'};
        const auto original = data;
        Owner owner{0, &api, true};
        auto* blob = api.blob(data.data(), data.size(), mode, &owner, Destroy);
        Require(blob != nullptr);
        CheckData(blob, original.data(), original.size());
        unsigned length = 0;
        const auto* borrowed = hb_blob_get_data(blob, &length);
        Require(length == data.size());
        if (mode == HB_MEMORY_MODE_DUPLICATE) {
            Require(owner.destroyed == 1 && borrowed != data.data());
        } else {
            Require(owner.destroyed == 0 && borrowed == data.data());
        }
        auto* retained = hb_blob_reference(blob);
        hb_blob_destroy(blob);
        Require(owner.destroyed == (mode == HB_MEMORY_MODE_DUPLICATE ? 1u : 0u));
        auto* writable = hb_blob_get_data_writable(retained, &length);
        Require(writable != nullptr && length == original.size());
        writable[2] = 'z';
        auto changed = original;
        changed[2] = 'z';
        CheckData(retained, changed.data(), changed.size());
        if (mode == HB_MEMORY_MODE_DUPLICATE || mode == HB_MEMORY_MODE_READONLY) Require(data == original);
        if (mode == HB_MEMORY_MODE_WRITABLE) Require(data[2] == 'z');
        hb_blob_make_immutable(retained);
        Require(hb_blob_get_data_writable(retained, &length) == nullptr && length == 0);
        hb_blob_destroy(retained);
        Require(owner.destroyed == 1);
    }
}

static void CheckBlobBoundaries(const Api& api) {
    char byte = 'x';
    for (const auto length : {0u, 1u << 31, std::numeric_limits<unsigned>::max()}) {
        for (std::uint32_t mode = 0; mode != 4; ++mode) {
            for (const auto* data : std::array<const char*, 2>{&byte, nullptr}) {
                Owner owner;
                auto* blob = api.blob(data, length, mode, &owner, Destroy);
                Require(blob != nullptr && hb_blob_get_length(blob) == 0 && owner.destroyed == 1);
                hb_blob_destroy(blob);
                Require(owner.destroyed == 1);
            }
        }
    }
    for (std::uint32_t mode = 0; mode != 4; ++mode) {
        auto* blob = api.blob(&byte, 1, mode, nullptr, nullptr);
        CheckData(blob, &byte, 1);
        hb_blob_destroy(blob);
    }
    Owner owner;
    for (const auto mode : {4u, std::numeric_limits<std::uint32_t>::max()}) {
        bool threw = false;
        try { api.blob(&byte, 1, mode, &owner, Destroy); }
        catch (const std::exception&) { threw = true; }
        Require(threw && owner.destroyed == 0);
    }
    bool threw = false;
    try { api.blob(nullptr, 1, HB_MEMORY_MODE_DUPLICATE, &owner, Destroy); }
    catch (const std::exception&) { threw = true; }
    Require(threw && owner.destroyed == 0);
}

struct FaceState {
    const Api* api;
    hb_face_t* face = nullptr;
    Owner faceOwner;
    Owner tableOwner;
    std::array<char, 4> bytes{'t', '\0', 'b', 'l'};
    unsigned requests = 0;
    unsigned reentries = 0;
};

static hb_blob_t* APS5_VABI Table(hb_face_t* face, std::uint32_t tag, void* opaque) {
    auto& state = *static_cast<FaceState*>(opaque);
    Require(face == state.face && state.faceOwner.destroyed == 0);
    ++state.requests;
    if (tag == HB_TAG('T', 'E', 'S', 'T')) {
        auto* missing = hb_face_reference_table(face, HB_TAG('N', 'O', 'N', 'E'));
        Require(hb_blob_get_length(missing) == 0);
        hb_blob_destroy(missing);
        ++state.reentries;
        return state.api->blob(state.bytes.data(), state.bytes.size(), HB_MEMORY_MODE_READONLY, &state.tableOwner, Destroy);
    }
    return nullptr;
}

static void APS5_VABI DestroyTableFace(void* opaque) {
    auto& state = *static_cast<FaceState*>(opaque);
    Destroy(&state.faceOwner);
}

static void CheckFaceLifetime(const Api& api) {
    FaceState state{&api};
    state.faceOwner.api = &api;
    state.faceOwner.reenter = true;
    state.face = api.face(Table, &state, DestroyTableFace);
    Require(state.face != nullptr && state.faceOwner.destroyed == 0 && state.requests == 0);
    auto* table = hb_face_reference_table(state.face, HB_TAG('T', 'E', 'S', 'T'));
    Require(state.requests == 2 && state.reentries == 1);
    CheckData(table, state.bytes.data(), state.bytes.size());
    Require(state.tableOwner.destroyed == 0);
    auto* retained = hb_face_reference(state.face);
    auto* font = api.font(state.face);
    Require(font != nullptr);
    api.destroyFace(state.face);
    api.destroyFace(retained);
    Require(state.faceOwner.destroyed == 0);
    api.destroyFont(font);
    Require(state.faceOwner.destroyed == 1 && state.tableOwner.destroyed == 0);
    CheckData(table, state.bytes.data(), state.bytes.size());
    hb_blob_destroy(table);
    Require(state.tableOwner.destroyed == 1);
}

static void CheckEmptyFaces(const Api& api) {
    Owner owner;
    auto* empty = api.face(nullptr, &owner, Destroy);
    Require(empty != nullptr && hb_face_get_glyph_count(empty) == 0 && owner.destroyed == 1);
    api.destroyFace(empty);
    Require(owner.destroyed == 1);
    empty = api.face(nullptr, nullptr, nullptr);
    Require(empty != nullptr && hb_face_get_glyph_count(empty) == 0);
    api.destroyFace(empty);
    FaceState state{&api};
    state.face = api.face(Table, &state, nullptr);
    auto* table = hb_face_reference_table(state.face, HB_TAG('N', 'O', 'N', 'E'));
    Require(hb_blob_get_length(table) == 0 && state.requests == 1);
    hb_blob_destroy(table);
    api.destroyFace(state.face);
    Require(state.faceOwner.destroyed == 0);
    api.destroyFace(nullptr);
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
    Api api{
        Import<decltype(Api::blob)>(module, patched ? "hj1KIIN5D54" : "hb_blob_create_nid_postfix"),
        Import<decltype(Api::face)>(module, patched ? "l7GEYs5PMVU" : "hb_face_create_for_tables_nid_postfix"),
        Import<decltype(Api::destroyFace)>(module, patched ? "ULNHQXciJ3s" : "hb_face_destroy_nid_postfix"),
        Import<decltype(Api::font)>(module, patched ? "FVhYusaWqA4" : "hb_font_create_nid_postfix"),
        Import<decltype(Api::destroyFont)>(module, patched ? "-8t2aEtFws4" : "hb_font_destroy_nid_postfix")};
    CheckBlobModes(api);
    CheckBlobBoundaries(api);
    CheckFaceLifetime(api);
    CheckEmptyFaces(api);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
    std::puts("HarfBuzz lifecycle checks passed");
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}

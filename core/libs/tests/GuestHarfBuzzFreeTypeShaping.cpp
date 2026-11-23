#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libharfbuzz/libharfbuzz.hpp"
#include "tests/CommandLineArgument.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <hb-ft.h>

#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <source_location>
#include <span>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz FreeType shaping check failed at line %u\n", location.line());
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
    decltype(&hb_ft_face_create_cached_nid_postfix) cachedFace;
    decltype(&hb_face_destroy_nid_postfix) faceDestroy;
    decltype(&hb_font_create_nid_postfix) fontCreate;
    decltype(&hb_font_destroy_nid_postfix) fontDestroy;
    decltype(&hb_font_set_scale_nid_postfix) fontScale;
    decltype(&hb_font_set_ppem_nid_postfix) fontPpem;
    decltype(&hb_font_funcs_create_nid_postfix) fontFuncsCreate;
    decltype(&hb_font_funcs_make_immutable_nid_postfix) fontFuncsMakeImmutable;
    decltype(&hb_font_funcs_set_nominal_glyph_func_nid_postfix) setNominalGlyph;
    decltype(&hb_font_funcs_set_glyph_h_advance_func_nid_postfix) setGlyphHAdvance;
    decltype(&hb_font_set_funcs_nid_postfix) fontSetFuncs;
    decltype(&hb_buffer_create_nid_postfix) bufferCreate;
    decltype(&hb_buffer_destroy_nid_postfix) bufferDestroy;
    decltype(&hb_buffer_add_utf16_nid_postfix) bufferAddUtf16;
    decltype(&hb_buffer_set_direction_nid_postfix) bufferSetDirection;
    decltype(&hb_buffer_set_script_nid_postfix) bufferSetScript;
    decltype(&hb_buffer_guess_segment_properties_nid_postfix) bufferGuessProperties;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) bufferGetInfos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) bufferGetPositions;
    decltype(&hb_buffer_reset_nid_postfix) bufferReset;
    decltype(&hb_shape_nid_postfix) shape;
};

struct FreeTypeApi {
    decltype(&FT_Init_FreeType_nid_postfix) initialize;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_New_Memory_Face_nid_postfix) newFace;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&FT_Get_Char_Index_nid_postfix) getCharIndex;
    decltype(&FT_Get_Advance_nid_postfix) getAdvance;
};

struct ShapedGlyph {
    std::uint32_t codepoint;
    std::uint32_t cluster;
    std::int32_t xAdvance;
    std::int32_t yAdvance;
    std::int32_t xOffset;
    std::int32_t yOffset;

    bool operator==(const ShapedGlyph&) const = default;
};

struct DestroyOwner {
    unsigned destroyed = 0;
};

struct FaceGenericState {
    FreeType::GuestFaceRec* face = nullptr;
    unsigned calls = 0;
};

struct CallbackTracker {
    unsigned id = 0;
    unsigned calls = 0;
    unsigned destroyed = 0;
};

struct CallbackFontState {
    const FreeTypeApi* ft = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    hb_font_t* font = nullptr;
    std::int32_t scaleMultiplier = 1;
    unsigned nominalCalls = 0;
    unsigned advanceCalls = 0;
    unsigned destroyed = 0;
};

static void APS5_VABI FinalizeGuestGeneric(void* object) {
    auto* face = static_cast<FreeType::GuestFaceRec*>(object);
    auto& state = *static_cast<FaceGenericState*>(face->generic.data);
    Require(face == state.face && state.calls++ == 0);
}

static void APS5_VABI DestroyCallbackTracker(void* opaque) {
    auto* tracker = static_cast<CallbackTracker*>(opaque);
    Require(tracker != nullptr && tracker->destroyed == 0);
    ++tracker->destroyed;
}

static void APS5_VABI DestroyFontState(void* opaque) {
    auto* state = static_cast<CallbackFontState*>(opaque);
    Require(state != nullptr && state->destroyed == 0);
    ++state->destroyed;
}

static int APS5_VABI NominalGlyphCallback(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t* glyph, void* callbackData) {
    auto* state = static_cast<CallbackFontState*>(fontData);
    auto* tracker = static_cast<CallbackTracker*>(callbackData);
    Require(state != nullptr && state->font == font && state->destroyed == 0);
    Require(tracker != nullptr && tracker->id == 1 && tracker->destroyed == 0);
    ++state->nominalCalls;
    ++tracker->calls;
    const auto gid = state->ft->getCharIndex(state->face, unicode);
    if (gid == 0) return 0;
    *glyph = gid;
    return 1;
}

static std::int32_t APS5_VABI GlyphHAdvanceCallback(hb_font_t* font, void* fontData, std::uint32_t glyph, void* callbackData) {
    auto* state = static_cast<CallbackFontState*>(fontData);
    auto* tracker = static_cast<CallbackTracker*>(callbackData);
    Require(state != nullptr && state->font == font && state->destroyed == 0);
    Require(tracker != nullptr && tracker->id == 2 && tracker->destroyed == 0);
    ++state->advanceCalls;
    ++tracker->calls;
    std::int64_t advance = 0;
    Require(state->ft->getAdvance(state->face, glyph, FT_LOAD_NO_SCALE, &advance) == 0);
    return static_cast<std::int32_t>(advance * state->scaleMultiplier);
}

static std::vector<unsigned char> ReadFontBytes() {
    const auto fontPath = std::filesystem::path(HARFBUZZ_TEST_FONTS) / "TestGPOSOne.ttf";
    std::ifstream file(fontPath, std::ios::binary);
    Require(file.good());
    std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(file), {});
    Require(!bytes.empty() && !file.bad());
    return bytes;
}

static std::vector<ShapedGlyph> ShapeGuest(
    const Api& api,
    hb_font_t* font,
    hb_buffer_t* buffer,
    std::span<const std::uint16_t> text,
    bool guessProperties,
    std::span<const HarfBuzz::Feature> features) {
    api.bufferReset(buffer);
    api.bufferAddUtf16(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    if (guessProperties) {
        api.bufferGuessProperties(buffer);
    } else {
        api.bufferSetDirection(buffer, HB_DIRECTION_LTR);
        api.bufferSetScript(buffer, HB_SCRIPT_LATIN);
    }
    api.shape(font, buffer, features.data(), static_cast<unsigned>(features.size()));
    unsigned infoLength = 0;
    unsigned posLength = 0;
    const auto* infos = api.bufferGetInfos(buffer, &infoLength);
    const auto* positions = api.bufferGetPositions(buffer, &posLength);
    Require(infoLength > 0 && infoLength == posLength && infos != nullptr && positions != nullptr);
    std::vector<ShapedGlyph> out;
    out.reserve(infoLength);
    for (unsigned i = 0; i < infoLength; ++i) {
        out.push_back({
            infos[i].codepoint,
            infos[i].cluster,
            positions[i].x_advance,
            positions[i].y_advance,
            positions[i].x_offset,
            positions[i].y_offset,
        });
    }
    return out;
}

static std::vector<ShapedGlyph> ShapeNative(
    hb_font_t* font,
    std::span<const std::uint16_t> text,
    bool guessProperties,
    std::span<const hb_feature_t> features) {
    auto* buffer = hb_buffer_create();
    Require(buffer != nullptr);
    hb_buffer_add_utf16(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    if (guessProperties) {
        hb_buffer_guess_segment_properties(buffer);
    } else {
        hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
    }
    hb_shape(font, buffer, features.data(), static_cast<unsigned>(features.size()));
    unsigned infoLength = 0;
    unsigned posLength = 0;
    const auto* infos = hb_buffer_get_glyph_infos(buffer, &infoLength);
    const auto* positions = hb_buffer_get_glyph_positions(buffer, &posLength);
    Require(infoLength > 0 && infoLength == posLength && infos != nullptr && positions != nullptr);
    std::vector<ShapedGlyph> out;
    out.reserve(infoLength);
    for (unsigned i = 0; i < infoLength; ++i) {
        out.push_back({
            infos[i].codepoint,
            infos[i].cluster,
            positions[i].x_advance,
            positions[i].y_advance,
            positions[i].x_offset,
            positions[i].y_offset,
        });
    }
    hb_buffer_destroy(buffer);
    return out;
}

static void CheckNonIdentityShaping(const FreeTypeApi& ft, FreeType::GuestFaceRec* guestFace, const std::vector<ShapedGlyph>& shaped) {
    Require(shaped.size() == 7);
    const std::array<std::uint32_t, 7> expectedClusters{0, 1, 1, 3, 5, 7, 8};
    for (std::size_t i = 0; i < expectedClusters.size(); ++i) {
        Require(shaped[i].cluster == expectedClusters[i]);
    }
    const auto glyphV = ft.getCharIndex(guestFace, 'V');
    const auto glyphF = ft.getCharIndex(guestFace, 'f');
    const auto glyphI = ft.getCharIndex(guestFace, 'i');
    const auto glyphL = ft.getCharIndex(guestFace, 'l');
    Require(glyphV != 0 && glyphF != 0 && glyphI != 0 && glyphL != 0);
    Require(shaped[0].codepoint == glyphV);
    Require(shaped[3].codepoint != glyphF && shaped[3].codepoint != glyphI);
    Require(shaped[4].codepoint != glyphF && shaped[4].codepoint != glyphL);
    std::int64_t unscaledVAdvance = 0;
    Require(ft.getAdvance(guestFace, glyphV, FT_LOAD_NO_SCALE, &unscaledVAdvance) == 0);
    Require(shaped[0].xAdvance != unscaledVAdvance);
    Require(shaped[5].xAdvance != unscaledVAdvance);
    Require(shaped[0].xAdvance != shaped[5].xAdvance);
    Require(shaped[2].xOffset != 0 || shaped[2].yOffset != 0);
}

static void CheckInvalidInputs(const Api& api, const FreeTypeApi& ft, const std::vector<unsigned char>& fontBytes) {
    auto rejectsRuntime = [](auto&& call) {
        errno = 71;
        bool threw = false;
        try {
            call();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && errno == 71);
    };

    FreeType::GuestLibrary* library = nullptr;
    Require(ft.initialize(&library) == 0);
    FreeType::GuestFaceRec* validFace = nullptr;
    Require(ft.newFace(library, fontBytes.data(), static_cast<std::int64_t>(fontBytes.size()), 0, &validFace) == 0);
    auto* cached = api.cachedFace(validFace);
    Require(cached != nullptr);
    auto* font = api.fontCreate(cached);
    Require(font != nullptr);

    FreeType::GuestFaceRec zeroed{};
    FreeType::GuestFaceRec forged{};
    forged.internal = reinterpret_cast<void*>(std::uintptr_t{0x10});
    FreeType::GuestFaceRec mismatchedOwner{};
    mismatchedOwner.internal = validFace->internal;

    rejectsRuntime([&] { api.cachedFace(nullptr); });
    rejectsRuntime([&] { api.cachedFace(&zeroed); });
    rejectsRuntime([&] { api.cachedFace(reinterpret_cast<FreeType::GuestFaceRec*>(std::uintptr_t{0x10})); });
    rejectsRuntime([&] { api.cachedFace(&forged); });
    rejectsRuntime([&] { api.cachedFace(&mismatchedOwner); });

    const std::array<std::uint16_t, 2> pair{'V', 'a'};
    auto* buffer = api.bufferCreate();
    const auto shaped = ShapeGuest(api, font, buffer, pair, false, {});
    Require(shaped.size() == 2 && shaped[0].xAdvance == 594);
    api.bufferDestroy(buffer);

    Require(api.cachedFace(validFace) == cached);
    api.faceDestroy(cached);
    api.faceDestroy(cached);
    api.fontDestroy(font);
    Require(ft.doneFace(validFace) == 0);
    Require(ft.doneLibrary(library) == 0);
}

static void CheckFreeTypeShaping(const Api& api, const FreeTypeApi& ft, const std::vector<unsigned char>& fontBytes) {
    FT_Library nativeLibrary = nullptr;
    FT_Face nativeFace = nullptr;
    Require(FT_Init_FreeType(&nativeLibrary) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, fontBytes.data(), static_cast<FT_Long>(fontBytes.size()), 0, &nativeFace) == 0);
    auto* nativeCached = hb_ft_face_create_cached(nativeFace);
    Require(nativeCached != nullptr);

    FreeType::GuestLibrary* guestLibrary = nullptr;
    Require(ft.initialize(&guestLibrary) == 0);
    FreeType::GuestFaceRec* guestFace = nullptr;
    Require(ft.newFace(guestLibrary, fontBytes.data(), static_cast<std::int64_t>(fontBytes.size()), 0, &guestFace) == 0);

    FaceGenericState priorGeneric{guestFace, 0};
    guestFace->generic = {&priorGeneric, FinalizeGuestGeneric};

    auto* cached = api.cachedFace(guestFace);
    Require(cached != nullptr && priorGeneric.calls == 1 && guestFace->generic.data == cached);
    auto* retained = api.cachedFace(guestFace);
    Require(retained == cached && priorGeneric.calls == 1);

    hb_user_data_key_t faceKey{};
    DestroyOwner faceUserDataOwner{};
    const auto destroyHostAbi = [](void* opaque) {
        auto* owner = static_cast<DestroyOwner*>(opaque);
        Require(owner != nullptr && owner->destroyed == 0);
        ++owner->destroyed;
    };
    Require(hb_face_set_user_data(cached, &faceKey, &faceUserDataOwner, destroyHostAbi, false));
    Require(hb_face_get_upem(cached) == hb_face_get_upem(nativeCached));
    Require(hb_face_get_glyph_count(cached) == hb_face_get_glyph_count(nativeCached));

    auto* otDefaultFont = api.fontCreate(cached);
    auto* otScaledFont = api.fontCreate(cached);
    auto* callbackFont = api.fontCreate(cached);
    auto* callbackScaledFont = api.fontCreate(cached);
    Require(otDefaultFont != nullptr && otScaledFont != nullptr && callbackFont != nullptr && callbackScaledFont != nullptr);

    api.fontScale(otScaledFont, 1600, 1600);
    api.fontPpem(otScaledFont, 24, 24);
    api.fontScale(callbackScaledFont, 2000, 2000);
    api.fontPpem(callbackScaledFont, 32, 32);

    auto* nativeDefaultFont = hb_font_create(nativeCached);
    auto* nativeScaledFont = hb_font_create(nativeCached);
    auto* nativeDoubleScaledFont = hb_font_create(nativeCached);
    Require(nativeDefaultFont != nullptr && nativeScaledFont != nullptr && nativeDoubleScaledFont != nullptr);
    hb_font_set_scale(nativeScaledFont, 1600, 1600);
    hb_font_set_ppem(nativeScaledFont, 24, 24);
    hb_font_set_scale(nativeDoubleScaledFont, 2000, 2000);
    hb_font_set_ppem(nativeDoubleScaledFont, 32, 32);

    CallbackTracker nominalTracker{1, 0, 0};
    CallbackTracker advanceTracker{2, 0, 0};
    CallbackFontState callbackFontState{&ft, guestFace, callbackFont, 1, 0, 0, 0};
    CallbackFontState callbackScaledFontState{&ft, guestFace, callbackScaledFont, 2, 0, 0, 0};

    auto* functions = api.fontFuncsCreate();
    Require(functions != nullptr);
    api.setNominalGlyph(functions, NominalGlyphCallback, &nominalTracker, DestroyCallbackTracker);
    api.setGlyphHAdvance(functions, GlyphHAdvanceCallback, &advanceTracker, DestroyCallbackTracker);
    api.fontFuncsMakeImmutable(functions);
    api.fontSetFuncs(callbackFont, functions, &callbackFontState, DestroyFontState);
    api.fontSetFuncs(callbackScaledFont, functions, &callbackScaledFontState, DestroyFontState);
    hb_font_funcs_destroy(functions);

    const std::array<std::uint16_t, 9> text{'V', 'a', 0x0304, 'f', 'i', 'f', 'l', 'V', '.'};
    auto* buffer = api.bufferCreate();
    Require(buffer != nullptr);

    const auto expectedDefault = ShapeNative(nativeDefaultFont, text, false, {});
    const auto expectedGuessed = ShapeNative(nativeDefaultFont, text, true, {});
    const auto expectedScaled = ShapeNative(nativeScaledFont, text, false, {});
    const auto expectedDoubleScaled = ShapeNative(nativeDoubleScaledFont, text, false, {});

    const std::array<HarfBuzz::Feature, 1> disableLigaGuest{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, 0xFFFFFFFFu}}};
    const std::array<hb_feature_t, 1> disableLigaNative{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, 0xFFFFFFFFu}}};
    const auto expectedNoLiga = ShapeNative(nativeDefaultFont, text, false, disableLigaNative);
    Require(expectedNoLiga.size() == text.size());

    const auto otDefaultResult = ShapeGuest(api, otDefaultFont, buffer, text, false, {});
    const auto otGuessedResult = ShapeGuest(api, otDefaultFont, buffer, text, true, {});
    const auto otScaledResult = ShapeGuest(api, otScaledFont, buffer, text, false, {});
    const auto otNoLigaResult = ShapeGuest(api, otDefaultFont, buffer, text, false, disableLigaGuest);

    Require(otDefaultResult == expectedDefault);
    Require(otGuessedResult == expectedGuessed);
    Require(otScaledResult == expectedScaled);
    Require(otNoLigaResult == expectedNoLiga);
    Require(otScaledResult[0].xAdvance != otDefaultResult[0].xAdvance);
    CheckNonIdentityShaping(ft, guestFace, otDefaultResult);

    const auto cbDefaultResult = ShapeGuest(api, callbackFont, buffer, text, false, {});
    const auto cbScaledResult = ShapeGuest(api, callbackScaledFont, buffer, text, false, {});
    Require(cbDefaultResult == expectedDefault);
    Require(cbScaledResult == expectedDoubleScaled);
    Require(callbackFontState.nominalCalls > 0 && callbackFontState.advanceCalls > 0);
    Require(callbackScaledFontState.nominalCalls > 0 && callbackScaledFontState.advanceCalls > 0);
    Require(nominalTracker.calls == callbackFontState.nominalCalls + callbackScaledFontState.nominalCalls);
    Require(advanceTracker.calls == callbackFontState.advanceCalls + callbackScaledFontState.advanceCalls);
    Require(callbackFontState.destroyed == 0 && callbackScaledFontState.destroyed == 0);
    Require(nominalTracker.destroyed == 0 && advanceTracker.destroyed == 0);

    api.faceDestroy(cached);
    api.faceDestroy(retained);
    Require(callbackFontState.destroyed == 0 && callbackScaledFontState.destroyed == 0);
    Require(nominalTracker.destroyed == 0 && advanceTracker.destroyed == 0);
    Require(faceUserDataOwner.destroyed == 0);

    const auto callsBefore = callbackFontState.nominalCalls;
    const auto advancesBefore = callbackFontState.advanceCalls;
    Require(ShapeGuest(api, otDefaultFont, buffer, text, false, {}) == expectedDefault);
    Require(ShapeGuest(api, otScaledFont, buffer, text, false, {}) == expectedScaled);
    Require(ShapeGuest(api, callbackFont, buffer, text, false, {}) == expectedDefault);
    Require(callbackFontState.nominalCalls > callsBefore && callbackFontState.advanceCalls > advancesBefore);

    auto* reacquired = api.cachedFace(guestFace);
    Require(reacquired == cached && priorGeneric.calls == 1 && faceUserDataOwner.destroyed == 0);
    api.faceDestroy(reacquired);
    Require(faceUserDataOwner.destroyed == 0 && callbackFontState.destroyed == 0);

    api.bufferDestroy(buffer);
    api.fontDestroy(otDefaultFont);
    api.fontDestroy(otScaledFont);
    api.fontDestroy(callbackFont);
    api.fontDestroy(callbackScaledFont);
    Require(callbackFontState.destroyed == 1 && callbackScaledFontState.destroyed == 1);
    Require(nominalTracker.destroyed == 1 && advanceTracker.destroyed == 1);
    Require(faceUserDataOwner.destroyed == 0);

    Require(ft.doneFace(guestFace) == 0);
    Require(priorGeneric.calls == 1 && faceUserDataOwner.destroyed == 1);
    Require(ft.doneLibrary(guestLibrary) == 0);

    hb_font_destroy(nativeDefaultFont);
    hb_font_destroy(nativeScaledFont);
    hb_font_destroy(nativeDoubleScaledFont);
    hb_face_destroy(nativeCached);
    Require(FT_Done_Face(nativeFace) == 0);
    Require(FT_Done_FreeType(nativeLibrary) == 0);
}

static auto OpenModule(const std::filesystem::path& path) {
#ifdef _WIN32
    auto module = LoadLibraryW(path.c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

int main(int argc, char** argv) try {
    Require(argc == 4);
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "unpatched") == 0);
    auto module = OpenModule(ArgumentPath(argv, 1));
    auto freeType = OpenModule(ArgumentPath(argv, 2));

    Api api{};
#define LoadHarfBuzz(field, name, nid) api.field = Import<decltype(api.field)>(module, patched ? nid : #name "_nid_postfix")
    LoadHarfBuzz(cachedFace, hb_ft_face_create_cached, "RTxFQhlbF+U");
    LoadHarfBuzz(faceDestroy, hb_face_destroy, "ULNHQXciJ3s");
    LoadHarfBuzz(fontCreate, hb_font_create, "FVhYusaWqA4");
    LoadHarfBuzz(fontDestroy, hb_font_destroy, "-8t2aEtFws4");
    LoadHarfBuzz(fontScale, hb_font_set_scale, "eG5EI3x6twg");
    LoadHarfBuzz(fontPpem, hb_font_set_ppem, "ztxZdb1IY+M");
    LoadHarfBuzz(fontFuncsCreate, hb_font_funcs_create, "8ZddTpSAKIg");
    LoadHarfBuzz(fontFuncsMakeImmutable, hb_font_funcs_make_immutable, "LyOypNLMoz0");
    LoadHarfBuzz(setNominalGlyph, hb_font_funcs_set_nominal_glyph_func, "88WaPmXp6x8");
    LoadHarfBuzz(setGlyphHAdvance, hb_font_funcs_set_glyph_h_advance_func, "SfmbF51JL1Y");
    LoadHarfBuzz(fontSetFuncs, hb_font_set_funcs, "WH6phgWx6TI");
    LoadHarfBuzz(bufferCreate, hb_buffer_create, "7f13XmbjTZo");
    LoadHarfBuzz(bufferDestroy, hb_buffer_destroy, "K-y1p7Z9THQ");
    LoadHarfBuzz(bufferAddUtf16, hb_buffer_add_utf16, "P+hgu937lFs");
    LoadHarfBuzz(bufferSetDirection, hb_buffer_set_direction, "845S2XQdxhw");
    LoadHarfBuzz(bufferSetScript, hb_buffer_set_script, "eGbbvD8o7go");
    LoadHarfBuzz(bufferGuessProperties, hb_buffer_guess_segment_properties, "DcrJr531A4w");
    LoadHarfBuzz(bufferGetInfos, hb_buffer_get_glyph_infos, "ENsHmJCNG4Q");
    LoadHarfBuzz(bufferGetPositions, hb_buffer_get_glyph_positions, "43mnfTQPCbw");
    LoadHarfBuzz(bufferReset, hb_buffer_reset, "PEggcTbDlfE");
    LoadHarfBuzz(shape, hb_shape, "dM3hP4y99rM");
#undef LoadHarfBuzz

    FreeTypeApi ft{};
#define LoadFreeType(field, name, nid) ft.field = Import<decltype(ft.field)>(freeType, patched ? nid : #name "_nid_postfix")
    LoadFreeType(initialize, FT_Init_FreeType, "GNsiLzm4SH8");
    LoadFreeType(doneLibrary, FT_Done_FreeType, "dH02CpEEcYk");
    LoadFreeType(newFace, FT_New_Memory_Face, "40Wa2neSLM0");
    LoadFreeType(doneFace, FT_Done_Face, "OgFczHko0w8");
    LoadFreeType(getCharIndex, FT_Get_Char_Index, "obnSzeb-KXk");
    LoadFreeType(getAdvance, FT_Get_Advance, "dMSDtQsYuJM");
#undef LoadFreeType

    const auto fontBytes = ReadFontBytes();
    CheckFreeTypeShaping(api, ft, fontBytes);
    CheckInvalidInputs(api, ft, fontBytes);

#ifdef _WIN32
    Require(FreeLibrary(freeType) != 0);
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(freeType) == 0);
    Require(dlclose(module) == 0);
#endif
    std::puts("HarfBuzz FreeType shaping checks passed");
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}

#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include "FontFixture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

enum class Scenario { Count, Index, Scale, OpenHandle, CloneHandle };
enum class Behavior { Success, ReturnedFailure, Throw };
struct PrimaryFailure {};
struct Block { void* pointer = nullptr; bool live = false; };
struct State {
    std::array<Block, 4096> blocks{};
    std::size_t records = 0;
    bool armed = false;
    Scenario scenario = Scenario::Count;
    Behavior behavior = Behavior::Success;
    Font::FontLibNative* library = nullptr;
    Font::FontHandleNative* font = nullptr;
    Font::FontCtxEntry* entry = nullptr;
    Font::FontObj* object = nullptr;
    std::uint32_t libraryWord = 0;
    std::uint32_t fontWord = 0;
    std::uint32_t entryWord = 0;
    std::uint32_t objectReferences = 0;
    bool allocationStage = false;
    bool callbackStage = false;
    std::size_t callbackCalls = 0;
};
State state;
const Font::SysDriver* actualDriver = nullptr;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "font lock unwind check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::size_t Live() {
    std::size_t found = 0;
    for (std::size_t i = 0; i < state.records; ++i) found += state.blocks[i].live;
    return found;
}

void* APS5_VABI Allocate(void*, std::uint32_t size) {
    if (state.armed && !state.allocationStage && (state.scenario == Scenario::OpenHandle || state.scenario == Scenario::CloneHandle)) {
        Require(size == sizeof(FontHandleOpaque));
        Require(state.entry->lock_mode1 == state.entryWord && state.object->refcount == state.objectReferences);
        if (state.scenario == Scenario::OpenHandle) {
            Require(state.library->lock_word == (state.libraryWord | Font::LOCK_BIT));
            Require(state.font->lock_word == state.fontWord);
        } else {
            Require(state.font->lock_word == (state.fontWord | Font::LOCK_BIT));
            Require(state.library->lock_word == state.libraryWord);
        }
        state.allocationStage = true;
        if (state.behavior == Behavior::Throw) throw PrimaryFailure{};
        if (state.behavior == Behavior::ReturnedFailure) return nullptr;
    }
    Require(state.records < state.blocks.size());
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    state.blocks[state.records++] = {pointer, true};
    return pointer;
}

void APS5_VABI Release(void*, void* pointer) {
    if (!pointer) return;
    for (std::size_t i = 0; i < state.records; ++i) {
        if (!state.blocks[i].live || state.blocks[i].pointer != pointer) continue;
        state.blocks[i].live = false;
        std::free(pointer);
        return;
    }
    Require(false);
}

bool BeforeQuery(Scenario expected, Font::FontObj* object) {
    if (!state.armed || state.scenario == Scenario::OpenHandle || state.scenario == Scenario::CloneHandle) return true;
    Require(state.scenario == expected && object == state.object && object->ft_face != nullptr);
    Require(state.entry->lock_mode1 == (state.entryWord | Font::LOCK_BIT));
    Require(state.library->lock_word == state.libraryWord && state.font->lock_word == state.fontWord);
    Require(object->refcount == state.objectReferences);
    state.callbackStage = true;
    ++state.callbackCalls;
    if (state.behavior == Behavior::Throw) throw PrimaryFailure{};
    return state.behavior != Behavior::ReturnedFailure;
}

int APS5_VABI Count(Font::FontObj* object, std::uint32_t* count) {
    if (!BeforeQuery(Scenario::Count, object)) return SCE_FONT_ERROR_FATAL;
    return actualDriver->glyphs_count(object, count);
}

int APS5_VABI Index(Font::FontObj* object, std::uint32_t code, std::uint32_t* index) {
    if (!BeforeQuery(Scenario::Index, object)) return SCE_FONT_ERROR_FATAL;
    return actualDriver->glyph_index(object, code, index);
}

int APS5_VABI Scale(Font::FontObj* object, std::uint32_t* resolution, float* scale) {
    if (!BeforeQuery(Scenario::Scale, object)) return SCE_FONT_ERROR_FATAL;
    return actualDriver->scale(object, resolution, scale);
}

int Query(FontHandle font, std::uint32_t& value, float& scale) {
    if (state.scenario == Scenario::Count) return sceFontGetFontGlyphsCount(font, &value);
    if (state.scenario == Scenario::Index) return sceFontGetCharGlyphCode(font, 'A', &value);
    return sceFontGetFontResolution(font, &value, &scale);
}

void Check(Scenario scenario, Behavior behavior) {
    Require(Live() == 0);
    state = {};
    state.scenario = scenario;
    state.behavior = behavior;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    Font::SysDriver driver = *actualDriver;
    driver.glyphs_count = Count;
    driver.glyph_index = Index;
    driver.scale = Scale;
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, &driver, &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 2, 0x52) == SCE_FONT_OK);
    const auto bytes = FontFixture::BitmapFont();
    const auto otherBytes = bytes;
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, bytes.data(), static_cast<std::uint32_t>(bytes.size()), nullptr, &font) == SCE_FONT_OK);
    state.library = static_cast<Font::FontLibNative*>(library);
    state.font = reinterpret_cast<Font::FontHandleNative*>(font);
    const auto* header = static_cast<const Font::FontCtxHeader*>(state.library->external_fonts_ctx);
    Require(header != nullptr && header->base != nullptr && state.font->open_info.ctx_entry_index < header->max_entries);
    state.entry = static_cast<Font::FontCtxEntry*>(header->base) + state.font->open_info.ctx_entry_index;
    state.object = static_cast<Font::FontObj*>(state.entry->obj_mode1);
    Require(state.object != nullptr && (state.font->flags & 0xF) == 1);
    state.libraryWord = state.library->lock_word;
    state.fontWord = state.font->lock_word;
    state.entryWord = state.entry->lock_mode1;
    state.objectReferences = state.object->refcount;
    Require((state.entryWord & Font::OPEN_BIT) != 0 && (state.entryWord & Font::COUNT_MASK) == 1 && state.objectReferences == 1);
    std::uint32_t expected = 0;
    float expectedScale = 0.0f;
    if (scenario != Scenario::OpenHandle && scenario != Scenario::CloneHandle) Require(Query(font, expected, expectedScale) == SCE_FONT_OK);
    const auto baseline = Live();
    state.armed = true;
    std::uint32_t value = 73;
    float scale = 73.0f;
    FontHandle opened = nullptr;
    FontOpenDetail detail{};
    detail.unique_id = 42;
    int result = 12345;
    bool primary = false;
    try {
        if (scenario == Scenario::OpenHandle) {
            result = sceFontOpenFontMemory(library, otherBytes.data(), static_cast<std::uint32_t>(otherBytes.size()), &detail, &opened);
        } else if (scenario == Scenario::CloneHandle) {
            result = sceFontOpenFontInstance(font, nullptr, &opened);
        } else {
            result = Query(font, value, scale);
        }
    } catch (const PrimaryFailure&) {
        primary = true;
    }
    state.armed = false;
    const bool acquisition = scenario == Scenario::OpenHandle || scenario == Scenario::CloneHandle;
    const bool expectedResult = behavior == Behavior::Throw ? primary && result == 12345 :
        !primary && result == (behavior == Behavior::ReturnedFailure ? (acquisition ? SCE_FONT_ERROR_ALLOCATION_FAILED : SCE_FONT_ERROR_FATAL) : SCE_FONT_OK);
    bool outputCorrect = acquisition ? (behavior == Behavior::Success ? opened != nullptr : opened == nullptr) :
        behavior != Behavior::Success || (value == expected && (scenario != Scenario::Scale || scale == expectedScale));
    if (opened) {
        Require(sceFontCloseFont(opened) == SCE_FONT_OK);
        opened = nullptr;
    }
    const bool locksReleased = state.library->lock_word == state.libraryWord && state.font->lock_word == state.fontWord && state.entry->lock_mode1 == state.entryWord;
    const bool ownershipPreserved = state.entry->obj_mode1 == state.object && state.object->refcount == state.objectReferences && Live() == baseline;
    const bool qualified = acquisition ? state.allocationStage && state.callbackCalls == 0 : state.callbackStage && state.callbackCalls == 1;
    std::fprintf(stderr, "font lock stage: scenario=%d behavior=%d allocation_stage=%d callback_stage=%d callbacks=%zu library_word=0x%x font_word=0x%x entry_word=0x%x expected_entry=0x%x retained=%td\n",
        static_cast<int>(scenario), static_cast<int>(behavior), state.allocationStage, state.callbackStage, state.callbackCalls,
        state.library->lock_word, state.font->lock_word, state.entry->lock_mode1, state.entryWord,
        static_cast<std::ptrdiff_t>(Live()) - static_cast<std::ptrdiff_t>(baseline));
    state.library->lock_word = state.libraryWord;
    state.font->lock_word = state.fontWord;
    state.entry->lock_mode1 = state.entryWord;
    if (scenario == Scenario::OpenHandle) {
        Require(sceFontOpenFontMemory(library, otherBytes.data(), static_cast<std::uint32_t>(otherBytes.size()), &detail, &opened) == SCE_FONT_OK);
        Require(sceFontCloseFont(opened) == SCE_FONT_OK);
    } else if (scenario == Scenario::CloneHandle) {
        Require(sceFontOpenFontInstance(font, nullptr, &opened) == SCE_FONT_OK);
        Require(sceFontCloseFont(opened) == SCE_FONT_OK);
    } else {
        value = 0;
        scale = 0.0f;
        Require(Query(font, value, scale) == SCE_FONT_OK && value == expected && scale == expectedScale);
    }
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK && Live() == 0);
    Require(expectedResult && outputCorrect && locksReleased && ownershipPreserved && qualified);
}

}

int main(int argc, char** argv) {
    actualDriver = static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    Require(actualDriver != nullptr);
    if (argc == 3) {
        const int scenario = std::atoi(argv[1]);
        const int behavior = std::atoi(argv[2]);
        Require(scenario >= 0 && scenario <= 4 && behavior >= 0 && behavior <= 2);
        Check(static_cast<Scenario>(scenario), static_cast<Behavior>(behavior));
        return 0;
    }
    Require(argc == 1);
    for (Scenario scenario : {Scenario::Count, Scenario::Index, Scenario::Scale, Scenario::OpenHandle, Scenario::CloneHandle}) {
        for (Behavior behavior : {Behavior::Success, Behavior::ReturnedFailure, Behavior::Throw}) Check(scenario, behavior);
    }
    std::puts("font lookup and handle acquisition lock stages passed");
}

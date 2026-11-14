#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

enum class Behavior { Success, ContextNull, ContextThrow, SupportError, SupportThrow, SupportAndCleanupThrow, ErrorCleanupThrow };
struct PrimaryFailure {};
struct SecondaryFailure {};
struct Block { void* pointer = nullptr; bool live = false; };
struct State {
    std::array<Block, 4096> blocks{};
    std::size_t records = 0;
    Behavior behavior = Behavior::Success;
    bool armed = false;
    bool system = false;
    bool allocationStage = false;
    bool supportStage = false;
    bool cleanupThrew = false;
    std::size_t supportCalls = 0;
    void* context = nullptr;
    Font::FontLibNative* library = nullptr;
};
State state;
constexpr std::uint32_t Seed = 0x20;
const Font::SysDriver* actualDriver = nullptr;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "font support unwind check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

void*& Slot() {
    return state.system ? state.library->sysfonts_ctx : state.library->external_fonts_ctx;
}

std::size_t Live() {
    std::size_t found = 0;
    for (std::size_t i = 0; i < state.records; ++i) found += state.blocks[i].live;
    return found;
}

bool ContextLive() {
    for (std::size_t i = 0; i < state.records; ++i) {
        if (state.blocks[i].live && state.blocks[i].pointer == state.context) return true;
    }
    return false;
}

void* APS5_VABI Allocate(void*, std::uint32_t size) {
    if (state.armed) {
        Require(state.library->lock_word == (Seed | Font::LOCK_BIT) && Slot() == nullptr);
        Require(size == (state.system ? 0x1020u : ((2u << 6) | 0x20u)));
        state.allocationStage = true;
        if (state.behavior == Behavior::ContextThrow) throw PrimaryFailure{};
        if (state.behavior == Behavior::ContextNull) return nullptr;
    }
    Require(state.records < state.blocks.size());
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    state.blocks[state.records++] = {pointer, true};
    if (state.armed) state.context = pointer;
    return pointer;
}

void APS5_VABI Release(void*, void* pointer) {
    if (!pointer) return;
    for (std::size_t i = 0; i < state.records; ++i) {
        if (!state.blocks[i].live || state.blocks[i].pointer != pointer) continue;
        state.blocks[i].live = false;
        std::free(pointer);
        if (state.armed && pointer == state.context && !state.cleanupThrew &&
            (state.behavior == Behavior::SupportAndCleanupThrow || state.behavior == Behavior::ErrorCleanupThrow)) {
            state.cleanupThrew = true;
            throw SecondaryFailure{};
        }
        return;
    }
    Require(false);
}

int APS5_VABI Support(Font::FontLibNative* library, std::uint32_t formats) {
    ++state.supportCalls;
    Require(library == state.library && library->lock_word == (Seed | Font::LOCK_BIT));
    Require(state.allocationStage && ContextLive() && Slot() == nullptr);
    state.supportStage = true;
    if (state.behavior == Behavior::SupportThrow || state.behavior == Behavior::SupportAndCleanupThrow) throw PrimaryFailure{};
    if (state.behavior == Behavior::SupportError || state.behavior == Behavior::ErrorCleanupThrow) return SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
    return actualDriver->support_formats(library, formats);
}

int SupportEntry(FontLibrary library) {
    return state.system ? sceFontSupportSystemFonts(library) : sceFontSupportExternalFonts(library, 2, 0x52);
}

void Check(bool system, Behavior behavior) {
    Require(Live() == 0);
    state = {};
    state.system = system;
    state.behavior = behavior;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, &state, nullptr, nullptr) == SCE_FONT_OK);
    Font::SysDriver driver = *actualDriver;
    driver.support_formats = Support;
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, &driver, &library) == SCE_FONT_OK);
    state.library = static_cast<Font::FontLibNative*>(library);
    state.library->lock_word = Seed;
    const auto baseline = Live();
    state.armed = true;
    int result = 12345;
    bool primary = false;
    bool secondary = false;
    try {
        result = SupportEntry(library);
    } catch (const PrimaryFailure&) {
        primary = true;
    } catch (const SecondaryFailure&) {
        secondary = true;
    }
    const bool primaryWanted = behavior == Behavior::ContextThrow || behavior == Behavior::SupportThrow || behavior == Behavior::SupportAndCleanupThrow;
    const bool secondaryWanted = behavior == Behavior::ErrorCleanupThrow;
    const bool expectedResult = primaryWanted ? primary && !secondary && result == 12345 :
        secondaryWanted ? secondary && !primary && result == 12345 :
        !primary && !secondary && result == (behavior == Behavior::ContextNull ? SCE_FONT_ERROR_ALLOCATION_FAILED :
                                            behavior == Behavior::SupportError ? SCE_FONT_ERROR_NO_SUPPORT_FUNCTION : SCE_FONT_OK);
    const bool lockReleased = state.library->lock_word == Seed;
    const bool published = Slot() != nullptr;
    const bool stateCorrect = behavior == Behavior::Success ? published && Slot() == state.context && Live() == baseline + 1 :
        !published && Live() == baseline;
    const bool callbackWanted = behavior != Behavior::ContextNull && behavior != Behavior::ContextThrow;
    const bool qualified = state.allocationStage && state.supportCalls == (callbackWanted ? 1u : 0u) && (!callbackWanted || state.supportStage);
    const bool cleanupQualified = (behavior != Behavior::SupportAndCleanupThrow && behavior != Behavior::ErrorCleanupThrow) || state.cleanupThrew;
    std::fprintf(stderr, "font support stage: system=%d behavior=%d allocation_stage=%d support_stage=%d support_calls=%zu lock_released=%d retained=%zu published=%d cleanup_threw=%d\n",
        system, static_cast<int>(behavior), state.allocationStage, state.supportStage, state.supportCalls, lockReleased,
        Live() - baseline, published, state.cleanupThrew);
    state.armed = false;
    state.library->lock_word = Seed;
    if (!published && ContextLive()) Release(nullptr, state.context);
    if (behavior != Behavior::Success) {
        state.behavior = Behavior::Success;
        state.supportCalls = 0;
        state.allocationStage = false;
        state.supportStage = false;
        state.armed = true;
        Require(SupportEntry(library) == SCE_FONT_OK);
        state.armed = false;
    }
    Require(state.library->lock_word == Seed);
    Require(SupportEntry(library) == SCE_FONT_ERROR_ALREADY_SPECIFIED);
    Require(state.library->lock_word == Seed);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr && Live() == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(expectedResult && lockReleased && stateCorrect && qualified && cleanupQualified);
}

}

int main(int argc, char** argv) {
    actualDriver = static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    Require(actualDriver != nullptr);
    if (argc == 3) {
        const int system = std::atoi(argv[1]);
        const int behavior = std::atoi(argv[2]);
        Require(system >= 0 && system <= 1 && behavior >= 0 && behavior <= 6);
        Check(system != 0, static_cast<Behavior>(behavior));
        return 0;
    }
    Require(argc == 1);
    for (const bool system : {false, true}) {
        for (Behavior behavior : {Behavior::Success, Behavior::ContextNull, Behavior::ContextThrow, Behavior::SupportError,
                                  Behavior::SupportThrow, Behavior::SupportAndCleanupThrow, Behavior::ErrorCleanupThrow}) Check(system, behavior);
    }
    std::puts("font support unwind stages passed");
}

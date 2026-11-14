#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <ft2build.h>
#include FT_SYSTEM_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

enum class Behavior { Success, ContextNull, ContextThrow, MemoryNull, MemoryThrow, MemoryAndCleanupThrow };
struct PrimaryFailure {};
struct SecondaryFailure {};
struct ContextProbe {
    void* allocCtx;
    const FontMemoryInterface* iface;
    FT_Memory memory;
    void* library;
    FT_Memory hostMemory;
    void* hostLibrary;
};
struct Block { void* pointer = nullptr; bool live = false; };
struct State {
    Behavior behavior = Behavior::Success;
    std::array<Block, 4096> blocks{};
    std::size_t records = 0;
    std::size_t calls = 0;
    unsigned contextFrees = 0;
    bool contextStage = false;
    bool memoryStage = false;
    bool cleanupThrew = false;
    const FontMemoryInterface* iface = nullptr;
};
State state;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "font Ft init unwind check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::size_t Live() {
    std::size_t count = 0;
    for (std::size_t i = 0; i < state.records; ++i) count += state.blocks[i].live;
    return count;
}

void* APS5_VABI Allocate(void* context, std::uint32_t size) {
    Require(context == &state);
    ++state.calls;
    if (state.calls == 1) {
        Require(size == sizeof(ContextProbe) && Live() == 0);
        state.contextStage = true;
        if (state.behavior == Behavior::ContextNull) return nullptr;
        if (state.behavior == Behavior::ContextThrow) throw PrimaryFailure{};
    } else if (state.calls == 2) {
        Require(size == sizeof(FT_MemoryRec_) && state.records == 1 && Live() == 1);
        ContextProbe probe{};
        std::memcpy(&probe, state.blocks[0].pointer, sizeof(probe));
        Require(probe.allocCtx == &state && probe.iface == state.iface && !probe.memory && !probe.library && !probe.hostMemory && !probe.hostLibrary);
        state.memoryStage = true;
        if (state.behavior == Behavior::MemoryNull) return nullptr;
        if (state.behavior == Behavior::MemoryThrow || state.behavior == Behavior::MemoryAndCleanupThrow) throw PrimaryFailure{};
    }
    Require(state.records < state.blocks.size());
    void* pointer = std::malloc(size == 0 ? 1 : size);
    Require(pointer != nullptr);
    state.blocks[state.records++] = {pointer, true};
    return pointer;
}

void APS5_VABI Release(void* context, void* pointer) {
    Require(context == &state);
    if (!pointer) return;
    for (std::size_t i = 0; i < state.records; ++i) {
        Block& block = state.blocks[i];
        if (!block.live || block.pointer != pointer) continue;
        block.live = false;
        std::free(pointer);
        if (i == 0) {
            ++state.contextFrees;
            if (state.behavior == Behavior::MemoryAndCleanupThrow) {
                Require(!state.cleanupThrew);
                state.cleanupThrew = true;
                throw SecondaryFailure{};
            }
        }
        return;
    }
    Require(false);
}

void Check(Behavior behavior) {
    Require(Live() == 0);
    state = {};
    state.behavior = behavior;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    state.iface = &iface;
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, &state, nullptr, nullptr) == SCE_FONT_OK);
    const auto* driver = static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    Require(driver && driver->init && driver->term);
    Font::FontLibNative library{};
    library.iface = &iface;
    library.alloc_ctx = &state;
    int result = 12345;
    bool primary = false;
    bool secondary = false;
    try {
        result = driver->init(&memory, &library);
    } catch (const PrimaryFailure&) {
        primary = true;
    } catch (const SecondaryFailure&) {
        secondary = true;
    }
    const bool throws = behavior == Behavior::ContextThrow || behavior == Behavior::MemoryThrow || behavior == Behavior::MemoryAndCleanupThrow;
    const bool outcome = throws ? primary && !secondary && result == 12345 : !primary && !secondary &&
        result == (behavior == Behavior::Success ? SCE_FONT_OK : SCE_FONT_ERROR_ALLOCATION_FAILED);
    const bool publication = behavior == Behavior::Success ? library.fontset_registry == state.blocks[0].pointer : library.fontset_registry == nullptr;
    if (behavior == Behavior::Success) Require(driver->term(&library) == SCE_FONT_OK && !library.fontset_registry);
    const auto retained = Live();
    const bool hasContext = behavior != Behavior::ContextNull && behavior != Behavior::ContextThrow;
    const bool qualified = state.contextStage && (state.memoryStage == hasContext);
    const bool cleanup = state.contextFrees == (hasContext ? 1u : 0u) &&
        (behavior != Behavior::MemoryAndCleanupThrow || state.cleanupThrew);
    std::fprintf(stderr, "stage=ft_memory_request behavior=%d qualified=%d context_stage=%d memory_stage=%d retained=%zu context_frees=%u primary=%d secondary=%d cleanup_threw=%d\n",
        static_cast<int>(behavior), qualified, state.contextStage, state.memoryStage, retained, state.contextFrees, primary, secondary, state.cleanupThrew);
    for (std::size_t i = 0; i < state.records; ++i) {
        if (!state.blocks[i].live) continue;
        std::free(state.blocks[i].pointer);
        state.blocks[i].live = false;
    }
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(qualified && outcome && publication && cleanup && retained == 0 && Live() == 0);
}

}

int main(int argc, char** argv) {
    if (argc == 2) {
        const int mode = std::atoi(argv[1]);
        Require(mode >= 0 && mode <= 5);
        Check(static_cast<Behavior>(mode));
        return 0;
    }
    Require(argc == 1);
    for (Behavior mode : {Behavior::Success, Behavior::ContextNull, Behavior::ContextThrow, Behavior::MemoryNull, Behavior::MemoryThrow, Behavior::MemoryAndCleanupThrow}) Check(mode);
}

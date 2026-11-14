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

enum class Kind { Library, Renderer };
enum class Behavior { Success, WorkspaceNull, WorkspaceThrow, CallbackError, CallbackThrow, CallbackAndCleanupThrow };
struct PrimaryFailure {};
struct SecondaryFailure {};
struct Block { void* pointer = nullptr; std::uint32_t size = 0; bool live = false; };
struct State {
    Kind kind = Kind::Library;
    Behavior behavior = Behavior::Success;
    std::array<Block, 4096> blocks{};
    std::size_t records = 0;
    std::size_t allocateCalls = 0;
    std::size_t callbackCalls = 0;
    bool workspaceStage = false;
    bool callbackStage = false;
    bool cleanupThrew = false;
};
State state;
const Font::SysDriver* actualDriver = nullptr;
const Font::RendererSelection* actualRenderer = nullptr;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "font factory unwind check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

std::size_t Live() {
    std::size_t count = 0;
    for (std::size_t i = 0; i < state.records; ++i) count += state.blocks[i].live;
    return count;
}

void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++state.allocateCalls;
    if (state.allocateCalls == 2) {
        Require(size == 0x4000 && state.records == 1 && Live() == 1);
        Require(state.blocks[0].size == (state.kind == Kind::Library ? sizeof(Font::FontLibNative) : sizeof(Font::RendererFt)));
        state.workspaceStage = true;
        if (state.behavior == Behavior::WorkspaceThrow) throw PrimaryFailure{};
        if (state.behavior == Behavior::WorkspaceNull) return nullptr;
    }
    Require(state.records < state.blocks.size());
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    state.blocks[state.records++] = {pointer, size, true};
    return pointer;
}

void APS5_VABI Release(void*, void* pointer) {
    if (!pointer) return;
    for (std::size_t i = 0; i < state.records; ++i) {
        Block& block = state.blocks[i];
        if (block.pointer != pointer || !block.live) continue;
        block.live = false;
        std::free(pointer);
        if (state.behavior == Behavior::CallbackAndCleanupThrow && i == 1 && !state.cleanupThrew) {
            state.cleanupThrew = true;
            throw SecondaryFailure{};
        }
        return;
    }
    Require(false);
}

void QualifyCallback(const void* object, const void* workspace) {
    ++state.callbackCalls;
    Require(state.allocateCalls == 2 && state.records == 2 && Live() == 2);
    Require(object == state.blocks[0].pointer && workspace == state.blocks[1].pointer);
    Require(state.blocks[1].size == 0x4000);
    state.callbackStage = true;
}

int APS5_VABI Initialize(const FontMemory* memory, Font::FontLibNative* library) {
    QualifyCallback(library, library->workspace);
    if (state.behavior == Behavior::CallbackThrow || state.behavior == Behavior::CallbackAndCleanupThrow) throw PrimaryFailure{};
    if (state.behavior == Behavior::CallbackError) return SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
    return actualDriver->init(memory, library);
}

int APS5_VABI Create(Font::RendererNative* renderer) {
    QualifyCallback(renderer, renderer->workspace);
    if (state.behavior == Behavior::CallbackThrow || state.behavior == Behavior::CallbackAndCleanupThrow) throw PrimaryFailure{};
    if (state.behavior == Behavior::CallbackError) return SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
    return actualRenderer->create_fn(renderer);
}

void Check(Kind kind, Behavior behavior) {
    Require(Live() == 0);
    state = {};
    state.kind = kind;
    state.behavior = behavior;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, &state, nullptr, nullptr) == SCE_FONT_OK);
    Font::SysDriver driver = *actualDriver;
    driver.init = Initialize;
    Font::RendererSelection rendererSelection = *actualRenderer;
    rendererSelection.create_fn = Create;
    FontLibrary library = nullptr;
    FontRenderer renderer = nullptr;
    int result = 12345;
    bool primary = false;
    bool secondary = false;
    try {
        result = kind == Kind::Library ? sceFontCreateLibraryWithEdition(&memory, &driver, 0, &library) :
            sceFontCreateRendererWithEdition(&memory, &rendererSelection, 0, &renderer);
    } catch (const PrimaryFailure&) {
        primary = true;
    } catch (const SecondaryFailure&) {
        secondary = true;
    }
    const bool throws = behavior == Behavior::WorkspaceThrow || behavior == Behavior::CallbackThrow || behavior == Behavior::CallbackAndCleanupThrow;
    const bool expectedResult = throws ? primary && !secondary && result == 12345 :
        !primary && !secondary && result == (behavior == Behavior::WorkspaceNull ? SCE_FONT_ERROR_ALLOCATION_FAILED :
                                            behavior == Behavior::CallbackError ? SCE_FONT_ERROR_NO_SUPPORT_FUNCTION : SCE_FONT_OK);
    const bool expectedOutput = behavior == Behavior::Success ? (kind == Kind::Library ? library != nullptr : renderer != nullptr) :
        library == nullptr && renderer == nullptr;
    if (behavior == Behavior::Success) {
        Require(kind == Kind::Library ? sceFontDestroyLibrary(&library) == SCE_FONT_OK : sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    }
    const auto retained = Live();
    const bool callbackWanted = behavior != Behavior::WorkspaceNull && behavior != Behavior::WorkspaceThrow;
    const bool qualified = state.workspaceStage && state.callbackCalls == (callbackWanted ? 1u : 0u) && (!callbackWanted || state.callbackStage);
    const bool cleanupQualified = behavior != Behavior::CallbackAndCleanupThrow || state.cleanupThrew;
    std::fprintf(stderr, "font factory stage: kind=%d behavior=%d callbacks=%zu workspace_stage=%d callback_stage=%d retained=%zu cleanup_threw=%d\n",
        static_cast<int>(kind), static_cast<int>(behavior), state.callbackCalls, state.workspaceStage, state.callbackStage, retained, state.cleanupThrew);
    for (std::size_t i = 0; i < state.records; ++i) {
        if (!state.blocks[i].live) continue;
        std::free(state.blocks[i].pointer);
        state.blocks[i].live = false;
    }
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(expectedResult && expectedOutput && qualified && cleanupQualified && retained == 0 && Live() == 0);
}

}

int main(int argc, char** argv) {
    actualDriver = static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    actualRenderer = static_cast<const Font::RendererSelection*>(sceFontSelectRendererFt(0));
    Require(actualDriver != nullptr && actualRenderer != nullptr);
    if (argc == 3) {
        const int kind = std::atoi(argv[1]);
        const int behavior = std::atoi(argv[2]);
        Require(kind >= 0 && kind <= 1 && behavior >= 0 && behavior <= 5);
        Check(static_cast<Kind>(kind), static_cast<Behavior>(behavior));
        return 0;
    }
    Require(argc == 1);
    for (Kind kind : {Kind::Library, Kind::Renderer}) {
        for (Behavior behavior : {Behavior::Success, Behavior::WorkspaceNull, Behavior::WorkspaceThrow,
                                  Behavior::CallbackError, Behavior::CallbackThrow, Behavior::CallbackAndCleanupThrow}) Check(kind, behavior);
    }
    std::puts("font factory unwind stages passed");
}

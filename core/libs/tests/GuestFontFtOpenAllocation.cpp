#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "FontFixture.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <unordered_set>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYSTEM_H

namespace {

enum class Behavior { Success, ReturnNull, Throw };

struct AllocationFailure {};

struct State {
    Behavior behavior = Behavior::Success;
    bool armed = false;
    bool tracking = false;
    bool stageQualified = false;
    std::size_t armedCalls = 0;
    std::size_t nativeAllocations = 0;
    std::size_t nativeReleases = 0;
    FT_Alloc_Func allocate = nullptr;
    FT_Free_Func release = nullptr;
    FT_Realloc_Func reallocate = nullptr;
    std::unordered_set<void*> guestBlocks;
    std::unordered_set<void*> nativeBlocks;
};

State state;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "font open allocation check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

void* APS5_VABI GuestAllocate(void*, std::uint32_t size) {
    if (state.armed) {
        ++state.armedCalls;
        Require(size == sizeof(Font::FontObj));
        Require(state.tracking && !state.nativeBlocks.empty());
        state.stageQualified = true;
        if (state.behavior == Behavior::Throw) throw AllocationFailure{};
        if (state.behavior == Behavior::ReturnNull) return nullptr;
    }
    void* block = std::malloc(size);
    if (block) Require(state.guestBlocks.insert(block).second);
    return block;
}

void APS5_VABI GuestRelease(void*, void* block) {
    if (block) Require(state.guestBlocks.erase(block) == 1);
    std::free(block);
}

void* NativeAllocate(FT_Memory memory, long size) {
    void* block = state.allocate(memory, size);
    if (state.tracking && block) {
        Require(state.nativeBlocks.insert(block).second);
        ++state.nativeAllocations;
    }
    return block;
}

void NativeRelease(FT_Memory memory, void* block) {
    if (state.tracking && state.nativeBlocks.erase(block)) ++state.nativeReleases;
    state.release(memory, block);
}

void* NativeReallocate(FT_Memory memory, long oldSize, long newSize, void* block) {
    const bool tracked = state.tracking && state.nativeBlocks.erase(block) != 0;
    void* result = state.reallocate(memory, oldSize, newSize, block);
    if (state.tracking) {
        if (result) {
            Require(state.nativeBlocks.insert(result).second);
        } else if (newSize > 0 && tracked) {
            Require(state.nativeBlocks.insert(block).second);
        }
    }
    return result;
}

void Check(Behavior behavior) {
    Require(state.guestBlocks.empty() && state.nativeBlocks.empty());
    state = {};
    const FontMemoryInterface iface{GuestAllocate, GuestRelease, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    memory.iface = &iface;
    Font::FontLibNative library{};
    library.iface = &iface;
    library.sysfont_flags = Font::SYSFONT_FLAG_SYSTEM_SET;
    const auto* driver = static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    Require(driver != nullptr && driver->init(&memory, &library) == SCE_FONT_OK);
    const auto font = FontFixture::BitmapFont();
    Font::FontObj* head = nullptr;
    Require(driver->open(&library, 1, font.data(), static_cast<std::uint32_t>(font.size()), 0, 0, &head) == SCE_FONT_OK);
    Require(head != nullptr && head->ft_face != nullptr);
    Font::FontObj* const warm = head;
    auto* nativeMemory = static_cast<FT_Face>(warm->ft_face)->memory;
    Require(nativeMemory != nullptr && nativeMemory->alloc != nullptr && nativeMemory->free != nullptr && nativeMemory->realloc != nullptr);
    state.allocate = nativeMemory->alloc;
    state.release = nativeMemory->free;
    state.reallocate = nativeMemory->realloc;
    nativeMemory->alloc = NativeAllocate;
    nativeMemory->free = NativeRelease;
    nativeMemory->realloc = NativeReallocate;
    state.behavior = behavior;
    state.tracking = true;
    state.armed = true;
    int result = 12345;
    bool caught = false;
    try {
        result = driver->open(&library, 1, font.data(), static_cast<std::uint32_t>(font.size()), 0, 0, &head);
    } catch (const AllocationFailure&) {
        caught = true;
    }
    state.armed = false;
    const bool expectedResult = behavior == Behavior::Throw ? caught && result == 12345 :
        !caught && result == (behavior == Behavior::ReturnNull ? SCE_FONT_ERROR_ALLOCATION_FAILED : SCE_FONT_OK);
    bool publicationPreserved = false;
    if (behavior == Behavior::Success && result == SCE_FONT_OK) {
        publicationPreserved = head != warm && head->next == warm && warm->prev == head && head->ft_face != nullptr;
        Require(driver->close(head, 0) == SCE_FONT_OK);
        head = warm;
    } else {
        publicationPreserved = head == warm;
    }
    publicationPreserved = publicationPreserved && warm->prev == nullptr;
    const auto beforeTermination = state.nativeBlocks.size();
    const bool stageQualified = state.stageQualified && state.armedCalls == 1 && state.nativeAllocations > 0;
    std::fprintf(stderr, "FontFt late allocation: behavior=%d guest_calls=%zu native_allocs=%zu native_frees=%zu native_live_before_term=%zu\n",
        static_cast<int>(behavior), state.armedCalls, state.nativeAllocations, state.nativeReleases, beforeTermination);
    Require(driver->close(warm, 0) == SCE_FONT_OK);
    Require(driver->term(&library) == SCE_FONT_OK);
    Require(state.nativeBlocks.empty() && state.guestBlocks.empty());
    Require(stageQualified && expectedResult && publicationPreserved);
    Require(beforeTermination == 0);
}

}

int main() {
    static_assert(sizeof(Font::FontObj) == 512);
    Check(Behavior::Success);
    Check(Behavior::ReturnNull);
    Check(Behavior::Throw);
    std::puts("font open late-allocation cleanup scenarios passed");
}

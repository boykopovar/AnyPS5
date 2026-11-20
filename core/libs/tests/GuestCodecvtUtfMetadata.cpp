#include "prx/libSceLibcInternal/include/CodecvtUtfMetadata.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Unshift = int (APS5_VABI*)(const void*, void*, char*, char*, char**);
using Metadata = int (APS5_VABI*)(const void*);

static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix), Unshift>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix), Unshift>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix), Metadata>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix), Metadata>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix), Metadata>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "UTF codecvt metadata: %s\n", message);
    std::abort();
}

template<typename TCall>
void RequireInvalid(TCall call, const char* text) {
    bool threw = false;
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument& error) {
        threw = std::strstr(error.what(), text) != nullptr;
    }
    Require(threw && errno == 71, "invalid guest memory reports the expected failure without changing errno");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guard page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded allocation succeeds");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guard page protection succeeds");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    std::byte* End() { return memory + pageSize; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only protection succeeds");
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only protection succeeds");
#endif
    }

private:
    std::byte* memory = nullptr;
    std::size_t pageSize = 0;
};

void CheckModes() {
    constexpr std::array<int, 8> lengths32{6, 6, 9, 9, 9, 9, 9, 9};
    constexpr std::array<int, 8> encodings32{0, 0, -1, -1, -1, -1, -1, -1};
    constexpr std::array<int, 8> lengths16{6, 6, 7, 7, 9, 9, 9, 9};
    alignas(8) std::array<std::byte, 32> facet{};
    for (const std::uint32_t extra : {0u, 0x80u, 0x100u, 0xabcdef00u, 0xfffffff8u}) {
        for (std::uint32_t bits = 0; bits < 8; ++bits) {
            const std::uint32_t mode = extra | bits;
            std::memcpy(facet.data() + 0x18, &mode, sizeof(mode));
            const auto before = facet;
            errno = 71;
            Require(_ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix(facet.data()) == encodings32[bits], "char32 encoding follows both header bits");
            Require(_ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix(facet.data()) == lengths32[bits], "char32 maximum length follows both header bits");
            Require(_ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix(facet.data()) == lengths16[bits], "char16 consume-header wins over generate-header");
            Require(facet == before && errno == 71, "metadata leaves the facet and errno untouched");
        }
    }
}

void CheckUnshift() {
    constexpr Unshift unshift32 = _ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix;
    constexpr Unshift unshift16 = _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix;
    std::array<char, 4> output{'a', 'b', 'c', 'd'};
    const auto beforeOutput = output;
    char* next = nullptr;
    void* inaccessible = reinterpret_cast<void*>(std::uintptr_t{0x10});
    errno = 71;
    Require(unshift32(inaccessible, inaccessible, output.data(), nullptr, &next) == 0 && next == output.data(), "char32 unshift ignores facet, state and end");
    Require(unshift32(nullptr, nullptr, nullptr, reinterpret_cast<char*>(inaccessible), &next) == 0 && next == nullptr, "char32 unshift accepts a null first pointer");
    Require(output == beforeOutput && errno == 71, "unshift produces no bytes and preserves errno");

    constexpr std::array<std::uint16_t, 4> states{0, 1, 2, 65535};
    constexpr std::array<int, 4> results{0, 0, 2, 2};
    alignas(8) std::array<std::byte, 8> state{};
    for (std::size_t i = 0; i < states.size(); ++i) {
        state.fill(std::byte{0xa5});
        std::memcpy(state.data(), &states[i], sizeof(states[i]));
        const auto before = state;
        next = nullptr;
        Require(unshift16(inaccessible, state.data(), output.data(), nullptr, &next) == results[i], "char16 unshift classifies the pending surrogate word");
        Require(next == output.data() && state == before && output == beforeOutput, "char16 unshift changes only the next pointer");
    }

    for (const std::uintptr_t address : {0u, 1u, 2u, 65535u}) {
        state.fill(std::byte{0xff});
        char* first = reinterpret_cast<char*>(address);
        Require(unshift16(nullptr, state.data(), first, nullptr, reinterpret_cast<char**>(state.data())) == (address < 2 ? 0 : 2), "an aliased output write precedes the shift-state read");
        char* stored = nullptr;
        std::memcpy(&stored, state.data(), sizeof(stored));
        Require(stored == first, "the aliased next pointer receives all pointer bytes");
    }

    next = nullptr;
    RequireInvalid([&] { unshift16(nullptr, inaccessible, output.data(), nullptr, &next); }, "shift state");
    Require(next == output.data(), "a failed shift-state read retains the preceding next-pointer write");
    RequireInvalid([&] { unshift16(nullptr, inaccessible, output.data(), nullptr, nullptr); }, "next pointer");
    RequireInvalid([&] { unshift32(nullptr, nullptr, output.data(), nullptr, reinterpret_cast<char**>(inaccessible)); }, "next pointer");
}

void CheckGuardedMemory() {
    GuardedPages facetPages;
    facetPages.End()[-1] = std::byte{4};
    const void* facet = facetPages.End() - 0x19;
    Require(_ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix(facet) == -1, "char32 encoding reads only one mode byte");
    Require(_ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix(facet) == 9, "char32 maximum length reads only one mode byte");
    RequireInvalid([&] { _ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix(facet); }, "facet mode");
    facetPages.ReadOnly();
    Require(_ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix(facet) == -1, "metadata accepts a read-only facet");

    GuardedPages statePages;
    const std::uint16_t state = 1;
    std::memcpy(statePages.End() - 2, &state, sizeof(state));
    char byte = 'x';
    char* next = nullptr;
    Require(_ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(nullptr, statePages.End() - 2, &byte, nullptr, &next) == 0, "char16 unshift reads exactly two state bytes");
    Require(next == &byte, "the guarded state leaves the correct next pointer");
    next = nullptr;
    RequireInvalid([&] { _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(nullptr, statePages.End() - 1, &byte, nullptr, &next); }, "shift state");
    Require(next == &byte, "a state read crossing the guard preserves the output write");

    GuardedPages outputPages;
    auto* destination = reinterpret_cast<char**>(outputPages.End() - sizeof(char*));
    char* original = nullptr;
    std::memcpy(destination, &original, sizeof(original));
    outputPages.ReadOnly();
    RequireInvalid([&] { _ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(nullptr, nullptr, &byte, nullptr, destination); }, "next pointer");
    RequireInvalid([&] { _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(nullptr, nullptr, &byte, nullptr, destination); }, "next pointer");
    char* stored = &byte;
    std::memcpy(&stored, destination, sizeof(stored));
    Require(stored == original, "a read-only next pointer remains unchanged");
}

void CheckInvalidFacets() {
    const std::array<Metadata, 3> methods{
        _ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix,
        _ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix,
        _ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix};
    for (Metadata method : methods) {
        RequireInvalid([&] { method(nullptr); }, "facet mode");
        RequireInvalid([&] { method(reinterpret_cast<void*>(std::uintptr_t{0x10})); }, "facet mode");
        RequireInvalid([&] { method(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 0x10)); }, "facet address");
    }
}

void WarmUpHostExceptionRuntime() {
    try {
        throw std::runtime_error("warm-up");
    } catch (const std::runtime_error&) {
    }
}

}

int main() {
    WarmUpHostExceptionRuntime();
    CheckModes();
    CheckUnshift();
    CheckGuardedMemory();
    CheckInvalidFacets();
    std::puts("UTF codecvt metadata checks passed");
    return 0;
}

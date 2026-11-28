#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

struct Case {
    std::uint32_t input;
    std::uint32_t expected;
};

constexpr Case KeyCodeToUnicode[] = {
    {0x00000000, 0x00000000},
    {0x0000001f, 0x00000000},
    {0x00000020, 0x00000020},
    {0x00000041, 0x00000041},
    {0x0000007e, 0x0000007e},
    {0x0000007f, 0x00000000},
    {0x0000009f, 0x00000000},
    {0x000000a0, 0x000000a0},
    {0x000000e9, 0x000000e9},
    {0x000000ff, 0x000000ff},
    {0x00000100, 0x00000000},
    {0x000001a1, 0x00000104},
    {0x000001a4, 0x00000000},
    {0x000001ff, 0x000002d9},
    {0x000006c1, 0x00000430},
    {0x00000aa2, 0x00002002},
    {0x000013bc, 0x00000152},
    {0x000020ac, 0x000020ac},
    {0x000020ad, 0x00000000},
    {0x0000ff08, 0x00000008},
    {0x0000ff0d, 0x0000000d},
    {0x0000ff80, 0x00000020},
    {0x0000ffb5, 0x00000035},
    {0x0000ffbd, 0x0000003d},
    {0x0000ffbe, 0x00000000},
    {0x0000ffff, 0x0000007f},
    {0x00010000, 0x00000000},
    {0x0100263a, 0x0000263a},
    {0x01000000, 0x00000000},
    {0x01ffffff, 0x00ffffff},
    {0x02000041, 0x00000000},
    {0x11000041, 0x00000000},
    {0xff000041, 0x00000000},
    {0xffffffff, 0x00000000},
};

constexpr Case UnicodeToKeyCode[] = {
    {0x00000000, 0x01000000},
    {0x00000008, 0x0000ff08},
    {0x00000009, 0x0000ff09},
    {0x0000000a, 0x0000ff0a},
    {0x0000000b, 0x0000ff0b},
    {0x0000000c, 0x0100000c},
    {0x0000000d, 0x0000ff0d},
    {0x0000001b, 0x0000ff1b},
    {0x0000001f, 0x0100001f},
    {0x00000020, 0x00000020},
    {0x00000041, 0x00000041},
    {0x0000007e, 0x0000007e},
    {0x0000007f, 0x0100007f},
    {0x0000009f, 0x0100009f},
    {0x000000a0, 0x000000a0},
    {0x000000e9, 0x000000e9},
    {0x000000ff, 0x000000ff},
    {0x00000100, 0x000003c0},
    {0x00000104, 0x000001a1},
    {0x00000152, 0x000013bc},
    {0x000002d8, 0x000001a2},
    {0x00002002, 0x00000aa2},
    {0x000020ac, 0x000020ac},
    {0x0000263a, 0x0100263a},
    {0x0000318e, 0x00000ef7},
    {0x0000318f, 0x0100318f},
    {0x0000ffff, 0x0100ffff},
    {0x00010000, 0x01010000},
    {0x0010ffff, 0x0110ffff},
    {0x01000041, 0x01000041},
    {0xffffffff, 0xffffffff},
};

template <typename Function>
int Check(const char* name, Function function, const Case* cases, std::size_t count) {
    int failures = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint32_t actual = function(cases[index].input);
        if (actual != cases[index].expected) {
            std::fprintf(stderr, "%s(0x%08x) returned 0x%08x, expected 0x%08x\n", name,
                static_cast<unsigned>(cases[index].input), static_cast<unsigned>(actual),
                static_cast<unsigned>(cases[index].expected));
            ++failures;
        }
    }
    return failures;
}

std::size_t lastCallocSize = 0;

int CheckPasteboardString() {
    NKWebKit::WpePasteboardString string{};
    char contents[] = {'a', '\0', 'b'};
    constexpr char expected[] = {'a', '\0', 'b'};
    wpe_pasteboard_string_initialize(&string, contents, sizeof(contents));
    char* allocation = string.data;
    contents[0] = 'c';
    wpe_pasteboard_string_initialize(&string, "d", 1);
    const bool preserved = string.data && string.data != contents && string.data == allocation &&
        string.length == sizeof(expected) && std::memcmp(string.data, expected, sizeof(expected)) == 0 &&
        lastCallocSize == sizeof(expected) + 1 && string.data[string.length] == '\0';
    wpe_pasteboard_string_free(&string);
    const bool cleared = !string.data && string.length == 0;
    wpe_pasteboard_string_free(&string);
    wpe_pasteboard_string_initialize(&string, nullptr, 0);
    const bool empty = string.data && string.length == 0 && lastCallocSize == 1 && string.data[0] == '\0';
    wpe_pasteboard_string_free(&string);
    bool rejectedString = false;
    try {
        wpe_pasteboard_string_initialize(nullptr, "a", 1);
    } catch (const std::invalid_argument&) {
        rejectedString = true;
    }
    bool rejectedContents = false;
    try {
        wpe_pasteboard_string_initialize(&string, nullptr, 1);
    } catch (const std::invalid_argument&) {
        rejectedContents = true;
    }
    bool rejectedOversize = false;
    try {
        wpe_pasteboard_string_initialize(&string, "a", std::numeric_limits<std::size_t>::max());
    } catch (const std::bad_alloc&) {
        rejectedOversize = true;
    }
    const bool untouched = !string.data && string.length == 0;
    if (!preserved || !cleared || !empty || !untouched || !rejectedString || !rejectedContents || !rejectedOversize) {
        std::fprintf(stderr, "WPE pasteboard string lifecycle failed.\n");
        return 1;
    }
    return 0;
}

} // namespace

namespace {

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCallocate(std::size_t count, std::size_t bytes) {
    lastCallocSize = count * bytes;
    return std::calloc(count, bytes);
}
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

std::array<void*, 10> HeapApi() {
    return {reinterpret_cast<void*>(HeapAllocate), reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate), reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign)};
}

}

int main() {
    std::array<void*, 10> heapApi = HeapApi();
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    int failures = Check("wpe_key_code_to_unicode", wpe_key_code_to_unicode_nid_postfix, KeyCodeToUnicode,
        sizeof(KeyCodeToUnicode) / sizeof(KeyCodeToUnicode[0]));
    failures += Check("wpe_unicode_to_key_code", wpe_unicode_to_key_code_nid_postfix, UnicodeToKeyCode,
        sizeof(UnicodeToKeyCode) / sizeof(UnicodeToKeyCode[0]));
    failures += CheckPasteboardString();
    if (failures != 0) {
        std::fprintf(stderr, "%d WPE key code cases failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("Wpe tests passed.\n");
    return EXIT_SUCCESS;
}

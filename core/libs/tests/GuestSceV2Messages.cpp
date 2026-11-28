#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v28messagesIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28messagesIwE2idE_nid_postfix;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

template<typename Text> struct GuestString {
    union {
        Text buffer[16 / sizeof(Text)];
        Text* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;

    static constexpr std::uint64_t InlineCapacity = 16 / sizeof(Text) - 1;
    const Text* data() const { return capacity > InlineCapacity ? pointer : buffer; }
    std::basic_string<Text> text() const { return std::basic_string<Text>(data(), size); }
};

static_assert(sizeof(GuestString<char>) == 0x20);
static_assert(sizeof(GuestString<char16_t>) == 0x20);

using Open = int (APS5_VABI *)(const void*, const GuestString<char>*, const void*);
template<typename Text> using Get = GuestString<Text>* (APS5_VABI *)(GuestString<Text>*, const void*, int, int, int, const GuestString<Text>*);
using Close = void (APS5_VABI *)(const void*, int);

template<typename Text> static GuestString<Text> RunGet(const void* facet, const GuestString<Text>& fallback) {
    GuestString<Text> result;
    std::memset(&result, 0xa5, sizeof(result));
    Require(Slot<Get<Text>>(facet, 5)(&result, facet, 3, 7, 11, &fallback) == &result, "get returns its result");
    return result;
}

template<typename Text> static void CheckMessages(const void* facet, const std::string& name) {
    GuestString<char> catalog{};
    std::memcpy(catalog.buffer, "app", 4);
    catalog.size = 3;
    catalog.capacity = 15;
    Require(Slot<Open>(facet, 4)(facet, &catalog, nullptr) == -1, name + " open finds no catalog");

    GuestString<Text> inlineFallback;
    std::memset(&inlineFallback, 0x5c, sizeof(inlineFallback));
    inlineFallback.buffer[0] = Text('o');
    inlineFallback.buffer[1] = Text('k');
    inlineFallback.buffer[2] = Text(0);
    inlineFallback.size = 2;
    inlineFallback.capacity = GuestString<Text>::InlineCapacity;
    const auto copied = RunGet(facet, inlineFallback);
    Require(copied.size == 2 && copied.capacity == GuestString<Text>::InlineCapacity, name + " get keeps a short default inline");
    Require(std::memcmp(copied.buffer, inlineFallback.buffer, sizeof(copied.buffer)) == 0, name + " get copies the whole inline buffer of the default");

    const std::basic_string<Text> longText(40, Text('m'));
    GuestString<Text> heapFallback{};
    heapFallback.pointer = const_cast<Text*>(longText.c_str());
    heapFallback.size = longText.size();
    heapFallback.capacity = 63;
    const auto long_ = RunGet(facet, heapFallback);
    Require(long_.size == 40 && long_.capacity == (40 | GuestString<Text>::InlineCapacity) && long_.data() != longText.c_str(), name + " get allocates for a long default with its own length");
    Require(long_.text() == longText && long_.data()[40] == 0, name + " get copies a long default with its terminator");
    const std::basic_string<Text> unterminated(41, Text('u'));
    heapFallback.pointer = const_cast<Text*>(unterminated.c_str());
    const auto copiedEnd = RunGet(facet, heapFallback);
    Require(copiedEnd.size == 40 && copiedEnd.data()[40] == Text('u'), name + " get copies the default's character after its length");

    const std::basic_string<Text> shortOnHeap(2, Text('h'));
    GuestString<Text> spare{};
    spare.pointer = const_cast<Text*>(shortOnHeap.c_str());
    spare.size = 1;
    spare.capacity = 31;
    const auto shrunk = RunGet(facet, spare);
    Require(shrunk.size == 1 && shrunk.capacity == GuestString<Text>::InlineCapacity && shrunk.buffer[0] == Text('h'), name + " get moves a short heap default inline");

    Slot<Close>(facet, 6)(facet, -1);
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    static const GuestLocale::SceV2Locinfo info{0, "C", "C"};
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);

    const void* narrow = locale->facets[_ZNSt8__sce_v28messagesIcE2idE_nid_postfix];
    const void* wide = locale->facets[_ZNSt8__sce_v28messagesIwE2idE_nid_postfix];
    Require(narrow != nullptr && wide != nullptr, "_Makeloc installs both messages facets");
    if (narrow != nullptr) CheckMessages<char>(narrow, "messages<char>");
    if (wide != nullptr) CheckMessages<char16_t>(wide, "messages<wchar_t>");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}

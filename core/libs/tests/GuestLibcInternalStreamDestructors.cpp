#include "prx/libSceLibcInternal/include/StreamDestructors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;
const char* currentStream = "";

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal stream destructors: %s: %s\n", currentStream, message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct VirtualTable {
    std::ptrdiff_t virtualOffset;
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    std::array<const void*, 2> functions;

    const void* AddressPoint() const { return functions.data(); }
};

static_assert(offsetof(VirtualTable, functions) - offsetof(VirtualTable, virtualOffset) == 24);

template <typename TChar>
struct GuestBasicIos {
    GuestLocale::IosBase base;
    void* streambuf;
    void* tie;
    TChar fill;
};

template <typename TChar>
struct GuestIstream {
    const void* vtable;
    std::int64_t count;
    GuestBasicIos<TChar> ios;
};

template <typename TChar>
struct GuestOstream {
    const void* vtable;
    GuestBasicIos<TChar> ios;
};

template <typename TStream>
struct Guarded {
    std::uint64_t before = 0x12345678;
    TStream stream{};
    std::uint64_t after = 0x87654321;
};

using Destructor = void (APS5_VABI*)(void*);

struct Exports {
    const char* name;
    Destructor complete;
    Destructor deleting;
    Destructor completeThunk;
    Destructor deletingThunk;
};

template <typename TStream>
void Prepare(TStream& stream, const VirtualTable& primary, const VirtualTable& secondary) {
    stream.vtable = primary.AddressPoint();
    stream.ios.base.vtable = secondary.AddressPoint();
    stream.ios.base.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    stream.ios.base.width = 7;
    stream.ios.base.flags = 0x201;
}

template <typename TStream>
TStream* NewStream(const VirtualTable& primary, const VirtualTable& secondary) {
    auto* stream = new (_Znwm_nid_postfix(sizeof(TStream))) TStream{};
    Prepare(*stream, primary, secondary);
    return stream;
}

template <typename TStream>
void CheckStream(const Exports& exports) {
    currentStream = exports.name;
    constexpr std::ptrdiff_t offset = offsetof(TStream, ios);
    const VirtualTable primary{offset, 0, nullptr, {}};
    const VirtualTable secondary{-offset, -offset, nullptr, {}};

    releases = 0;
    Guarded<TStream> object;
    Prepare(object.stream, primary, secondary);
    exports.complete(&object.stream);
    Require(object.stream.ios.base.locale == nullptr, "the complete destructor runs the ios_base destructor on the virtual base");
    Require(object.stream.vtable == primary.AddressPoint(), "the complete destructor leaves the stream's own vtable pointer");
    Require(object.before == 0x12345678 && object.after == 0x87654321, "the complete destructor writes only the stream");
    Require(releases == 0, "the complete destructor frees nothing");

    Guarded<TStream> expected;
    Guarded<TStream> actual;
    Prepare(expected.stream, primary, secondary);
    Prepare(actual.stream, primary, secondary);
    _ZNSt8ios_baseD2Ev_nid_postfix(&expected.stream.ios.base);
    exports.complete(&actual.stream);
    Require(actual.stream.ios.base.locale == expected.stream.ios.base.locale && actual.stream.ios.base.width == expected.stream.ios.base.width
            && actual.stream.ios.base.flags == expected.stream.ios.base.flags,
        "the complete destructor leaves the virtual base as the ios_base destructor does");

    Guarded<TStream> standard;
    Prepare(standard.stream, primary, secondary);
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&standard.stream.ios.base);
    _ZNSt8ios_base7_AddstdEPS__nid_postfix(&standard.stream.ios.base);
    exports.complete(&standard.stream);
    Require(standard.stream.ios.base.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix, "a standard stream registered twice keeps its locale after one destruction");
    exports.complete(&standard.stream);
    Require(standard.stream.ios.base.locale == nullptr, "the last destruction of a standard stream releases its locale");

    Guarded<TStream> adjusted;
    Prepare(adjusted.stream, primary, secondary);
    exports.completeThunk(&adjusted.stream.ios);
    Require(adjusted.stream.ios.base.locale == nullptr, "the complete destructor thunk destroys the stream that holds the virtual base");
    Require(adjusted.before == 0x12345678 && adjusted.after == 0x87654321, "the complete destructor thunk writes only the stream");
    Require(releases == 0, "the complete destructor thunk frees nothing");

    auto* heapStream = NewStream<TStream>(primary, secondary);
    releases = 0;
    exports.deleting(heapStream);
    Require(releases == 1 && lastReleased == heapStream, "the deleting destructor frees the stream through operator delete");

    heapStream = NewStream<TStream>(primary, secondary);
    releases = 0;
    exports.deletingThunk(&heapStream->ios);
    Require(releases == 1 && lastReleased == heapStream, "the deleting destructor thunk frees the whole stream, not its virtual base");

    GuestLocale::Implementation* otherLocale = nullptr;
    heapStream = NewStream<TStream>(primary, secondary);
    heapStream->ios.base.locale = &otherLocale;
    releases = 0;
    Require(Throws([&] { exports.deleting(heapStream); }), "an ios_base destructor failure reaches the caller of the deleting destructor");
    Require(Throws([&] { exports.deletingThunk(&heapStream->ios); }), "an ios_base destructor failure reaches the caller of the deleting destructor thunk");
    Require(releases == 0, "a stream whose destruction fails is not freed");
    heapStream->ios.base.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    exports.deleting(heapStream);
    Require(releases == 1 && lastReleased == heapStream, "the stream is freed once its destruction succeeds");

    auto* const unmapped = reinterpret_cast<void*>(std::uintptr_t{0x10});
    Guarded<TStream> broken;
    Prepare(broken.stream, primary, secondary);
    broken.stream.vtable = nullptr;
    broken.stream.ios.base.vtable = nullptr;
    Guarded<TStream> unreadableVtable;
    Prepare(unreadableVtable.stream, primary, secondary);
    unreadableVtable.stream.vtable = reinterpret_cast<const void*>(std::uintptr_t{0x28});
    unreadableVtable.stream.ios.base.vtable = reinterpret_cast<const void*>(std::uintptr_t{0x28});
    releases = 0;
    for (const Destructor destructor : {exports.complete, exports.deleting}) {
        Require(Throws([&] { destructor(nullptr); }), "a null stream is rejected");
        Require(Throws([&] { destructor(unmapped); }), "an unmapped stream is rejected");
        Require(Throws([&] { destructor(&broken.stream); }), "a stream without a vtable is rejected");
        Require(Throws([&] { destructor(&unreadableVtable.stream); }), "a stream whose vtable cannot be read is rejected");
    }
    for (const Destructor destructor : {exports.completeThunk, exports.deletingThunk}) {
        Require(Throws([&] { destructor(nullptr); }), "a null virtual base is rejected");
        Require(Throws([&] { destructor(unmapped); }), "an unmapped virtual base is rejected");
        Require(Throws([&] { destructor(&broken.stream.ios); }), "a virtual base without a vtable is rejected");
        Require(Throws([&] { destructor(&unreadableVtable.stream.ios); }), "a virtual base whose vtable cannot be read is rejected");
    }
    Require(releases == 0, "a rejected stream is not freed");
    Require(broken.stream.ios.base.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix && unreadableVtable.stream.ios.base.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix,
        "a rejected stream is left alone");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckStream<GuestIstream<char>>({"basic_istream<char>", &sceLibcInternalIstreamCompleteDestructor_nid_no_patch,
        &sceLibcInternalIstreamDeletingDestructor_nid_no_patch, &sceLibcInternalIstreamCompleteDestructorThunk_nid_no_patch,
        &sceLibcInternalIstreamDeletingDestructorThunk_nid_no_patch});
    CheckStream<GuestOstream<char>>({"basic_ostream<char>", &sceLibcInternalOstreamCompleteDestructor_nid_no_patch,
        &sceLibcInternalOstreamDeletingDestructor_nid_no_patch, &sceLibcInternalOstreamCompleteDestructorThunk_nid_no_patch,
        &sceLibcInternalOstreamDeletingDestructorThunk_nid_no_patch});
    CheckStream<GuestIstream<std::uint16_t>>({"basic_istream<wchar_t>", &sceLibcInternalWideIstreamCompleteDestructor_nid_no_patch,
        &sceLibcInternalWideIstreamDeletingDestructor_nid_no_patch, &sceLibcInternalWideIstreamCompleteDestructorThunk_nid_no_patch,
        &sceLibcInternalWideIstreamDeletingDestructorThunk_nid_no_patch});
    CheckStream<GuestOstream<std::uint16_t>>({"basic_ostream<wchar_t>", &sceLibcInternalWideOstreamCompleteDestructor_nid_no_patch,
        &sceLibcInternalWideOstreamDeletingDestructor_nid_no_patch, &sceLibcInternalWideOstreamCompleteDestructorThunk_nid_no_patch,
        &sceLibcInternalWideOstreamDeletingDestructorThunk_nid_no_patch});

    std::puts("libc internal stream destructor checks passed");
    return 0;
}

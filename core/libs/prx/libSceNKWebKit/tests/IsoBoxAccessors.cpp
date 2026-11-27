#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace NKWebKit::IsoAbi;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "IsoBoxAccessors check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static ByteVector Bytes(const char* text) {
    ByteVector bytes;
    for (const char* c = text; *c; ++c) bytes.append(static_cast<std::uint8_t>(*c));
    return bytes;
}

static bool Same(const ByteVector& vector, const char* text) {
    return vector.size == std::strlen(text) && (!vector.size || std::memcmp(vector.buffer, text, vector.size) == 0);
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static bool Same(const String& string, std::u16string_view text) {
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    if (view.length != text.size()) return false;
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != text[i]) return false;
    }
    return true;
}

static void BoxAndFullBox() {
    FullBox box{};
    box.box.size = 0x1'0000'0020ULL;
    box.box.boxType = 0x7363686dU;
    box.box.extendedType = Bytes("uuid-bytes");
    box.version = 1;
    box.flags = 0x00abcdefU;
    Require(_ZNK7WebCore6ISOBox4sizeEv(&box.box) == 0x1'0000'0020ULL);
    const std::uint32_t* boxType = _ZNK7WebCore6ISOBox7boxTypeEv(&box.box);
    Require(boxType == &box.box.boxType && *boxType == 0x7363686dU);
    Require(reinterpret_cast<const unsigned char*>(boxType) - reinterpret_cast<const unsigned char*>(&box.box) == 16);
    box.box.boxType = 0x74656e63U;
    Require(*boxType == 0x74656e63U);
    box.box.boxType = 0x7363686dU;
    Require(&_ZNK7WebCore6ISOBox12extendedTypeEv(&box.box) == &box.box.extendedType);
    Require(_ZNK7WebCore10ISOFullBox7versionEv(&box) == 1);
    Require(_ZNK7WebCore10ISOFullBox5flagsEv(&box) == 0x00abcdefU);
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore6ISOBox4sizeEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore10ISOFullBox5flagsEv(nullptr); }));
}

static void TrackEncryptionBoxFields() {
    TrackEncryptionBox box{};
    box.defaultCryptByteBlock = {-3, true};
    box.defaultSkipByteBlock = {0, false};
    box.defaultIsProtected = 1;
    box.defaultPerSampleIVSize = 16;
    box.defaultKID = Bytes("0123456789abcdef");
    using OptionalGetter = unsigned char* (APS5_VABI*)(unsigned char*, const TrackEncryptionBox*);
    const auto cryptGetter = reinterpret_cast<OptionalGetter>(&_ZNK7WebCore21ISOTrackEncryptionBox21defaultCryptByteBlockEv);
    const auto skipGetter = reinterpret_cast<OptionalGetter>(&_ZNK7WebCore21ISOTrackEncryptionBox20defaultSkipByteBlockEv);
    unsigned char scratch[128];
    std::memset(scratch, 0xA5, sizeof(scratch));
    Require(cryptGetter(scratch, &box) == scratch);
    Require(scratch[0] == 1 && static_cast<signed char>(scratch[1]) == -3 && scratch[2] == 0xA5);
    std::memset(scratch, 0xA5, sizeof(scratch));
    Require(skipGetter(scratch, &box) == scratch);
    Require(scratch[0] == 0 && scratch[1] == 0 && scratch[2] == 0xA5);
    box.defaultSkipByteBlock = {127, true};
    box.defaultCryptByteBlock = {-128, false};
    std::memset(scratch, 0xA5, sizeof(scratch));
    skipGetter(scratch, &box);
    Require(scratch[0] == 1 && scratch[1] == 127 && scratch[2] == 0xA5);
    std::memset(scratch, 0xA5, sizeof(scratch));
    cryptGetter(scratch, &box);
    Require(scratch[0] == 0 && scratch[1] == 0 && scratch[2] == 0xA5);
    Require(ThrowsInvalidArgument([&] { cryptGetter(scratch, nullptr); }));
    Require(ThrowsInvalidArgument([&] { skipGetter(nullptr, &box); }));
    Require(_ZNK7WebCore21ISOTrackEncryptionBox18defaultIsProtectedEv(&box) == 1);
    Require(_ZNK7WebCore21ISOTrackEncryptionBox22defaultPerSampleIVSizeEv(&box) == 16);
    auto kid = _ZNK7WebCore21ISOTrackEncryptionBox10defaultKIDEv(&box);
    Require(Same(kid, "0123456789abcdef") && kid.buffer != box.defaultKID.buffer && kid.capacity == kid.size);
    kid.buffer[0] = 'X';
    Require(Same(box.defaultKID, "0123456789abcdef"));
    auto iv = _ZNK7WebCore21ISOTrackEncryptionBox17defaultConstantIVEv(&box);
    Require(!iv.buffer && iv.size == 0 && iv.capacity == 0);
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore21ISOTrackEncryptionBox10defaultKIDEv(nullptr); }));
}

static void ProtectionSystemSpecificHeaderFields() {
    ProtectionSystemSpecificHeaderBox box{};
    box.systemID = Bytes("edef8ba979d64ace");
    box.keyIDs.append(Bytes("first key id...."));
    box.keyIDs.append(Bytes("second key id..."));
    box.data = Bytes("payload");
    auto system = _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox8systemIDEv(&box);
    Require(Same(system, "edef8ba979d64ace") && system.buffer != box.systemID.buffer);
    auto keys = _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox6keyIDsEv(&box);
    Require(keys.size == 2 && keys.capacity == 2 && keys.buffer != box.keyIDs.buffer);
    Require(Same(keys.buffer[0], "first key id....") && Same(keys.buffer[1], "second key id..."));
    Require(keys.buffer[0].buffer != box.keyIDs.buffer[0].buffer);
    keys.buffer[1].buffer[0] = 'Z';
    Require(Same(box.keyIDs.buffer[1], "second key id..."));
    Require(Same(_ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox4dataEv(&box), "payload"));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox6keyIDsEv(nullptr); }));
}

static void WebVTTCueFields() {
    WebVTTCue cue;
    cue.presentationTime = NKWebKit::MediaAbi::MediaTime(1500, 1000, 1);
    cue.duration = NKWebKit::MediaAbi::MediaTime(250, 1000, 1);
    cue.sourceID = Latin1("source");
    cue.identifier = Latin1("cue-1");
    cue.originalStartTime = Latin1("00:00:01.500");
    cue.settings = Latin1("align:start");
    cue.cueText = Latin1("Hello");
    Require(&_ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(&cue) == &cue.presentationTime);
    Require(&_ZNK7WebCore12ISOWebVTTCue8durationEv(&cue) == &cue.duration);
    Require(&_ZNK7WebCore12ISOWebVTTCue8sourceIDEv(&cue) == &cue.sourceID);
    Require(&_ZNK7WebCore12ISOWebVTTCue2idEv(&cue) == &cue.identifier);
    Require(&_ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(&cue) == &cue.originalStartTime);
    Require(&_ZNK7WebCore12ISOWebVTTCue8settingsEv(&cue) == &cue.settings);
    Require(&_ZNK7WebCore12ISOWebVTTCue7cueTextEv(&cue) == &cue.cueText);
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore12ISOWebVTTCue7cueTextEv(nullptr); }));
}

static void WebVTTCueJSON() {
    using NKWebKit::MediaAbi::MediaTime;

    WebVTTCue cue{};
    Require(Same(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(&cue),
        uR"({"text":"","sourceId":"","id":"","originalStartTime":"","settings":"","presentationTime":null,"duration":null})"));

    const char16_t text[] = u"<caf\u00e9> \"hi\"\n";
    cue.cueText = _ZN3WTF10StringImpl6createEPKDsj(text, std::size(text) - 1);
    cue.sourceID = Latin1("cam\xe9ra / 1");
    cue.identifier = Latin1("cue#1");
    cue.originalStartTime = Latin1("00:00:01.500");
    cue.settings = Latin1("align:start");
    cue.presentationTime = MediaTime(1500, 1000, MediaTime::Valid);
    cue.duration = MediaTime(250, 1000, MediaTime::Valid);
    Require(Same(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(&cue),
        u"{\"text\":\"<caf\u00e9> \\\"hi\\\"\\n\",\"sourceId\":\"cam%C3%A9ra%20%2F%201\",\"id\":\"cue%231\","
        u"\"originalStartTime\":\"00%3A00%3A01.500\",\"settings\":\"align%3Astart\",\"presentationTime\":1.5,\"duration\":0.25}"));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(nullptr); }));
    const void* originalData = cue.cueText.impl->data;
    cue.cueText.impl->data = nullptr;
    const bool rejected = ThrowsInvalidArgument([&] { _ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(&cue); });
    cue.cueText.impl->data = originalData;
    Require(rejected);
}

struct GuardedCuePages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedCuePages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedCuePages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }

    unsigned char* Guard() const { return base + pageSize; }
};

template <typename Action>
static bool ThrowsNotImplemented(Action action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

static void WebVTTCueJSONGuestPointers() {
    constexpr const char* kSymbol = "_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv";
    GuardedCuePages pages;

    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(reinterpret_cast<const WebVTTCue*>(0x10))); }, kSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(reinterpret_cast<const WebVTTCue*>(pages.Guard()))); }, kSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(reinterpret_cast<const WebVTTCue*>(pages.Guard() - 32))); }, kSymbol));

    alignas(WebVTTCue) unsigned char rawCue[sizeof(WebVTTCue)]{};
    auto* cuePtr = reinterpret_cast<const WebVTTCue*>(rawCue);
    const auto* badImpl = reinterpret_cast<const NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
    std::memcpy(rawCue + offsetof(WebVTTCue, cueText), &badImpl, sizeof(badImpl));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(cuePtr)); }, kSymbol));

    std::memset(rawCue, 0, sizeof(rawCue));
    std::memcpy(rawCue + offsetof(WebVTTCue, sourceID), &badImpl, sizeof(badImpl));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(cuePtr)); }, kSymbol));

    std::memset(rawCue, 0, sizeof(rawCue));
    NKWebKit::WtfAbi::StringImpl badDataImpl{2, 8, pages.Guard() - 4, 4};
    const auto* badDataPtr = &badDataImpl;
    std::memcpy(rawCue + offsetof(WebVTTCue, settings), &badDataPtr, sizeof(badDataPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(cuePtr)); }, kSymbol));

    std::memset(rawCue, 0, sizeof(rawCue));
    NKWebKit::WtfAbi::StringImpl nullDataImpl{2, 4, nullptr, 4};
    const auto* nullDataPtr = &nullDataImpl;
    std::memcpy(rawCue + offsetof(WebVTTCue, identifier), &nullDataPtr, sizeof(nullDataPtr));
    Require(ThrowsInvalidArgument([&] { static_cast<void>(_ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(cuePtr)); }));
}

int main() {
    BoxAndFullBox();
    TrackEncryptionBoxFields();
    ProtectionSystemSpecificHeaderFields();
    WebVTTCueFields();
    WebVTTCueJSON();
    WebVTTCueJSONGuestPointers();
    std::puts("IsoBoxAccessors passed");
    return 0;
}

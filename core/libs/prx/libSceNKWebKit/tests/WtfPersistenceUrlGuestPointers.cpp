#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WtfAbi::Decoder;
using NKWebKit::WtfAbi::Encoder;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF persistence and URL guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

constexpr std::size_t pageBytes = 4096;

static unsigned char* MapPages(std::size_t pages) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, pages * pageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, pages * pageBytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void Protect(void* page, std::size_t pages, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, pages * pageBytes, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, pages * pageBytes, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(void* page, std::size_t pages) {
#ifdef _WIN32
    static_cast<void>(pages);
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pages * pageBytes) == 0);
#endif
}

static void EncodeValues(Encoder* encoder, const String& text) {
    _ZN3WTF11Persistence7EncoderlsEb(encoder, true);
    _ZN3WTF11Persistence7EncoderlsEh(encoder, 0xAB);
    _ZN3WTF11Persistence7EncoderlsEs(encoder, -2);
    _ZN3WTF11Persistence7EncoderlsEt(encoder, 0xBEEF);
    _ZN3WTF11Persistence7EncoderlsEi(encoder, -70000);
    _ZN3WTF11Persistence7EncoderlsEj(encoder, 0xDEADBEEFU);
    _ZN3WTF11Persistence7EncoderlsEl(encoder, -1234567890123LL);
    _ZN3WTF11Persistence7EncoderlsEm(encoder, 0x0123456789ABCDEFULL);
    _ZN3WTF11Persistence7EncoderlsEf(encoder, 1.25f);
    _ZN3WTF11Persistence7EncoderlsEd(encoder, 2.5);
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*encoder, text);
    _ZN3WTF11Persistence7Encoder14encodeChecksumEv(encoder);
}

static void TestEncoder() {
    auto* unreadable = reinterpret_cast<Encoder*>(0x10);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence7EncoderC1Ev(unreadable); return 0; }, "_ZN3WTF11Persistence7EncoderC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence7EncoderD1Ev(unreadable); return 0; }, "_ZN3WTF11Persistence7EncoderD1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence7Encoder14encodeChecksumEv(unreadable); return 0; },
        "_ZN3WTF11Persistence7Encoder14encodeChecksumEv"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEb(unreadable, true); }, "_ZN3WTF11Persistence7EncoderlsEb"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEh(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEh"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEs(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEs"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEt(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEt"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEi(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEi"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEj(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEj"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEl(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEl"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEm(unreadable, 1); }, "_ZN3WTF11Persistence7EncoderlsEm"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEf(unreadable, 1.0f); }, "_ZN3WTF11Persistence7EncoderlsEf"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEd(unreadable, 1.0); }, "_ZN3WTF11Persistence7EncoderlsEd"));
    String text = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("text"), 4);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*unreadable, String()); return 0; },
        "_ZN3WTF11Persistence7EncoderlsEj"));

    constexpr std::size_t encoderPages = (sizeof(Encoder) + pageBytes - 1) / pageBytes;
    unsigned char* readOnlyPages = MapPages(encoderPages);
    auto* readOnly = reinterpret_cast<Encoder*>(readOnlyPages);
    _ZN3WTF11Persistence7EncoderC1Ev(readOnly);
    _ZN3WTF11Persistence7EncoderlsEj(readOnly, 7);
    std::vector<unsigned char> readOnlyBytes(readOnlyPages, readOnlyPages + sizeof(Encoder));
    Protect(readOnlyPages, encoderPages, true);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence7EncoderC2Ev(readOnly); return 0; }, "_ZN3WTF11Persistence7EncoderC1Ev"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF11Persistence7EncoderlsEm(readOnly, 1); }, "_ZN3WTF11Persistence7EncoderlsEm"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence7Encoder14encodeChecksumEv(readOnly); return 0; },
        "_ZN3WTF11Persistence7Encoder14encodeChecksumEv"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*readOnly, text); return 0; },
        "_ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_"));
    Require(std::memcmp(readOnlyPages, readOnlyBytes.data(), sizeof(Encoder)) == 0);
    _ZN3WTF11Persistence7EncoderD1Ev(readOnly);
    UnmapPages(readOnlyPages, encoderPages);

    std::string longText(5000, 'q');
    String large = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(longText.data()), static_cast<unsigned>(longText.size()));
    for (const String* value : {&text, &large}) {
        auto* host = new Encoder;
        _ZN3WTF11Persistence7EncoderC1Ev(host);
        EncodeValues(host, *value);
        unsigned char* guestPages = MapPages(encoderPages);
        auto* guest = reinterpret_cast<Encoder*>(guestPages);
        _ZN3WTF11Persistence7EncoderC1Ev(guest);
        Require(guest->buffer == guest->inlineBuffer && guest->capacity == 4096 && guest->size == 0);
        EncodeValues(guest, *value);
        Require(guest->size == host->size && std::memcmp(guest->buffer, host->buffer, host->size) == 0);
        Require(std::memcmp(&guest->sha1, &host->sha1, sizeof(host->sha1)) == 0);
        Require((value == &large) == (guest->buffer != guest->inlineBuffer));

        _ZN3WTF11Persistence7EncoderD1Ev(guest);
        _ZN3WTF11Persistence7EncoderD1Ev(host);
        delete host;
        UnmapPages(guestPages, encoderPages);
    }
}

constexpr const char* encodeStringName = "_ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_";
constexpr const char* encodeCStringName = "_ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_";

template <typename Encode>
static void TestFailedEncodeKeepsEncoder(const String* prefix, Encode failingEncode, const char* name) {
    constexpr std::size_t encoderPages = (sizeof(Encoder) + pageBytes - 1) / pageBytes;
    auto* reference = new Encoder;
    _ZN3WTF11Persistence7EncoderC1Ev(reference);
    unsigned char* guestPages = MapPages(encoderPages);
    auto* guest = reinterpret_cast<Encoder*>(guestPages);
    _ZN3WTF11Persistence7EncoderC1Ev(guest);
    if (prefix) {
        _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*reference, *prefix);
        _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*guest, *prefix);
    }
    std::vector<unsigned char> encoderBefore(guestPages, guestPages + sizeof(Encoder));
    std::vector<unsigned char> bufferBefore(guest->buffer, guest->buffer + guest->capacity);

    Require(FaultsAsNotImplemented([&] { failingEncode(*guest); return 0; }, name));
    Require(std::memcmp(guestPages, encoderBefore.data(), sizeof(Encoder)) == 0);
    Require(std::memcmp(guest->buffer, bufferBefore.data(), bufferBefore.size()) == 0);

    String followUp = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("after"), 5);
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*guest, followUp);
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(*reference, followUp);
    _ZN3WTF11Persistence7EncoderlsEj(guest, 0x01020304U);
    _ZN3WTF11Persistence7EncoderlsEj(reference, 0x01020304U);
    Require(guest->size == reference->size && std::memcmp(guest->buffer, reference->buffer, reference->size) == 0);
    Require(std::memcmp(&guest->sha1, &reference->sha1, sizeof(reference->sha1)) == 0);

    _ZN3WTF11Persistence7EncoderD1Ev(guest);
    _ZN3WTF11Persistence7EncoderD1Ev(reference);
    delete reference;
    UnmapPages(guestPages, encoderPages);
}

static void TestFailedStringPayload(const String* prefix, unsigned failingLength, bool is8Bit) {
    unsigned char* unreadableData = MapPages(1);
    Protect(unreadableData, 1, false);
    NKWebKit::WtfAbi::StringImpl unreadableImpl{2, failingLength, unreadableData, is8Bit ? 4U : 0U};
    String unreadableText(&unreadableImpl);
    TestFailedEncodeKeepsEncoder(prefix, [&](Encoder& encoder) {
        _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(encoder, unreadableText);
    }, encodeStringName);
    unreadableText.impl = nullptr;
    UnmapPages(unreadableData, 1);
}

static void TestFailedStringImpl(const String* prefix) {
    unsigned char* unreadableImpl = MapPages(1);
    Protect(unreadableImpl, 1, false);
    String unreadableText(reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(unreadableImpl));
    TestFailedEncodeKeepsEncoder(prefix, [&](Encoder& encoder) {
        _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(encoder, unreadableText);
    }, encodeStringName);
    unreadableText.impl = nullptr;
    UnmapPages(unreadableImpl, 1);
}

static void TestFailedCStringPayload(const String* prefix, std::size_t failingLength) {
    unsigned char* pages = MapPages(2);
    auto* header = reinterpret_cast<NKWebKit::WtfAbi::CStringBuffer*>(pages + pageBytes - sizeof(NKWebKit::WtfAbi::CStringBuffer));
    header->refCount = 2;
    header->length = failingLength;
    Protect(pages + pageBytes, 1, false);
    NKWebKit::WtfAbi::CString unreadableText(header);
    TestFailedEncodeKeepsEncoder(prefix, [&](Encoder& encoder) {
        _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(encoder, unreadableText);
    }, encodeCStringName);
    unreadableText.buffer = nullptr;
    UnmapPages(pages, 2);
}

static void TestFailedCStringBuffer(const String* prefix) {
    unsigned char* unreadableBuffer = MapPages(1);
    Protect(unreadableBuffer, 1, false);
    NKWebKit::WtfAbi::CString unreadableText(reinterpret_cast<NKWebKit::WtfAbi::CStringBuffer*>(unreadableBuffer));
    TestFailedEncodeKeepsEncoder(prefix, [&](Encoder& encoder) {
        _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(encoder, unreadableText);
    }, encodeCStringName);
    unreadableText.buffer = nullptr;
    UnmapPages(unreadableBuffer, 1);
}

static void TestFailedEncodesKeepEncoder() {
    std::string longText(5000, 'q');
    String large = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(longText.data()), static_cast<unsigned>(longText.size()));
    String small = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("text"), 4);
    for (const String* prefix : std::initializer_list<const String*>{nullptr, &small, &large}) {
        TestFailedStringPayload(prefix, 5000, true);
        TestFailedStringPayload(prefix, 3, true);
        TestFailedStringPayload(prefix, 3, false);
        TestFailedStringImpl(prefix);
        TestFailedCStringPayload(prefix, 5000);
        TestFailedCStringPayload(prefix, 3);
        TestFailedCStringBuffer(prefix);
    }
}

static void TestDecoder() {
    auto* unreadable = reinterpret_cast<Decoder*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(unreadable, 1); },
        "_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF11Persistence7Decoder14verifyChecksumEv(unreadable); },
        "_ZN3WTF11Persistence7Decoder14verifyChecksumEv"));

    Encoder encoder;
    _ZN3WTF11Persistence7EncoderC1Ev(&encoder);
    _ZN3WTF11Persistence7EncoderlsEj(&encoder, 42);
    NKWebKit::WtfAbi::SHA1 payloadHash = encoder.sha1;
    unsigned payloadSize = encoder.size;
    _ZN3WTF11Persistence7Encoder14encodeChecksumEv(&encoder);

    unsigned char* dataPages = MapPages(2);
    Protect(dataPages + pageBytes, 1, false);
    unsigned char* data = dataPages + pageBytes - encoder.size;
    std::memcpy(data, encoder.buffer, encoder.size);
    auto* dangling = reinterpret_cast<const std::uint8_t*>(0x1000);
    Decoder unreadableData{dangling, 64, dangling, payloadHash};
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF11Persistence7Decoder14verifyChecksumEv(&unreadableData); },
        "_ZN3WTF11Persistence7Decoder14verifyChecksumEv"));
    Require(unreadableData.bufferPosition == dangling);

    unsigned char* decoderPage = MapPages(1);
    auto* readOnly = reinterpret_cast<Decoder*>(decoderPage);
    *readOnly = Decoder{data, encoder.size, data + payloadSize, payloadHash};
    std::vector<unsigned char> decoderBytes(decoderPage, decoderPage + sizeof(Decoder));
    Protect(decoderPage, 1, true);
    Require(_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(readOnly, 20));
    Require(!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(readOnly, 21));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF11Persistence7Decoder14verifyChecksumEv(readOnly); },
        "_ZN3WTF11Persistence7Decoder14verifyChecksumEv"));
    Require(std::memcmp(decoderPage, decoderBytes.data(), sizeof(Decoder)) == 0);
    UnmapPages(decoderPage, 1);

    Decoder overflowing{data, SIZE_MAX, data, payloadHash};
    Require(!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(&overflowing, 0));
    Require(!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(&overflowing, 1));
    Decoder beforeData{data, encoder.size, data - 1, payloadHash};
    Require(!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(&beforeData, 1));

    Decoder decoder{data, encoder.size, data + payloadSize, payloadHash};
    Require(_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&decoder));
    Require(decoder.bufferPosition == data + encoder.size);
    Require(!_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&decoder));

    UnmapPages(dataPages, 2);
    _ZN3WTF11Persistence7EncoderD1Ev(&encoder);
}

static void TestURL() {
    String text = _ZN3WTF10StringImpl6createEPKhj(
        reinterpret_cast<const unsigned char*>("https://alice:secret@api.example.com:8443/v1/data?key=val#section"), 65);
    const URL fields{text.impl, 1U | 2U | (5U << 3) | (5U << 6), 8, 13, 20, 36, 45, 49, 57};
    auto* unreadable = reinterpret_cast<URL*>(0x10);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(unreadable); return 0; },
        "_ZN3WTF3URL32removeQueryAndFragmentIdentifierEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL11hostAndPortEv(unreadable); }, "_ZNK3WTF3URL11hostAndPortEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(unreadable, StringView{"example.com", 11, true}); },
        "_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL19protocolHostAndPortEv(unreadable); }, "_ZNK3WTF3URL19protocolHostAndPortEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL4userEv(unreadable); }, "_ZNK3WTF3URL4userEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL8passwordEv(unreadable); }, "_ZNK3WTF3URL8passwordEv"));
    URL url = fields;
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL10protocolIsEPKc(&url, reinterpret_cast<const char*>(0x10)); },
        "_ZNK3WTF3URL10protocolIsEPKc"));

    unsigned char* stringPages = MapPages(2);
    Protect(stringPages + pageBytes, 1, false);
    std::memcpy(stringPages + pageBytes - 5, "https", 5);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL10protocolIsEPKc(&url, reinterpret_cast<const char*>(stringPages + pageBytes - 5)); },
        "_ZNK3WTF3URL10protocolIsEPKc"));
    std::memcpy(stringPages + pageBytes - 6, "https", 6);
    Require(_ZNK3WTF3URL10protocolIsEPKc(&url, reinterpret_cast<const char*>(stringPages + pageBytes - 6)));
    UnmapPages(stringPages, 2);
    unsigned char* crossing = MapPages(2);
    std::memcpy(crossing + pageBytes - 3, "https", 6);
    Require(_ZNK3WTF3URL10protocolIsEPKc(&url, reinterpret_cast<const char*>(crossing + pageBytes - 3)));
    std::memcpy(crossing + pageBytes - 2, "http", 5);
    Require(!_ZNK3WTF3URL10protocolIsEPKc(&url, reinterpret_cast<const char*>(crossing + pageBytes - 2)));
    UnmapPages(crossing, 2);

    unsigned char* urlPages = MapPages(2);
    Protect(urlPages, 1, false);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF3URL10protocolIsEPKc(reinterpret_cast<const URL*>(urlPages), "https"); },
        "_ZNK3WTF3URL10protocolIsEPKc"));
    UnmapPages(urlPages, 2);
    urlPages = MapPages(2);
    std::memcpy(urlPages + pageBytes - sizeof(URL), &fields, sizeof(URL));
    Require(_ZNK3WTF3URL10protocolIsEPKc(reinterpret_cast<const URL*>(urlPages + pageBytes - sizeof(URL)), "https"));
    Require(!_ZNK3WTF3URL10protocolIsEPKc(reinterpret_cast<const URL*>(urlPages + pageBytes - sizeof(URL)), "http"));
    std::memcpy(urlPages + pageBytes - sizeof(URL) / 2, &fields, sizeof(URL) / 2);
    Protect(urlPages + pageBytes, 1, false);
    Require(FaultsAsNotImplemented(
        [&] { return _ZNK3WTF3URL10protocolIsEPKc(reinterpret_cast<const URL*>(urlPages + pageBytes - sizeof(URL) / 2), "https"); },
        "_ZNK3WTF3URL10protocolIsEPKc"));
    UnmapPages(urlPages, 2);

    unsigned char* page = MapPages(1);
    auto* readOnly = reinterpret_cast<URL*>(page);
    *readOnly = fields;
    Protect(page, 1, true);
    unsigned refCount = text.impl->refCount;
    Require(FaultsAsNotImplemented([&] { _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(readOnly); return 0; },
        "_ZN3WTF3URL32removeQueryAndFragmentIdentifierEv"));
    Require(readOnly->impl == text.impl && readOnly->queryEnd == 57 && text.impl->refCount == refCount);
    Require(_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(readOnly, StringView{"example.com", 11, true}));
    String hostAndPort = _ZNK3WTF3URL11hostAndPortEv(readOnly);
    Require(hostAndPort.impl && hostAndPort.impl->length == 20 && std::memcmp(hostAndPort.impl->data, "api.example.com:8443", 20) == 0);
    String origin = _ZNK3WTF3URL19protocolHostAndPortEv(readOnly);
    Require(origin.impl && origin.impl->length == 28 && std::memcmp(origin.impl->data, "https://api.example.com:8443", 28) == 0);
    String user = _ZNK3WTF3URL4userEv(readOnly);
    Require(user.impl && user.impl->length == 5 && std::memcmp(user.impl->data, "alice", 5) == 0);
    String password = _ZNK3WTF3URL8passwordEv(readOnly);
    Require(password.impl && password.impl->length == 6 && std::memcmp(password.impl->data, "secret", 6) == 0);
    UnmapPages(page, 1);

    unsigned char* writable = MapPages(1);
    auto* guest = reinterpret_cast<URL*>(writable);
    String owned = _ZN3WTF10StringImpl6createEPKhj(
        reinterpret_cast<const unsigned char*>("https://alice:secret@api.example.com:8443/v1/data?key=val#section"), 65);
    *guest = URL{owned.impl, fields.bits, 8, 13, 20, 36, 45, 49, 57};
    owned.impl = nullptr;
    _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(guest);
    Require(guest->impl && guest->impl->length == 49 && guest->queryEnd == 49 && guest->pathEnd == 49);
    Require(std::memcmp(guest->impl->data, "https://alice:secret@api.example.com:8443/v1/data", 49) == 0);
    String release(guest->impl);
    UnmapPages(writable, 1);
}

int main() {
    TestEncoder();
    TestFailedEncodesKeepEncoder();
    TestDecoder();
    TestURL();
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/IsoBoxParsingExports.hpp"
#include "prx/libSceNKWebKit/include/IsoProtectionBoxExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::IsoAbi::Box;
using NKWebKit::IsoAbi::ByteVector;
using NKWebKit::IsoAbi::FullBox;
using NKWebKit::IsoParse::DataView;

using NKWebKit::IsoAbi::TrackEncryptionBox;
using NKWebKit::IsoAbi::OriginalFormatBox;
using NKWebKit::IsoAbi::SchemeTypeBox;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ISO protection box check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

namespace IsoDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
    void MixBytes(const std::uint8_t* data, std::size_t size) {
        Mix(size);
        for (std::size_t i = 0; i < size; ++i) Mix(data[i]);
    }
};

enum BoxKind : int {
    KindBox,
    KindFullBox,
    KindOriginalFormat,
    KindSchemeType,
    KindTrackEncryption,
    KindSchemeInformation,
    KindProtectionSchemeInfo,
    KindProtectionSystemSpecificHeader,
    KindCount,
};

enum ViewMode : int {
    ViewPlain,
    ViewResizableFixed,
    ViewResizableAuto,
    ViewModeCount,
};

struct Random {
    std::uint32_t state;
    explicit Random(std::uint32_t seed) : state(seed ? seed : 1) { }
    int operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<int>(state % bound);
    }
};

using Bytes = std::vector<std::uint8_t>;

inline void Put32(Bytes& out, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

inline void Put64(Bytes& out, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

inline std::uint32_t Type(const char* t) {
    return (std::uint32_t(std::uint8_t(t[0])) << 24) | (std::uint32_t(std::uint8_t(t[1])) << 16) | (std::uint32_t(std::uint8_t(t[2])) << 8) | std::uint8_t(t[3]);
}

inline void RandomBytes(Random& random, Bytes& out, int count) {
    for (int i = 0; i < count; ++i) out.push_back(static_cast<std::uint8_t>(random(256)));
}

inline bool HasCopy(int kind) {
    return kind != KindSchemeInformation && kind != KindProtectionSchemeInfo;
}

inline bool HasAssign(int kind) {
    return kind == KindFullBox || kind == KindOriginalFormat || kind == KindTrackEncryption;
}

inline Bytes Wrap(Random& random, std::uint32_t type, const Bytes& body) {
    Bytes out;
    int sizeMode = random(12);
    std::uint64_t size = 8 + body.size();
    if (sizeMode == 0) {
        Put32(out, 1);
        Put32(out, type);
        std::uint64_t large = 16 + body.size();
        int largeMode = random(4);
        Put64(out, largeMode == 0 ? large + random(5) : largeMode == 1 ? 0 : large);
    } else {
        std::uint32_t written = static_cast<std::uint32_t>(size);
        if (sizeMode == 1) written = 0;
        else if (sizeMode == 2) written = static_cast<std::uint32_t>(size) + 1 + random(6);
        else if (sizeMode == 3 && size > 9) written = static_cast<std::uint32_t>(size) - 1 - random(2);
        else if (sizeMode == 4) written = 0xffffff00u + random(256);
        Put32(out, written);
        Put32(out, type);
    }
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

inline void PutVersionAndFlags(Random& random, Bytes& body, int version) {
    Put32(body, (std::uint32_t(version) << 24) | (random(4) ? 0 : random(0x1000000)));
}

inline Bytes MakeTrackEncryption(Random& random) {
    Bytes body;
    int version = random(3) ? random(2) : random(256);
    PutVersionAndFlags(random, body, version);
    body.push_back(0);
    body.push_back(static_cast<std::uint8_t>(random(256)));
    int isProtected = random(4) ? 1 : random(3);
    body.push_back(static_cast<std::uint8_t>(isProtected));
    int ivSize = random(3) ? 0 : (random(2) ? 8 : 16);
    body.push_back(static_cast<std::uint8_t>(ivSize));
    RandomBytes(random, body, random(6) ? 16 : random(16));
    if (isProtected == 1 && !ivSize) {
        int constant = random(5) ? 8 + 8 * random(2) : random(256);
        body.push_back(static_cast<std::uint8_t>(constant));
        RandomBytes(random, body, random(4) ? (constant < 128 ? constant : 4) : random(20));
    }
    return Wrap(random, Type("tenc"), body);
}

inline Bytes MakeProtectionSystemSpecificHeader(Random& random) {
    Bytes body;
    int version = random(3) ? random(2) : random(256);
    PutVersionAndFlags(random, body, version);
    RandomBytes(random, body, random(8) ? 16 : random(16));
    if (version) {
        int count = random(4);
        std::uint32_t written = random(10) ? count : (random(2) ? 0x10000001u : 0xffffffffu);
        Put32(body, written);
        RandomBytes(random, body, 16 * count - (random(6) ? 0 : random(16)));
    }
    int dataSize = random(3) ? random(40) : 0;
    Put32(body, random(10) ? dataSize : dataSize + 1 + random(100));
    RandomBytes(random, body, dataSize);
    return Wrap(random, Type("pssh"), body);
}

inline Bytes MakeBox(Random& random, int depth);

inline Bytes MakeSchemeInformation(Random& random, int depth) {
    Bytes body;
    int content = random(4);
    if (content == 0) body = MakeTrackEncryption(random);
    else if (content == 1) body = MakeBox(random, depth + 1);
    else if (content == 2) RandomBytes(random, body, random(12));
    else {
        body = MakeTrackEncryption(random);
        Bytes extra = MakeBox(random, depth + 1);
        body.insert(body.end(), extra.begin(), extra.end());
    }
    return Wrap(random, Type("schi"), body);
}

inline Bytes MakeProtectionSchemeInfo(Random& random, int depth) {
    Bytes body;
    Bytes format;
    Put32(format, random(2) ? Type("avc1") : random(0x7fffffff));
    Bytes frma = Wrap(random, Type("frma"), format);
    if (random(8)) body.insert(body.end(), frma.begin(), frma.end());
    if (random(3)) {
        Bytes schm;
        PutVersionAndFlags(random, schm, random(2));
        Put32(schm, random(2) ? Type("cenc") : Type("cbcs"));
        Put32(schm, 0x10000);
        Bytes box = Wrap(random, Type("schm"), schm);
        body.insert(body.end(), box.begin(), box.end());
    }
    if (random(3)) {
        Bytes schi = MakeSchemeInformation(random, depth + 1);
        body.insert(body.end(), schi.begin(), schi.end());
    }
    if (!random(6)) {
        Bytes extra = MakeBox(random, depth + 1);
        body.insert(body.end(), extra.begin(), extra.end());
    }
    return Wrap(random, Type("sinf"), body);
}

inline Bytes MakeBox(Random& random, int depth) {
    if (depth > 2) {
        Bytes body;
        RandomBytes(random, body, random(8));
        return Wrap(random, Type("free"), body);
    }
    switch (random(9)) {
    case 0: {
        Bytes body;
        Put32(body, random(2) ? Type("encv") : random(0x7fffffff));
        return Wrap(random, Type("frma"), body);
    }
    case 1: {
        Bytes body;
        PutVersionAndFlags(random, body, random(3));
        Put32(body, Type("cenc"));
        Put32(body, random(0x7fffffff));
        return Wrap(random, Type("schm"), body);
    }
    case 2: return MakeTrackEncryption(random);
    case 3: return MakeSchemeInformation(random, depth);
    case 4: return MakeProtectionSchemeInfo(random, depth);
    case 5: return MakeProtectionSystemSpecificHeader(random);
    case 6: {
        Bytes body;
        RandomBytes(random, body, random(20));
        return Wrap(random, Type("mdat"), body);
    }
    default: {
        Bytes body;
        RandomBytes(random, body, random(24));
        return Wrap(random, random(2) ? Type("free") : random(0x7fffffff), body);
    }
    }
}

inline Bytes MakeStream(Random& random) {
    Bytes out;
    if (!random(10)) {
        RandomBytes(random, out, random(40));
        return out;
    }
    int count = 1 + random(3);
    for (int i = 0; i < count; ++i) {
        Bytes box = MakeBox(random, 0);
        out.insert(out.end(), box.begin(), box.end());
    }
    if (!random(4) && !out.empty()) out.resize(out.size() - 1 - random(static_cast<std::uint32_t>(std::min<std::size_t>(out.size(), 12))));
    return out;
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count, int kindCount, bool peekSystemID) {
    Hash hash;
    Random random(seed);
    for (int i = 0; i < count; ++i) {
        Bytes payload = MakeStream(random);
        int prefix = random(3) ? 0 : random(9);
        Bytes buffer(prefix, 0xEE);
        buffer.insert(buffer.end(), payload.begin(), payload.end());
        int suffix = random(4) ? 0 : random(9);
        buffer.insert(buffer.end(), suffix, 0xDD);
        int mode = random(6) ? ViewPlain : 1 + random(2);
        std::size_t viewLength = buffer.size() - prefix - (random(3) ? suffix : 0);
        adapter.MakeView(buffer, prefix, viewLength, mode);
        hash.Mix(adapter.ViewByteLength());

        int kind = random(static_cast<std::uint32_t>(kindCount));
        adapter.Create(kind, 0);
        bool trusted = true;
        int operations = 1 + random(3);
        for (int op = 0; op < operations; ++op) {
            int which = random(5);
            unsigned offset = random(4) ? 0 : static_cast<unsigned>(random(static_cast<std::uint32_t>(buffer.size() + 4)));
            bool result = false;
            if (which == 0) {
                result = adapter.Read(0);
            } else if (which == 1) {
                result = adapter.ReadAt(0, offset);
                hash.Mix(offset);
            } else if (which == 2 && kind != KindBox && kind != KindFullBox) {
                result = adapter.Parse(0, offset);
                hash.Mix(offset);
            } else {
                result = adapter.Parse(0, offset);
                hash.Mix(offset);
            }
            hash.Mix(result);
            trusted = trusted && result;
            adapter.MixBox(0, hash, trusted);
        }
        if (HasCopy(kind) && random(3) == 0) {
            adapter.CopyConstruct(1, 0);
            adapter.MixBox(1, hash, trusted);
            adapter.Destroy(1);
        }
        if (HasAssign(kind) && random(3) == 0) {
            adapter.Create(kind, 1);
            if (random(2)) {
                unsigned other = 0;
                hash.Mix(adapter.ReadAt(1, other));
            }
            adapter.Assign(1, 0);
            adapter.MixBox(1, hash, trusted);
            adapter.Destroy(1);
        }
        adapter.Destroy(0);
        for (int peek = 0; peek < 2; ++peek) {
            unsigned offset = random(3) ? 0 : static_cast<unsigned>(random(static_cast<std::uint32_t>(buffer.size() + 4)));
            adapter.PeekBox(offset, hash);
            if (peekSystemID) adapter.PeekSystemID(offset, hash);
        }
    }
    return hash.value;
}

} // namespace IsoDriver


struct FakeBuffer {
    alignas(16) unsigned char object[96] {};
    std::vector<std::uint8_t> storage;
};

void Store(void* base, std::size_t offset, const void* value, std::size_t size) {
    std::memcpy(static_cast<unsigned char*>(base) + offset, value, size);
}

void MixVector(IsoDriver::Hash& hash, const ByteVector& v, bool trusted = true) {
    hash.Mix(v.capacity);
    if (trusted)
        hash.MixBytes(v.buffer, v.size);
    else
        hash.Mix(v.size);
}

void MixOptional(IsoDriver::Hash& hash, const NKWebKit::IsoAbi::OptionalInt8& o) {
    hash.Mix(o.engaged);
    hash.Mix(o.engaged ? static_cast<std::uint8_t>(o.value) : 0);
}

void MixBase(IsoDriver::Hash& hash, const Box& box) {
    hash.Mix(box.size);
    hash.Mix(box.boxType);
    MixVector(hash, box.extendedType);
}

void MixFull(IsoDriver::Hash& hash, const FullBox& box) {
    MixBase(hash, box.box);
    hash.Mix(box.version);
    hash.Mix(box.flags);
}

void MixAny(IsoDriver::Hash& hash, const void* box, int kind, bool trusted) {
    using namespace IsoDriver;
    hash.Mix(0xB0 + kind);
    if (!box) {
        hash.Mix(0xDEAD);
        return;
    }
    switch (kind) {
    case KindBox: MixBase(hash, *static_cast<const Box*>(box)); break;
    case KindFullBox: MixFull(hash, *static_cast<const FullBox*>(box)); break;
    case KindOriginalFormat: {
        auto& b = *static_cast<const OriginalFormatBox*>(box);
        MixBase(hash, b.box);
        hash.Mix(b.dataFormat);
        break;
    }
    case KindSchemeType: {
        auto& b = *static_cast<const SchemeTypeBox*>(box);
        MixFull(hash, b.fullBox);
        hash.Mix(b.schemeType);
        hash.Mix(b.schemeVersion);
        break;
    }
    case KindTrackEncryption: {
        auto& b = *static_cast<const TrackEncryptionBox*>(box);
        MixFull(hash, b.fullBox);
        MixOptional(hash, b.defaultCryptByteBlock);
        MixOptional(hash, b.defaultSkipByteBlock);
        hash.Mix(static_cast<std::uint8_t>(b.defaultIsProtected));
        hash.Mix(static_cast<std::uint8_t>(b.defaultPerSampleIVSize));
        MixVector(hash, b.defaultKID, trusted);
        MixVector(hash, b.defaultConstantIV);
        break;
    }
    }
}

struct VtableView {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    bool (APS5_VABI* parse)(void*, DataView*, unsigned*);
};

struct Guest {
    FakeBuffer buffer;
    DataView view {};
    void* slot[2] = {};
    int kinds[2] = {};

    void MakeView(const IsoDriver::Bytes& bytes, std::size_t byteOffset, std::size_t length, int mode) {
        buffer.storage.assign(bytes.size() + 16, 0);
        std::copy(bytes.begin(), bytes.end(), buffer.storage.begin());
        std::memset(buffer.object, 0, sizeof(buffer.object));
        void* data = buffer.storage.data();
        std::size_t size = bytes.size();
        Store(buffer.object, 16, &data, sizeof(data));
        Store(buffer.object, 48, &size, sizeof(size));
        if (mode != IsoDriver::ViewPlain) {
            std::size_t maxByteLength = size + 16;
            bool hasMax = true;
            Store(buffer.object, 56, &maxByteLength, sizeof(maxByteLength));
            Store(buffer.object, 64, &hasMax, sizeof(hasMax));
        }
        view = {};
        view.refCount = 1;
        view.type = 12;
        view.isDetachable = true;
        view.flags = mode == IsoDriver::ViewPlain ? 0 : mode == IsoDriver::ViewResizableFixed ? 1 : 5;
        view.byteOffset = byteOffset;
        view.byteLength = mode == IsoDriver::ViewResizableAuto ? 0 : length;
        view.baseAddress = buffer.storage.data() + byteOffset;
        view.buffer = buffer.object;
    }
    std::uint64_t ViewByteLength() { return NKWebKit::IsoParse::ViewByteLength(view); }
    static std::size_t SizeOf(int kind) {
        static const std::size_t sizes[] = {40, 48, 48, 56, 88, 48, 104, 96};
        return sizes[kind];
    }
    static void Construct(void* memory, int kind) {
        switch (kind) {
        case IsoDriver::KindBox: _ZN7WebCore6ISOBoxC1Ev(static_cast<Box*>(memory)); break;
        case IsoDriver::KindFullBox: _ZN7WebCore10ISOFullBoxC1Ev(static_cast<FullBox*>(memory)); break;
        case IsoDriver::KindOriginalFormat: _ZN7WebCore20ISOOriginalFormatBoxC1Ev(static_cast<OriginalFormatBox*>(memory)); break;
        case IsoDriver::KindSchemeType: _ZN7WebCore16ISOSchemeTypeBoxC1Ev(static_cast<SchemeTypeBox*>(memory)); break;
        case IsoDriver::KindTrackEncryption: _ZN7WebCore21ISOTrackEncryptionBoxC1Ev(static_cast<TrackEncryptionBox*>(memory)); break;
        default: std::abort();
        }
    }
    void Create(int kind, int s) {
        void* memory = _ZN7WebCore6ISOBoxnwEm(SizeOf(kind));
        std::memset(memory, 0xa5, SizeOf(kind));
        Construct(memory, kind);
        slot[s] = memory;
        kinds[s] = kind;
    }
    static const VtableView& Vtable(void* box) { return **static_cast<const VtableView* const*>(box); }
    bool Read(int s) { return _ZN7WebCore6ISOBox4readERN3JSC8DataViewE(static_cast<Box*>(slot[s]), &view); }
    bool ReadAt(int s, unsigned& offset) { return _ZN7WebCore6ISOBox4readERN3JSC8DataViewERj(static_cast<Box*>(slot[s]), &view, &offset); }
    bool Parse(int s, unsigned& offset) { return Vtable(slot[s]).parse(slot[s], &view, &offset); }
    void MixBox(int s, IsoDriver::Hash& hash, bool trusted) { MixAny(hash, slot[s], kinds[s], trusted); }
    void CopyConstruct(int to, int from) {
        int kind = kinds[from];
        void* memory = _ZN7WebCore6ISOBoxnwEm(SizeOf(kind));
        std::memset(memory, 0x5a, SizeOf(kind));
        switch (kind) {
        case IsoDriver::KindBox: _ZN7WebCore6ISOBoxC1ERKS0_(static_cast<Box*>(memory), static_cast<Box*>(slot[from])); break;
        case IsoDriver::KindFullBox: _ZN7WebCore10ISOFullBoxC1ERKS0_(static_cast<FullBox*>(memory), static_cast<FullBox*>(slot[from])); break;
        case IsoDriver::KindOriginalFormat: _ZN7WebCore20ISOOriginalFormatBoxC1ERKS0_(static_cast<OriginalFormatBox*>(memory), static_cast<OriginalFormatBox*>(slot[from])); break;
        case IsoDriver::KindSchemeType: _ZN7WebCore16ISOSchemeTypeBoxC1ERKS0_(static_cast<SchemeTypeBox*>(memory), static_cast<SchemeTypeBox*>(slot[from])); break;
        case IsoDriver::KindTrackEncryption: _ZN7WebCore21ISOTrackEncryptionBoxC1ERKS0_(static_cast<TrackEncryptionBox*>(memory), static_cast<TrackEncryptionBox*>(slot[from])); break;
        default: std::abort();
        }
        slot[to] = memory;
        kinds[to] = kind;
    }
    void Assign(int to, int from) {
        switch (kinds[from]) {
        case IsoDriver::KindFullBox: Require(_ZN7WebCore10ISOFullBoxaSERKS0_(static_cast<FullBox*>(slot[to]), static_cast<FullBox*>(slot[from])) == slot[to]); break;
        case IsoDriver::KindOriginalFormat: Require(_ZN7WebCore20ISOOriginalFormatBoxaSERKS0_(static_cast<OriginalFormatBox*>(slot[to]), static_cast<OriginalFormatBox*>(slot[from])) == slot[to]); break;
        case IsoDriver::KindTrackEncryption: Require(_ZN7WebCore21ISOTrackEncryptionBoxaSERKS0_(static_cast<TrackEncryptionBox*>(slot[to]), static_cast<TrackEncryptionBox*>(slot[from])) == slot[to]); break;
        default: std::abort();
        }
    }
    void Destroy(int s) {
        Vtable(slot[s]).deletingDestructor(slot[s]);
        slot[s] = nullptr;
    }
    void PeekBox(unsigned offset, IsoDriver::Hash& hash) {
        auto r = _ZN7WebCore6ISOBox7peekBoxERN3JSC8DataViewEj(&view, offset);
        hash.Mix(r.engaged);
        if (r.engaged) {
            hash.Mix(r.type);
            hash.Mix(r.size);
        }
    }
    void PeekSystemID(unsigned, IsoDriver::Hash&) {
        std::abort();
    }
};

constexpr std::uint64_t ExpectedHashes[] = {
    0x80c2067e2403c8a0ull,
    0xefe85cf9498388b8ull,
    0xa8d707156c687ea8ull,
    0x659e7c3b78cf4196ull,
    0x13dbda13ec4cd37eull,
    0x9a3f99862e3b058full,
    0xcacf25dd7965d336ull,
    0x10ce5ec335d25dddull,
    0x9b54a31e4da4206bull,
    0x57c8d56c4d9eea8dull,
    0x4cb1bb335543744bull,
    0x9a8238066c62ffc1ull,
    0x88071c645c066a43ull,
    0x9cc4e469de12147cull,
    0xd71a12ca2d580d51ull,
    0x33c3ffc99dc5de06ull,
};

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        Guest guest;
        const std::uint64_t hash = IsoDriver::Run(guest, seed, 400, 5, false);
        if (hash != ExpectedHashes[seed - 1]) std::fprintf(stderr, "seed %u: 0x%016llx\n", seed, static_cast<unsigned long long>(hash));
        Require(hash == ExpectedHashes[seed - 1]);
    }
}

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestDetachedTrackEncryption() {
    Guest guest;
    IsoDriver::Bytes body;
    IsoDriver::Put32(body, 0);
    body.insert(body.end(), {0, 0, 1, 8});
    body.resize(body.size() + 16, 0x42);
    IsoDriver::Bytes bytes;
    IsoDriver::Put32(bytes, static_cast<std::uint32_t>(8 + body.size()));
    IsoDriver::Put32(bytes, IsoDriver::Type("tenc"));
    bytes.insert(bytes.end(), body.begin(), body.end());
    guest.MakeView(bytes, 0, bytes.size(), IsoDriver::ViewPlain);
    guest.Create(IsoDriver::KindTrackEncryption, 0);
    Require(guest.Read(0));
    auto* box = static_cast<TrackEncryptionBox*>(guest.slot[0]);
    Require(box->defaultIsProtected == 1 && box->defaultPerSampleIVSize == 8);
    Require(box->defaultKID.size == 16 && box->defaultKID.buffer[15] == 0x42);
    guest.view.buffer = nullptr;
    Require(!guest.Read(0));
    guest.Destroy(0);
}

void TestNullArguments() {
    DataView view {};
    unsigned offset = 0;
    Require(ThrowsInvalidArgument([] { _ZN7WebCore20ISOOriginalFormatBoxC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore16ISOSchemeTypeBox5parseERN3JSC8DataViewERj(nullptr, &view, &offset); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore21ISOTrackEncryptionBoxaSERKS0_(nullptr, nullptr); }));
}

}  // namespace

int main() {
    TestTranscripts();
    TestDetachedTrackEncryption();
    TestNullArguments();
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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

static void BoxAndFullBox() {
    FullBox box{};
    box.box.size = 0x1'0000'0020ULL;
    box.box.boxType = 0x7363686dU;
    box.box.extendedType = Bytes("uuid-bytes");
    box.version = 1;
    box.flags = 0x00abcdefU;
    Require(_ZNK7WebCore6ISOBox4sizeEv(&box.box) == 0x1'0000'0020ULL);
    Require(_ZNK7WebCore6ISOBox7boxTypeEv(&box.box) == 0x7363686dU);
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
    const auto crypt = _ZNK7WebCore21ISOTrackEncryptionBox21defaultCryptByteBlockEv(&box);
    Require(crypt.engaged && crypt.value == -3);
    Require(!_ZNK7WebCore21ISOTrackEncryptionBox20defaultSkipByteBlockEv(&box).engaged);
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

int main() {
    BoxAndFullBox();
    TrackEncryptionBoxFields();
    ProtectionSystemSpecificHeaderFields();
    WebVTTCueFields();
    std::puts("IsoBoxAccessors passed");
    return 0;
}

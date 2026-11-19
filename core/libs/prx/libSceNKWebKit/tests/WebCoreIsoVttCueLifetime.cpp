#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/IsoWebVttCueExports.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using namespace NKWebKit::IsoAbi;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCoreIsoVttCueLifetime check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Destructor = void(APS5_VABI*)(WebVTTCue*);

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

struct Held {
    String sourceID = Latin1("source");
    String identifier = Latin1("cue-1");
    String originalStartTime = Latin1("00:00:01.000");
    String settings = Latin1("align:start");
    String cueText = Latin1("hello");
};

static WebVTTCue* MakeCue(const Held& held, bool withExtendedType) {
    auto* cue = new (_ZN7WebCore6ISOBoxnwEm(sizeof(WebVTTCue))) WebVTTCue{};
    cue->box.size = 0x40;
    cue->box.boxType = 0x76747463U;
    if (withExtendedType) {
        for (const char* c = "0123456789abcdef"; *c; ++c) cue->box.extendedType.append(static_cast<std::uint8_t>(*c));
    }
    cue->presentationTime = NKWebKit::MediaAbi::MediaTime(3, 1, 1);
    cue->duration = NKWebKit::MediaAbi::MediaTime(2, 1, 1);
    cue->sourceID = held.sourceID;
    cue->identifier = held.identifier;
    cue->originalStartTime = held.originalStartTime;
    cue->settings = held.settings;
    cue->cueText = held.cueText;
    return cue;
}

static bool AllHeldOnce(const Held& held) {
    return held.sourceID.impl->refCount == 2 && held.identifier.impl->refCount == 2 && held.originalStartTime.impl->refCount == 2 &&
        held.settings.impl->refCount == 2 && held.cueText.impl->refCount == 2;
}

static bool AllHeldTwice(const Held& held) {
    return held.sourceID.impl->refCount == 4 && held.identifier.impl->refCount == 4 && held.originalStartTime.impl->refCount == 4 &&
        held.settings.impl->refCount == 4 && held.cueText.impl->refCount == 4;
}

static void BaseDestructorReleasesMembers() {
    Held held;
    auto* cue = MakeCue(held, true);
    Require(AllHeldTwice(held));
    _ZN7WebCore12ISOWebVTTCueD2Ev(cue);
    Require(AllHeldOnce(held));
    _ZN7WebCore6ISOBoxdlEPv(cue);
}

static void CompleteDestructorForwardsToBase() {
    Held held;
    auto* cue = MakeCue(held, false);
    Require(AllHeldTwice(held));
    reinterpret_cast<Destructor>(&_ZN7WebCore12ISOWebVTTCueD1Ev)(cue);
    Require(AllHeldOnce(held));
    _ZN7WebCore6ISOBoxdlEPv(cue);
}

static void DeletingDestructorReleasesAndFrees() {
    Held held;
    auto* cue = MakeCue(held, true);
    Require(AllHeldTwice(held));
    _ZN7WebCore12ISOWebVTTCueD0Ev(cue);
    Require(AllHeldOnce(held));
}

static void NullStringsAreSkipped() {
    auto* cue = new (_ZN7WebCore6ISOBoxnwEm(sizeof(WebVTTCue))) WebVTTCue{};
    Require(!cue->sourceID.impl && !cue->cueText.impl && !cue->box.extendedType.buffer);
    _ZN7WebCore12ISOWebVTTCueD0Ev(cue);
}

static void NullThisThrows() {
    Require(ThrowsInvalidArgument([] { _ZN7WebCore12ISOWebVTTCueD2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore12ISOWebVTTCueD0Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { reinterpret_cast<Destructor>(&_ZN7WebCore12ISOWebVTTCueD1Ev)(nullptr); }));
}

int main() {
    BaseDestructorReleasesMembers();
    CompleteDestructorForwardsToBase();
    DeletingDestructorReleasesAndFrees();
    NullStringsAreSkipped();
    NullThisThrows();
    std::puts("WebCoreIsoVttCueLifetime tests passed");
    return 0;
}

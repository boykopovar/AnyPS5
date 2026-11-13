#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PAL::KillRing check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using StringMember = void (APS5_VABI *)(PAL::KillRing*, const String*);
using StateMember = void (APS5_VABI *)(PAL::KillRing*);
using YankMember = String (APS5_VABI *)(PAL::KillRing*);
static_assert(std::is_same_v<decltype(&_ZN3PAL8KillRing6appendERKN3WTF6StringE), StringMember>);
static_assert(std::is_same_v<decltype(&_ZN3PAL8KillRing7prependERKN3WTF6StringE), StringMember>);
static_assert(std::is_same_v<decltype(&_ZN3PAL8KillRing4yankEv), YankMember>);
static_assert(std::is_same_v<decltype(&_ZN3PAL8KillRing16startNewSequenceEv), StateMember>);
static_assert(std::is_same_v<decltype(&_ZN3PAL8KillRing16setToYankedStateEv), StateMember>);

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

bool Holds(const String& string, const char* text, unsigned refCount) {
    return string.impl && string.impl->refCount == refCount && string.impl->length == std::strlen(text) &&
           std::memcmp(string.impl->data, text, string.impl->length) == 0;
}

void RequireYankIsNull(PAL::KillRing* ring) {
    String yanked = _ZN3PAL8KillRing4yankEv(ring);
    Require(yanked.impl == nullptr);
}

}  // namespace

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    PAL::KillRing ring;
    RequireYankIsNull(&ring);

    String first = Make("first");
    String second = Make("second");
    const unsigned firstCount = first.impl->refCount;
    const unsigned secondCount = second.impl->refCount;
    String empty;

    _ZN3PAL8KillRing16startNewSequenceEv(&ring);
    _ZN3PAL8KillRing6appendERKN3WTF6StringE(&ring, &first);
    _ZN3PAL8KillRing7prependERKN3WTF6StringE(&ring, &second);
    _ZN3PAL8KillRing6appendERKN3WTF6StringE(&ring, &empty);
    _ZN3PAL8KillRing7prependERKN3WTF6StringE(&ring, &empty);
    Require(Holds(first, "first", firstCount));
    Require(Holds(second, "second", secondCount));
    RequireYankIsNull(&ring);

    _ZN3PAL8KillRing16setToYankedStateEv(&ring);
    RequireYankIsNull(&ring);
    _ZN3PAL8KillRing16startNewSequenceEv(&ring);
    _ZN3PAL8KillRing6appendERKN3WTF6StringE(&ring, &second);
    RequireYankIsNull(&ring);
    Require(Holds(second, "second", secondCount));

    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}

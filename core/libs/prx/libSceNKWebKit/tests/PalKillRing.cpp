#include "prx/libSceNKWebKit/include/PalKillRingExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

using NKWebKit::PalAbi::KillRing;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PAL KillRing check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}  // namespace

int main() {
    KillRing ring{};
    String text;
    _ZN3PAL8KillRing6appendERKN3WTF6StringE(&ring, &text);
    _ZN3PAL8KillRing7prependERKN3WTF6StringE(&ring, &text);
    _ZN3PAL8KillRing16startNewSequenceEv(&ring);
    _ZN3PAL8KillRing16setToYankedStateEv(&ring);
    Require(ring.empty == 0);
    Require(text.impl == nullptr);

    alignas(String) unsigned char storage[sizeof(String)];
    std::memset(storage, 0xa5, sizeof(storage));
    String* yanked = new (storage) String(_ZN3PAL8KillRing4yankEv(&ring));
    Require(yanked->impl == nullptr);
    yanked->~String();
    return 0;
}

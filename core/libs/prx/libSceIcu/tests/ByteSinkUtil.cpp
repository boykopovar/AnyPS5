// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>
#include <cstring>

namespace {
void Check(bool value) { if (!value) std::abort(); }
}
int main() {
    using namespace Icu::Abi;
    CharString destination{};
    destination.buffer.pointer = destination.buffer.stack;
    destination.buffer.capacity = 40;
    CharStringByteSink object{};
    _ZN6icu_6818CharStringByteSinkC1EPNS_10CharStringE_nid_postfix(&object, &destination);
    auto* sink = reinterpret_cast<ByteSink*>(&object);
    _ZN6icu_6812ByteSinkUtil14appendTwoBytesEiRNS_8ByteSinkE_nid_postfix(0x80, sink);
    Check(destination.length == 2 && static_cast<unsigned char>(destination.buffer.pointer[0]) == 0xc2 && static_cast<unsigned char>(destination.buffer.pointer[1]) == 0x80);
    Edits edits{};
    edits.array = edits.stack;
    edits.capacity = 100;
    _ZN6icu_6812ByteSinkUtil15appendCodePointEiiRNS_8ByteSinkEPNS_5EditsE_nid_postfix(1, 0x1f600, sink, &edits);
    Check(destination.length == 6 && edits.delta == 3 && edits.changes == 1);
    const std::uint8_t unchanged[]{'a', 'b'};
    _ZN6icu_6812ByteSinkUtil23appendNonEmptyUnchangedEPKhiRNS_8ByteSinkEjPNS_5EditsE_nid_postfix(unchanged, 2, sink, 0x4000, &edits);
    Check(destination.length == 6);
    std::int32_t status = 3;
    Check(!_ZN6icu_6812ByteSinkUtil12appendChangeEiPKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(1, nullptr, 100, nullptr, &edits, &status));
    status = 0;
    Check(_ZN6icu_6812ByteSinkUtil12appendChangeEiPKDsiRNS_8ByteSinkEPNS_5EditsER10UErrorCode_nid_postfix(2, u"xyz", 3, sink, &edits, &status));
    Check(status == 0 && destination.length == 9 && std::memcmp(destination.buffer.pointer + 6, "xyz", 3) == 0);
    _ZN6icu_685EditsD1Ev_nid_postfix(&edits);
    _ZN6icu_6818CharStringByteSinkD1Ev_nid_postfix(&object);
    if (destination.buffer.needToRelease) _ZN6icu_687UMemorydlEPv_nid_postfix(destination.buffer.pointer);
    return 0;
}

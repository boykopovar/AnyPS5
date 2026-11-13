// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdlib>
#include <cstring>

void Check(bool value) { if (!value) std::abort(); }
int main() {
    using namespace Icu::Abi;
    const char16_t units[]{u'a', 0xd83d, 0xde00, 0xd800};
    UnicodeString input{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&input, units, 4);
    std::array<char, 32> buffer{};
    CheckedArrayByteSink sink{};
    _ZN6icu_6820CheckedArrayByteSinkC1EPci_nid_postfix(&sink, buffer.data(), buffer.size());
    _ZNK6icu_6813UnicodeString6toUTF8ERNS_8ByteSinkE_nid_postfix(&input, reinterpret_cast<ByteSink*>(&sink));
    const unsigned char expected[]{0x61, 0xf0, 0x9f, 0x98, 0x80, 0xef, 0xbf, 0xbd};
    Check(sink.size == 8 && sink.appended == 8 && !sink.overflowed && std::memcmp(buffer.data(), expected, 8) == 0);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&input);
    return 0;
}

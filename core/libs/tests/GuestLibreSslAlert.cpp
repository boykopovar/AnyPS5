#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

void Check(int value, const char* expected) {
    const char* actual = SSL_alert_desc_string_long_nid_postfix(value);
    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::fprintf(stderr, "LibreSSL alert %d: expected %s, got %s\n", value, expected, actual ? actual : "(null)");
        std::abort();
    }
}

int main() {
    std::array<const char*, 256> expected;
    expected.fill("unknown");
    expected[0] = "close notify";
    expected[10] = "unexpected_message";
    expected[20] = "bad record mac";
    expected[30] = "decompression failure";
    expected[40] = "handshake failure";
    expected[41] = "no certificate";
    expected[42] = "bad certificate";
    expected[43] = "unsupported certificate";
    expected[44] = "certificate revoked";
    expected[45] = "certificate expired";
    expected[46] = "certificate unknown";
    expected[47] = "illegal parameter";
    expected[21] = "decryption failed";
    expected[22] = "record overflow";
    expected[48] = "unknown CA";
    expected[49] = "access denied";
    expected[50] = "decode error";
    expected[51] = "decrypt error";
    expected[60] = "export restriction";
    expected[70] = "protocol version";
    expected[71] = "insufficient security";
    expected[80] = "internal error";
    expected[90] = "user canceled";
    expected[100] = "no renegotiation";
    expected[110] = "unsupported extension";
    expected[111] = "certificate unobtainable";
    expected[112] = "unrecognized name";
    expected[113] = "bad certificate status response";
    expected[114] = "bad certificate hash value";
    expected[115] = "unknown PSK identity";
    for (int code = 0; code < 256; ++code) {
        Check(code, expected[code]);
        Check(0x100 | code, expected[code]);
        Check(0x200 | code, expected[code]);
        Check(0x123400 | code, expected[code]);
        Check(code - 256, expected[code]);
    }
    Check(std::numeric_limits<int>::min(), "close notify");
    Check(std::numeric_limits<int>::max(), "unknown");
    const char* retained = SSL_alert_desc_string_long_nid_postfix(40);
    Check(42, "bad certificate");
    if (std::strcmp(retained, "handshake failure") != 0) std::abort();
    return 0;
}

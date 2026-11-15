#include "prx/libharfbuzz/include/InventoryExports.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "check failed: %s\n", message);
        std::abort();
    }
}

constexpr std::uint32_t Tag(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(d));
}

}

int main() {
    Require(hb_ot_tag_to_script_nid_postfix(0) == 0, "null tag");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('D', 'F', 'L', 'T')) == 0, "DFLT tag");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('d', 'f', 'l', 't')) == Tag('D', 'f', 'l', 't'), "dflt tag general path");

    Require(hb_ot_tag_to_script_nid_postfix(Tag('m', 'a', 't', 'h')) == Tag('Z', 'm', 't', 'h'), "math tag");

    Require(hb_ot_tag_to_script_nid_postfix(Tag('l', 'a', 't', 'n')) == Tag('L', 'a', 't', 'n'), "latn lowercase");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('L', 'A', 'T', 'N')) == Tag('L', 'A', 'T', 'N'), "LATN uppercase general path");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('a', 'r', 'a', 'b')) == Tag('A', 'r', 'a', 'b'), "arab");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('g', 'r', 'e', 'k')) == Tag('G', 'r', 'e', 'k'), "grek");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('1', '2', '3', '4')) == 0x11323334u, "numeric tag general path");

    Require(hb_ot_tag_to_script_nid_postfix(Tag('b', 'n', 'g', '2')) == Tag('B', 'e', 'n', 'g'), "bng2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('d', 'e', 'v', '2')) == Tag('D', 'e', 'v', 'a'), "dev2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('g', 'j', 'r', '2')) == Tag('G', 'u', 'j', 'r'), "gjr2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('g', 'u', 'r', '2')) == Tag('G', 'u', 'r', 'u'), "gur2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('k', 'n', 'd', '2')) == Tag('K', 'n', 'd', 'a'), "knd2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('m', 'l', 'm', '2')) == Tag('M', 'l', 'y', 'm'), "mlm2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('o', 'r', 'y', '2')) == Tag('O', 'r', 'y', 'a'), "ory2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('t', 'a', 'm', '2')) == Tag('Z', 'z', 'z', 'z'), "unknown digit-2 tag");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('t', 'm', 'l', '2')) == Tag('T', 'a', 'm', 'l'), "tml2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('t', 'e', 'l', '2')) == Tag('T', 'e', 'l', 'u'), "tel2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('m', 'y', 'm', '2')) == Tag('M', 'y', 'm', 'r'), "mym2");

    Require(hb_ot_tag_to_script_nid_postfix(Tag('d', 'e', 'v', '3')) == Tag('D', 'e', 'v', 'a'), "dev3 folded to dev2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('b', 'n', 'g', '3')) == Tag('B', 'e', 'n', 'g'), "bng3 folded to bng2");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('x', 'y', 'z', '3')) == Tag('Z', 'z', 'z', 'z'), "unknown digit-3 tag");

    Require(hb_ot_tag_to_script_nid_postfix(Tag('a', 'b', 'c', ' ')) == Tag('A', 'b', 'c', 'c'), "abc space padded");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('x', 'y', ' ', ' ')) == Tag('X', 'y', 'y', 'y'), "xy two spaces padded");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('l', 'a', 'o', ' ')) == Tag('L', 'a', 'o', 'o'), "lao padded");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('n', 'k', 'o', ' ')) == Tag('N', 'k', 'o', 'o'), "nko padded");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('v', 'a', 'i', ' ')) == Tag('V', 'a', 'i', 'i'), "vai padded");
    Require(hb_ot_tag_to_script_nid_postfix(Tag('y', 'i', ' ', ' ')) == Tag('Y', 'i', 'i', 'i'), "yi padded");

    return 0;
}

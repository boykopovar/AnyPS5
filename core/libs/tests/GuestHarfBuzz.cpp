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
    Require(hb_ot_tag_to_script(0) == 0, "null tag");
    Require(hb_ot_tag_to_script(Tag('D', 'F', 'L', 'T')) == 0, "DFLT tag");
    Require(hb_ot_tag_to_script(Tag('d', 'f', 'l', 't')) == 0, "dflt tag");

    Require(hb_ot_tag_to_script(Tag('l', 'a', 't', 'n')) == Tag('L', 'a', 't', 'n'), "latn");
    Require(hb_ot_tag_to_script(Tag('a', 'r', 'a', 'b')) == Tag('A', 'r', 'a', 'b'), "arab");
    Require(hb_ot_tag_to_script(Tag('g', 'r', 'e', 'k')) == Tag('G', 'r', 'e', 'k'), "grek");
    Require(hb_ot_tag_to_script(Tag('c', 'y', 'r', 'l')) == Tag('C', 'y', 'r', 'l'), "cyrl");
    Require(hb_ot_tag_to_script(Tag('h', 'e', 'b', 'r')) == Tag('H', 'e', 'b', 'r'), "hebr");
    Require(hb_ot_tag_to_script(Tag('L', 'A', 'T', 'N')) == Tag('L', 'a', 't', 'n'), "LATN uppercase");

    Require(hb_ot_tag_to_script(Tag('b', 'n', 'g', '2')) == Tag('B', 'e', 'n', 'g'), "bng2");
    Require(hb_ot_tag_to_script(Tag('d', 'e', 'v', '2')) == Tag('D', 'e', 'v', 'a'), "dev2");
    Require(hb_ot_tag_to_script(Tag('g', 'j', 'r', '2')) == Tag('G', 'u', 'j', 'r'), "gjr2");
    Require(hb_ot_tag_to_script(Tag('l', 'a', 'o', ' ')) == Tag('L', 'a', 'o', 'o'), "lao padded");
    Require(hb_ot_tag_to_script(Tag('n', 'k', 'o', ' ')) == Tag('N', 'k', 'o', 'o'), "nko padded");

    Require(hb_ot_tag_to_script(Tag('1', '2', '3', '4')) == Tag('Z', 'z', 'z', 'z'), "numeric tag");
    Require(hb_ot_tag_to_script(Tag('?', '!', '#', '$')) == Tag('Z', 'z', 'z', 'z'), "special characters");

    return 0;
}

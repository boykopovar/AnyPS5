#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using Compare = std::int32_t (APS5_VABI*)(const char*, const char*);

static_assert(std::is_same_v<decltype(&uprv_compareASCIIPropertyNames_nid_postfix), Compare>);
static_assert(std::is_same_v<decltype(&uprv_compareEBCDICPropertyNames_nid_postfix), Compare>);

void Ascii() {
    const Compare compare = uprv_compareASCIIPropertyNames_nid_postfix;
    Require(compare("", "") == 0);
    Require(compare("General_Category", "generalcategory") == 0);
    Require(compare(" g-c\t\n\v\f\r", "GC") == 0);
    Require(compare("_- ", "") == 0);
    Require(compare("a", "b") == -1);
    Require(compare("B", "a") == 1);
    Require(compare("a", "") == 0x61);
    Require(compare("", "A") == -0x61);
    Require(compare("abc", "A B") == 0x63);
    Require(compare("sc", "scx") == -0x78);
    Require(compare("a.b", "ab") == 0x2e - 0x62);
    Require(compare("\xc1", "\xe1") == 0xc1 - 0xe1);
    Require(compare("\x85" "a", "a") == 0x85 - 0x61);
    Require(compare("[", "{") == 0x5b - 0x7b);
    Require(compare("@", "`") == 0x40 - 0x60);
}

void Ebcdic() {
    const Compare compare = uprv_compareEBCDICPropertyNames_nid_postfix;
    Require(compare("", "") == 0);
    Require(compare("\xc7\xc3", "\x87\x83") == 0);
    Require(compare("\xc7\x6d\xc3", "\x87\x60\x40\x83") == 0);
    Require(compare("\x05\x15\x25\x0b\x0c\x0d\x87", "\xc7") == 0);
    Require(compare("\xc9\xd1\xd9\xe2\xe9", "\x89\x91\x99\xa2\xa9") == 0);
    Require(compare("\x81", "\x82") == -1);
    Require(compare("\xc2", "\x81") == 1);
    Require(compare("\x81", "") == 0x81);
    Require(compare("\xca", "\x8a") == 0xca - 0x8a);
    Require(compare("\xe1", "\xa1") == 0xe1 - 0xa1);
    Require(compare("\xc0", "\x80") == 0xc0 - 0x80);
    Require(compare("\x41", "\x61") == 0x41 - 0x61);
    Require(compare("\x2d", "") == 0x2d);
}

}

int main() {
    Ascii();
    Ebcdic();
    RequireThrows([] { uprv_compareASCIIPropertyNames_nid_postfix(nullptr, ""); });
    RequireThrows([] { uprv_compareASCIIPropertyNames_nid_postfix("", nullptr); });
    RequireThrows([] { uprv_compareEBCDICPropertyNames_nid_postfix(nullptr, ""); });
    RequireThrows([] { uprv_compareEBCDICPropertyNames_nid_postfix("", nullptr); });
    return 0;
}

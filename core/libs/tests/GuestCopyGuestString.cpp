#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CopyGuestString check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    std::string copy = "unchanged";
    Require(CopyGuestString_nid_no_patch(nullptr, 16, copy) == 14 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(0x10), 16, copy) == 14 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 0, copy) == 63 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 5, copy) == 63 && copy == "unchanged");
    Require(CopyGuestString_nid_no_patch("guest", 6, copy) == 0 && copy == "guest");
    Require(CopyGuestString_nid_no_patch("", 1, copy) == 0 && copy.empty());
    Require(CopyGuestString_nid_no_patch("guest", 16, copy) == 0 && copy == "guest");

    const std::string longText(100000, 'x');
    Require(CopyGuestString_nid_no_patch(longText.c_str(), longText.size(), copy) == 63);
    Require(CopyGuestString_nid_no_patch(longText.c_str(), longText.size() + 1, copy) == 0 && copy == longText);

    const std::size_t unbounded = std::numeric_limits<std::size_t>::max() / 2;
    Require(CopyGuestString_nid_no_patch("guest", unbounded, copy) == 0 && copy == "guest");
    Require(CopyGuestString_nid_no_patch(longText.c_str(), unbounded, copy) == 0 && copy == longText);
}

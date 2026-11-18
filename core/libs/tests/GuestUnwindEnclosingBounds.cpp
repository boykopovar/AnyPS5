#include "prx/libc/include/Api.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

extern "C" {
extern const unsigned char GuestUnwindBoundsFirst[];
extern const unsigned char GuestUnwindBoundsFirstEnd[];
extern const unsigned char GuestUnwindBoundsSecond[];
extern const unsigned char GuestUnwindBoundsSecondEnd[];
}

namespace {

void RequireEqual(const char* name, _Unwind_Ptr actual, _Unwind_Ptr expected) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s: expected 0x%" PRIxPTR ", actual 0x%" PRIxPTR "\n", name, expected, actual);
    std::exit(1);
}

void CheckEnclosing(const char* name, _Unwind_Ptr address, _Unwind_Ptr expected) {
    RequireEqual(name, _Unwind_FindEnclosingFunction_nid_postfix(reinterpret_cast<void*>(address)), expected);
}

}

int main() {
    const auto first = reinterpret_cast<_Unwind_Ptr>(GuestUnwindBoundsFirst);
    const auto firstEnd = reinterpret_cast<_Unwind_Ptr>(GuestUnwindBoundsFirstEnd);
    const auto second = reinterpret_cast<_Unwind_Ptr>(GuestUnwindBoundsSecond);
    const auto secondEnd = reinterpret_cast<_Unwind_Ptr>(GuestUnwindBoundsSecondEnd);

    RequireEqual("first frame length", firstEnd - first, 16);
    RequireEqual("second frame length", secondEnd - second, 16);
    RequireEqual("gap between frames", second - firstEnd, 16);

    CheckEnclosing("first start + 1", first + 1, first);
    CheckEnclosing("first end", firstEnd, first);
    CheckEnclosing("first end + 1", firstEnd + 1, 0);
    CheckEnclosing("first gap interior", firstEnd + 8, 0);
    CheckEnclosing("second start + 1", second + 1, second);
    CheckEnclosing("second end", secondEnd, second);
    CheckEnclosing("second end + 1", secondEnd + 1, 0);
    return 0;
}

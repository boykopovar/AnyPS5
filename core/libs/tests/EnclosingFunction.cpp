#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Unwind.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "enclosing function check failed: %s\n", message);
    std::exit(1);
}

int dataMarker = 7;

template<class Function>
_Unwind_Ptr EntryOf(Function function) {
    return reinterpret_cast<_Unwind_Ptr>(function);
}

}

extern "C" [[gnu::noinline]] void* AddressInsideCaller() {
    return __builtin_return_address(0);
}

extern "C" [[gnu::noinline]] void* EnclosingFromInside() {
    void* inside = AddressInsideCaller();
    Require(inside != nullptr, "the caller address is captured");
    Require(_Unwind_FindEnclosingFunction_nid_postfix(inside) == EntryOf(&EnclosingFromInside),
            "an address inside a function reports that function's first byte");
    return inside;
}

extern "C" [[gnu::noinline]] void* EnclosingFromOtherInside() {
    void* inside = AddressInsideCaller();
    Require(inside != nullptr, "the caller address is captured");
    Require(_Unwind_FindEnclosingFunction_nid_postfix(inside) == EntryOf(&EnclosingFromOtherInside),
            "a second function reports its own first byte");
    return inside;
}

extern "C" [[gnu::noinline]] void EnclosingFromDepth() {
    void* inside = EnclosingFromInside();
    Require(_Unwind_FindEnclosingFunction_nid_postfix(inside) == EntryOf(&EnclosingFromInside),
            "an address captures deeper in the stack still reports its own function");
    inside = EnclosingFromOtherInside();
    Require(_Unwind_FindEnclosingFunction_nid_postfix(inside) == EntryOf(&EnclosingFromOtherInside),
            "the two probed functions are told apart");
    Require(_Unwind_FindEnclosingFunction_nid_postfix(nullptr) == 0, "a null address reports no function");
    Require(_Unwind_FindEnclosingFunction_nid_postfix(&dataMarker) == 0,
            "an address in a data object reports no function");
}

int main() {
    EnclosingFromDepth();
    std::printf("enclosing function test passed\n");
    return 0;
}

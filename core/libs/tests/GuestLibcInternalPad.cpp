#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal _Pad destructor: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    GuestPad pad{};
    _ZNSt4_PadC2Ev_nid_postfix(&pad);
    Require(pad.condition != nullptr && pad.mutex != nullptr && !pad.started, "the base-object constructor creates the mutex and condition");

    sceLibcInternalPadCompleteDestructor_nid_no_patch(&pad);
    Require(pad.condition == nullptr && pad.mutex == nullptr, "the complete-object destructor releases the mutex and condition");
    Require(!pad.started && pad.vtable == nullptr, "the destructor leaves the other members alone");

    GuestPad again{};
    _ZNSt4_PadC2Ev_nid_postfix(&again);
    sceLibcInternalPadCompleteDestructor_nid_no_patch(&again);
    _ZNSt4_PadC2Ev_nid_postfix(&again);
    Require(again.condition != nullptr && again.mutex != nullptr, "a destroyed pad can be constructed again");
    _ZNSt4_PadD2Ev_nid_postfix(&again);

    Require(Throws([&] { sceLibcInternalPadCompleteDestructor_nid_no_patch(&pad); }), "destroying a pad twice is rejected");
    Require(Throws([] { sceLibcInternalPadCompleteDestructor_nid_no_patch(nullptr); }), "a null pad is rejected");

    std::puts("libc internal _Pad complete-object destructor checks passed");
    return 0;
}

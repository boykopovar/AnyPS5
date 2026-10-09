#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <thread>
struct GuestStack {
    void* sp;
    std::size_t size;
    int flags;
};
extern "C" {
int APS5_VABI sigaltstack_nid_postfix(const GuestStack*, GuestStack*);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
static GuestStack Query() {
    GuestStack current;
    std::memset(&current, 0xa5, sizeof(current));
    Require(sigaltstack_nid_postfix(nullptr, &current) == 0);
    return current;
}
static void Fails(const GuestStack& stack, int error) {
    GuestStack untouched;
    std::memset(&untouched, 0xa5, sizeof(untouched));
    GuestStack previous = untouched;
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&stack, &previous) == -1 && *__error_nid_postfix() == error);
    Require(std::memcmp(&previous, &untouched, sizeof(previous)) == 0);
}
int main() {
    alignas(16) static char area[4096];
    GuestStack current = Query();
    Require(current.sp == nullptr && current.size == 0 && current.flags == 4);
    Require(sigaltstack_nid_postfix(nullptr, nullptr) == 0);

    Fails({area, sizeof(area), 1}, 22);
    Fails({area, sizeof(area), 4 | 2}, 22);
    Fails({area, 2047, 0}, 12);
    current = Query();
    Require(current.sp == nullptr && current.size == 0 && current.flags == 4);

    GuestStack previous{};
    const GuestStack minimum{area, 2048, 0};
    Require(sigaltstack_nid_postfix(&minimum, &previous) == 0);
    Require(previous.sp == nullptr && previous.flags == 4);
    const GuestStack installed{area, sizeof(area), 0};
    Require(sigaltstack_nid_postfix(&installed, &previous) == 0);
    Require(previous.sp == area && previous.size == 2048 && previous.flags == 0);
    current = Query();
    Require(current.sp == area && current.size == sizeof(area) && current.flags == 0);

    std::thread([] {
        const GuestStack fresh = Query();
        Require(fresh.sp == nullptr && fresh.size == 0 && fresh.flags == 4);
    }).join();

    const GuestStack disable{nullptr, 0, 4};
    Require(sigaltstack_nid_postfix(&disable, &previous) == 0);
    Require(previous.sp == area && previous.size == sizeof(area) && previous.flags == 0);
    current = Query();
    Require(current.sp == area && current.size == sizeof(area) && current.flags == 4);
    Fails({area, 16, 0}, 12);
}

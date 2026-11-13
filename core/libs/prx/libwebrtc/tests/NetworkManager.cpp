#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc NetworkManager check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    unsigned char manager[64] = {};
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager) == nullptr);
    Require(_ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager + 8) == nullptr);
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(nullptr) == nullptr);
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    unsigned char manager[64] = {};
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager) == nullptr);
    Require(_ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager + 8) == nullptr);
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(nullptr) == nullptr);
    return 0;
}

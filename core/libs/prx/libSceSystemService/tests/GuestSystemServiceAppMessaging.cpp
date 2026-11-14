#include "prx/libSceSystemService/include/SystemService.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceSystemService AppMessaging check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    AppMessagingMessage message{};
    Require(sceAppMessagingTryReceiveMsg(&message) == static_cast<int>(0x80C50005));
    Require(sceAppMessagingReceiveMsg(nullptr) == static_cast<int>(0x80C50004));
    Require(sceAppMessagingTryReceiveMsg(nullptr) == static_cast<int>(0x80C50004));

    const char text[] = "abc";
    Require(sceAppMessagingSendMsg(42, 7, text, 3, 0) == 0);
    Require(sceAppMessagingSendMsgToShellCore(9, nullptr, 0, 0) == 0);
    Require(sceAppMessagingSendMsgToShellUI(10, text, 3, 0) == 0);
    Require(sceAppMessagingTryReceiveMsg(&message) == static_cast<int>(0x80C50005));

    Require(sceAppMessagingSendMsg(1, 1, nullptr, 1, 0) == static_cast<int>(0x80C50001));
    Require(sceAppMessagingSendMsg(1, 1, text, sizeof(AppMessagingMessage::payload) + 1, 0) == static_cast<int>(0x80C50001));
    Require(sceAppMessagingSendMsgToShellCore(1, nullptr, sizeof(AppMessagingMessage::payload) + 1, 0) == static_cast<int>(0x80C50001));
    Require(sceAppMessagingSendMsgToShellUI(1, nullptr, sizeof(AppMessagingMessage::payload) + 1, 0) == static_cast<int>(0x80C50001));
    Require(sceAppMessagingTryReceiveMsg(&message) == static_cast<int>(0x80C50005));

    Require(sceAppMessagingSetEventFlag(5, 0x5) == 0);
    Require(sceAppMessagingSetEventFlag(5, 0x2) == 0);
    std::uint64_t flags = 0;
    Require(sceAppMessagingTryGetEventFlag(0xf, &flags, 0) == 0 && flags == 0x7);
    Require(sceAppMessagingTryGetEventFlag(0x2, &flags, 1) == 0 && flags == 0x2);
    Require(sceAppMessagingTryGetEventFlag(0xf, &flags, 0) == 0 && flags == 0x5);
    Require(sceAppMessagingClearEventFlag(5, 0x1) == 0);
    Require(sceAppMessagingTryGetEventFlag(0xf, &flags, 0) == 0 && flags == 0x4);
    Require(sceAppMessagingTryGetEventFlag(0xf, nullptr, 0) == static_cast<int>(0x80C50004));

    std::puts("libSceSystemService AppMessaging tests passed");
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::MessagePortChannel;
using NKWebKit::WebCoreAbi::MessagePortIdentifier;
using NKWebKit::WebCoreAbi::OptionalProcessIdentifier;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "message port channel check failed at line %u\n", location.line());
        std::abort();
    }
}

MessagePortChannel MakeChannel() {
    MessagePortChannel channel {};
    channel.ports[0] = {11, 101};
    channel.ports[1] = {11, 102};
    return channel;
}

}

int main() {
    {
        MessagePortChannel channel {};
        const auto* bytes = reinterpret_cast<const unsigned char*>(&channel);
        Require(reinterpret_cast<const unsigned char*>(&channel.ports[1]) - bytes == 0x18);
        Require(reinterpret_cast<const unsigned char*>(&channel.processes[1]) - bytes == 0x40);
        Require(reinterpret_cast<const unsigned char*>(&channel.pendingMessages[1].size) - bytes == 0x7c);
    }

    MessagePortChannel channel = MakeChannel();
    channel.processes[0] = {7001, 1};
    channel.processes[1] = {7002, 1};

    const MessagePortIdentifier first = {11, 101};
    const MessagePortIdentifier second = {11, 102};
    OptionalProcessIdentifier result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &first);
    Require(result.engaged == 1 && result.value == 7001);
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &second);
    Require(result.engaged == 1 && result.value == 7002);
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &channel.ports[0]);
    Require(result.engaged == 1 && result.value == 7001);

    const MessagePortIdentifier sameProcessOnly = {11, 999};
    const MessagePortIdentifier sameObjectOnly = {12, 101};
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &sameProcessOnly);
    Require(result.value == 7002);
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &sameObjectOnly);
    Require(result.value == 7002);

    channel.processes[1] = {0, 0};
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &second);
    Require(result.engaged == 0);
    result = _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(&channel, &first);
    Require(result.engaged == 1 && result.value == 7001);

    MessagePortChannel idle = MakeChannel();
    Require(!_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&idle));

    MessagePortChannel inFlight = MakeChannel();
    inFlight.messageBatchesInFlight = 1;
    Require(_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&inFlight));

    MessagePortChannel pendingFirst = MakeChannel();
    pendingFirst.pendingMessages[0].size = 3;
    Require(_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&pendingFirst));

    MessagePortChannel pendingSecond = MakeChannel();
    pendingSecond.pendingMessages[1].size = 1;
    Require(_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&pendingSecond));

    MessagePortChannel capacityOnly = MakeChannel();
    capacityOnly.pendingMessages[0].capacity = 16;
    capacityOnly.pendingMessages[1].capacity = 16;
    Require(!_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&capacityOnly));

    MessagePortChannel highInFlight = MakeChannel();
    highInFlight.messageBatchesInFlight = 1ull << 40;
    Require(_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(&highInFlight));
    return 0;
}

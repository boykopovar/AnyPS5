#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc trivial returns check failed at line %u\n", location.line());
    std::abort();
}

std::array<unsigned char, 64> Filled() {
    std::array<unsigned char, 64> bytes{};
    std::memset(bytes.data(), 0x5A, bytes.size());
    return bytes;
}

bool Unchanged(const std::array<unsigned char, 64>& bytes) {
    for (const auto byte : bytes) {
        if (byte != 0x5A) return false;
    }
    return true;
}

} // namespace

int main() {
    auto options = Filled();
    _ZN6webrtc13PacketOptionsD1Ev_nid_postfix(options.data());
    Require(Unchanged(options));

    auto pli = Filled();
    _ZN6webrtc4rtcp3PliD1Ev_nid_postfix(pli.data());
    Require(Unchanged(pli));

    auto encoder = Filled();
    _ZN6webrtc12AudioEncoder45OnReceivedUplinkRecoverablePacketLossFractionEf_nid_postfix(encoder.data(), 0.25f);
    _ZN6webrtc12AudioEncoder45OnReceivedUplinkRecoverablePacketLossFractionEf_nid_postfix(encoder.data(), -1.0f);
    Require(Unchanged(encoder));

    auto buffer = Filled();
    Require(_ZNK6webrtc16VideoFrameBuffer7GetI420Ev_nid_postfix(buffer.data()) == nullptr);
    Require(Unchanged(buffer));

    Require(libwebrtcUnknown37() == 0);
}

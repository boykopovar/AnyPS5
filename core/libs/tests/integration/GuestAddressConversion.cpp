#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI __inet_pton_nid_postfix(int, const char*, void*);
const char* APS5_VABI __inet_ntop_nid_postfix(int, const void*, char*, std::uint32_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Address = std::array<unsigned char, 16>;

constexpr int guestInet = 2;
constexpr int guestInet6 = 28;
constexpr int linuxInet6 = 10;
constexpr int enospc = 28;
constexpr int eafnosupport = 47;

struct Sample {
    int family;
    const char* text;
};

constexpr Sample samples[] = {{guestInet, "192.0.2.17"}, {guestInet6, "2001:db8::17"}};

std::string Name(const Sample& sample) {
    return "family " + std::to_string(sample.family) + " " + sample.text;
}

Address Parse(const Sample& sample) {
    Address address{};
    RequireEqual(__inet_pton_nid_postfix(sample.family, sample.text, address.data()), 1, "parse " + Name(sample));
    return address;
}

const Case roundTrip{"InetPtonNtop_ValidAddress_RoundTrips", [] {
    for (const auto& sample : samples) {
        const Address address = Parse(sample);
        char output[64]{};
        Require(__inet_ntop_nid_postfix(sample.family, address.data(), output, sizeof(output)) == output,
                "ntop returns the output buffer for " + Name(sample));
        RequireEqual(std::string_view(output), std::string_view(sample.text), "formatted " + Name(sample));
    }
}};

const Case smallBuffer{"InetNtop_BufferTooSmall_FailsWithEnospcAndLeavesBuffer", [] {
    for (const auto& sample : samples) {
        const Address address = Parse(sample);
        char output[64]{'x'};
        Require(__inet_ntop_nid_postfix(sample.family, address.data(), output, 1) == nullptr, "null result for " + Name(sample));
        RequireEqual(*__error_nid_postfix(), enospc, "errno for " + Name(sample));
        RequireEqual(output[0], 'x', "buffer untouched for " + Name(sample));
    }
}};

const Case invalidText{"InetPton_InvalidText_ReturnsZeroAndLeavesAddress", [] {
    for (const auto& sample : samples) {
        Address address = Parse(sample);
        const Address original = address;
        RequireEqual(__inet_pton_nid_postfix(sample.family, "not-an-address", address.data()), 0, "result for " + Name(sample));
        Require(address == original, "address untouched for " + Name(sample));
    }
}};

const Case hostFamily{"InetPton_HostLinuxInet6Family_FailsWithEafnosupport", [] {
    Address address{};
    RequireEqual(__inet_pton_nid_postfix(linuxInet6, "::1", address.data()), -1, "result");
    RequireEqual(*__error_nid_postfix(), eafnosupport, "errno");
}};

const Case octetOverflow{"InetPton_OctetAbove255_ReturnsZero", [] {
    Address address{};
    RequireEqual(__inet_pton_nid_postfix(guestInet, "256.1.2.3", address.data()), 0, "256.1.2.3");
}};

const Case mappedAddress{"InetPton_Ipv4MappedIpv6_ParsesEmbeddedIpv4", [] {
    Address address{};
    RequireEqual(__inet_pton_nid_postfix(guestInet6, "::ffff:192.0.2.17", address.data()), 1, "result");
    RequireEqual(address[10], 255, "byte 10");
    RequireEqual(address[11], 255, "byte 11");
    RequireEqual(address[12], 192, "byte 12");
    RequireEqual(address[15], 17, "byte 15");
}};

} // namespace

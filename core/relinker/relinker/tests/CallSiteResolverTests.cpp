#include <relinker/analysis/CallSiteResolver.hpp>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& name) {
    if (!condition) throw std::runtime_error(name);
}

void check(const std::string& name, std::initializer_list<std::uint8_t> instruction, bool expected) {
    auto resolver = Relinker::MakeCallSiteResolver();
    const std::vector<std::uint8_t> text(instruction);
    const auto sites = resolver->ResolveCallSites(text, 0x1000, 0x1100, 8);
    require(sites == (expected ? std::vector<Relinker::FileByteOffset>{0x1000} : std::vector<Relinker::FileByteOffset>{}), name);
    require(resolver->ResolveCallSites(text, 0x1000, 0x1200, 8).empty(), name + " unexpected address");
}

}

int main() {
    try {
        check("indirect call", {0xff, 0x15, 0xfa, 0, 0, 0}, true);
        check("indirect tail jump", {0xff, 0x25, 0xfa, 0, 0, 0}, true);
        check("far indirect call", {0xff, 0x1d, 0xfa, 0, 0, 0}, true);
        check("far indirect jump", {0xff, 0x2d, 0xfa, 0, 0, 0}, true);
        check("RIP-relative function pointer load", {0x48, 0x8b, 0x05, 0xf9, 0, 0, 0}, true);
        check("REX-prefixed indirect call", {0x48, 0xff, 0x15, 0xf9, 0, 0, 0}, true);
        check("multiple REX prefixes on pointer load", {0x40, 0x48, 0x8b, 0x05, 0xf8, 0, 0, 0}, true);
        check("earlier REX.W does not widen a pointer load", {0x48, 0x40, 0x8b, 0x05, 0xf8, 0, 0, 0}, false);
        check("final REX.W widens a pointer load", {0x48, 0x40, 0x48, 0x8b, 0x05, 0xf7, 0, 0, 0}, true);
        check("BND indirect call", {0xf2, 0xff, 0x15, 0xf9, 0, 0, 0}, true);
        check("BND indirect tail jump", {0xf2, 0xff, 0x25, 0xf9, 0, 0, 0}, true);
        check("NOTRACK indirect call", {0x3e, 0xff, 0x15, 0xf9, 0, 0, 0}, true);
        check("NOTRACK indirect tail jump", {0x3e, 0xff, 0x25, 0xf9, 0, 0, 0}, true);
        check("NOTRACK BND indirect call", {0x3e, 0xf2, 0xff, 0x15, 0xf8, 0, 0, 0}, true);
        check("BND NOTRACK indirect tail jump", {0xf2, 0x3e, 0xff, 0x25, 0xf8, 0, 0, 0}, true);
        check("BND REX indirect call", {0xf2, 0x48, 0xff, 0x15, 0xf8, 0, 0, 0}, true);
        check("NOTRACK REX indirect tail jump", {0x3e, 0x48, 0xff, 0x25, 0xf8, 0, 0, 0}, true);
        check("NOTRACK-prefixed pointer load", {0x3e, 0x48, 0x8b, 0x05, 0xf8, 0, 0, 0}, true);
        check("REPNE-prefixed pointer load", {0xf2, 0x48, 0x8b, 0x05, 0xf8, 0, 0, 0}, true);
        check("DS after REX.W leaves a narrow load", {0x48, 0x3e, 0x8b, 0x05, 0xf8, 0, 0, 0}, false);
        check("REPNE after REX.W leaves a narrow load", {0x48, 0xf2, 0x8b, 0x05, 0xf8, 0, 0, 0}, false);
        check("final REX.W after DS widens a load", {0x48, 0x3e, 0x48, 0x8b, 0x05, 0xf7, 0, 0, 0}, true);
        check("final REX.W after REPNE widens a load", {0x48, 0xf2, 0x48, 0x8b, 0x05, 0xf7, 0, 0, 0}, true);
        check("NOTRACK preserves final narrow REX", {0x3e, 0x48, 0x40, 0x8b, 0x05, 0xf7, 0, 0, 0}, false);
        check("FS call is not a flat GOT reference", {0x64, 0xf2, 0xff, 0x15, 0xf8, 0, 0, 0}, false);
        check("GS jump is not a flat GOT reference", {0x3e, 0x65, 0xff, 0x25, 0xf8, 0, 0, 0}, false);
        check("address-size override is not a RIP64 reference", {0x3e, 0x67, 0xff, 0x15, 0xf8, 0, 0, 0}, false);
        check("BND register call has no RIP operand", {0xf2, 0xff, 0xd0}, false);
        check("NOTRACK register jump has no RIP operand", {0x3e, 0xff, 0xe0}, false);
        check("BND-prefixed increment is not an import access", {0xf2, 0xff, 0x05, 0xf9, 0, 0, 0}, false);
        check("DS-prefixed decrement is not an import access", {0x3e, 0xff, 0x0d, 0xf9, 0, 0, 0}, false);
        check("pointer load without REX.W", {0x40, 0x8b, 0x05, 0xf9, 0, 0, 0}, false);
        check("register pointer load", {0x48, 0x8b, 0x00}, false);
        check("increment memory", {0xff, 0x05, 0xfa, 0, 0, 0}, false);
        check("decrement memory", {0xff, 0x0d, 0xfa, 0, 0, 0}, false);
        check("push memory", {0xff, 0x35, 0xfa, 0, 0, 0}, true);
        std::cout << "Call-site resolver tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "call_site_resolver_tests: " << error.what() << '\n';
        return 1;
    }
}

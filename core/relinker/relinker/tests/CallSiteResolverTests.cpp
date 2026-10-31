#include <relinker/analysis/CallSiteResolver.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& name) {
    if (!condition) throw std::runtime_error(name);
}

void check(const std::string& name, const std::vector<std::uint8_t>& text, bool expected) {
    auto resolver = Relinker::MakeCallSiteResolver();
    const auto sites = resolver->ResolveCallSites(text, 0x1000, 0x1100, 8);
    require(sites == (expected ? std::vector<Relinker::FileByteOffset>{0x1000} : std::vector<Relinker::FileByteOffset>{}), name);
    require(resolver->ResolveCallSites(text, 0x1000, 0x1200, 8).empty(), name + " unexpected address");
}

std::vector<std::uint8_t> ripInstruction(std::vector<std::uint8_t> prefixes, std::uint8_t opcode, std::uint8_t modrm) {
    prefixes.push_back(opcode);
    prefixes.push_back(modrm);
    const auto displacement = static_cast<std::uint32_t>(0x100 - (prefixes.size() + 4));
    for (unsigned shift = 0; shift < 32; shift += 8)
        prefixes.push_back(static_cast<std::uint8_t>(displacement >> shift));
    return prefixes;
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
        for (const std::uint8_t segment : {0x2e, 0x26, 0x36, 0x3e}) {
            const auto name = "segment " + std::to_string(segment);
            for (const std::uint8_t modrm : {0x15, 0x1d, 0x25, 0x2d, 0x35})
                check(name + " indirect access " + std::to_string(modrm), ripInstruction({segment}, 0xff, modrm), true);
            check(name + " pointer load", ripInstruction({segment, 0x48}, 0x8b, 0x05), true);
            check(name + " before BND call", ripInstruction({segment, 0xf2}, 0xff, 0x15), true);
            check(name + " after BND jump", ripInstruction({0xf2, segment}, 0xff, 0x25), true);
            check(name + " after REX.W leaves a narrow load", ripInstruction({0x48, segment}, 0x8b, 0x05), false);
            check(name + " final REX.W restores pointer width", ripInstruction({0x48, segment, 0x48}, 0x8b, 0x05), true);
            check(name + " final narrow REX", ripInstruction({segment, 0x48, 0x40}, 0x8b, 0x05), false);
            check(name + " increment", ripInstruction({segment}, 0xff, 0x05), false);
            check(name + " decrement", ripInstruction({segment}, 0xff, 0x0d), false);
            check(name + " narrow load", ripInstruction({segment}, 0x8b, 0x05), false);
            for (const std::uint8_t unsupported : {0x64, 0x65, 0x67, 0x66, 0xf0, 0xf3}) {
                const auto suffix = " unsupported prefix " + std::to_string(unsupported);
                check(name + " before" + suffix, ripInstruction({segment, unsupported}, 0xff, 0x15), false);
                check(name + " after" + suffix, ripInstruction({unsupported, segment}, 0xff, 0x15), false);
            }
        }
        std::cout << "Call-site resolver tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "call_site_resolver_tests: " << error.what() << '\n';
        return 1;
    }
}

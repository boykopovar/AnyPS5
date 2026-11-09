#include <relinker/analysis/CallSiteResolver.hpp>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

static void Require(bool ok) { if (!ok) throw std::runtime_error("Incorrect call-site index"); }

int main() {
    try {
        std::vector<std::uint8_t> code;
        const std::vector<std::uint64_t> targets{0x2000, 0x1000, 0x2000, 0x3000, 0x1008};
        for (const auto target : targets) {
            const auto offset = code.size();
            code.insert(code.end(), {0xFF, 0x15, 0, 0, 0, 0});
            const auto disp = static_cast<std::int32_t>(target - (0x100 + offset + 6));
            std::memcpy(code.data() + offset + 2, &disp, 4);
        }
        auto resolver = Relinker::MakeCallSiteResolver();
        Require(resolver->ResolveCallSites(code, 0x100, 0x2000, 8) == std::vector<std::uint64_t>{0x100, 0x10C});
        Require(resolver->ResolveCallSites(code, 0x100, 0x1000, 8) == std::vector<std::uint64_t>{0x106});
        Require(resolver->ResolveCallSites(code, 0x100, 0x1000, 9) == std::vector<std::uint64_t>{0x106, 0x118});
        Require(resolver->ResolveCallSites(code, 0x100, 0x2000, 0).empty());
        Require(resolver->ResolveCallSites(code, 0x100, 0x4000, 8).empty());
        auto copy = code;
        Require(resolver->ResolveCallSites(copy, 0x100, 0x2000, 8) == std::vector<std::uint64_t>{0x100, 0x10C});
        Require(resolver->ResolveCallSites(copy, 0x200, 0x2100, 8) == std::vector<std::uint64_t>{0x200, 0x20C});
        std::vector<std::uint8_t> mov{0x48, 0x8B, 0x05, 0xF9, 0x0F, 0, 0};
        Require(resolver->ResolveCallSites(mov, 0x1000, 0x2000, 8) == std::vector<std::uint64_t>{0x1000});
        Require(resolver->ResolveCallSites({}, 0, 0, 1).empty());
        bool rejected = false;
        try { resolver->ResolveCallSites(code, 0, std::numeric_limits<std::uint64_t>::max(), 1); }
        catch (const std::exception&) { rejected = true; }
        Require(rejected);
        rejected = false;
        try { resolver->ResolveCallSites(code, std::numeric_limits<std::uint64_t>::max(), 0x1000, 8); }
        catch (const std::exception&) { rejected = true; }
        Require(rejected);
        constexpr std::uint64_t highBase = 0x8000000000000000ULL;
        std::vector<std::uint8_t> highCode{0xFF, 0x15, 0xFA, 0xFF, 0xFF, 0xFF};
        Require(resolver->ResolveCallSites(highCode, highBase + 0x100, highBase + 0x100, 8) == std::vector<std::uint64_t>{highBase + 0x100});
        std::cout << "12 call-site index cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "check failed: %s\n", message);
        std::abort();
    }
}

struct Elf64Rela {
    std::uint64_t r_offset;
    std::uint64_t r_info;
    std::int64_t r_addend;
};

constexpr std::uint32_t R_X86_64_PC32 = 2;

constexpr std::uint64_t PackInfo(std::uint32_t sym, std::uint32_t type) {
    return (static_cast<std::uint64_t>(sym) << 32) | type;
}

} // namespace

int main() {
    volatile std::int32_t target = 0;
    const std::uint64_t symbolValue = 0x20000000ULL;
    const std::uint64_t placeAddress = 0x10000000ULL;
    const std::int64_t addend = -4;

    Elf64Rela rela{
        .r_offset = placeAddress,
        .r_info = PackInfo(1, R_X86_64_PC32),
        .r_addend = addend
    };

    Require(rela.r_info == 0x100000002ULL, "r_info packing pc32");
    Require(rela.r_addend == -4, "r_addend pc32");

    std::int64_t displacement = static_cast<std::int64_t>(symbolValue) + rela.r_addend - static_cast<std::int64_t>(placeAddress);
    Require(displacement == 0x10000000ULL, "pc32 displacement calculation");

    Require(displacement >= static_cast<std::int64_t>(-0x80000000LL) &&
            displacement <= static_cast<std::int64_t>(0x7fffffffLL), "pc32 range bounds");

    target = static_cast<std::int32_t>(displacement);
    Require(target == 0x10000000, "target value updated");

    std::puts("GuestRelinkerPc32Reloc checks passed");
    return 0;
}

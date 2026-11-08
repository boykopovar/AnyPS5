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

constexpr std::uint32_t R_X86_64_64 = 1;

constexpr std::uint64_t PackInfo(std::uint32_t sym, std::uint32_t type) {
    return (static_cast<std::uint64_t>(sym) << 32) | type;
}

} // namespace

int main() {
    volatile std::uint64_t target = 0x1234567887654321ULL;
    const std::uint64_t symbolValue = 0x10000000ULL;
    const std::int64_t addend = 0x1234ULL;

    Elf64Rela rela{
        .r_offset = reinterpret_cast<std::uint64_t>(&target),
        .r_info = PackInfo(1, R_X86_64_64),
        .r_addend = addend
    };

    Require(rela.r_info == 0x100000001ULL, "r_info packing");
    Require(rela.r_addend == 0x1234ULL, "r_addend value");
    Require(rela.r_offset != 0, "r_offset non-zero");

    std::uint64_t computed = symbolValue + static_cast<std::uint64_t>(rela.r_addend);
    Require(computed == 0x10001234ULL, "direct relocation arithmetic");

    *reinterpret_cast<std::uint64_t*>(rela.r_offset) = computed;
    Require(target == 0x10001234ULL, "target updated via relocation");

    std::puts("GuestRelinkerDirect64 checks passed");
    return 0;
}

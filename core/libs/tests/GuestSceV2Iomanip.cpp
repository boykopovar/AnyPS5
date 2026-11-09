#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

struct Case {
    const char* label;
    int kind;
    std::int64_t argument;
    std::uint32_t flags;
    std::uint64_t returned;
    std::uint32_t resultFlags;
    std::uint64_t resultPrecision;
};

const Case Cases[] = {
    {"resetiosflags clears the given bits", 0, 513LL, 0xa03u, 0x201ULL, 0x802u, 0x5555555555555555ULL},
    {"resetiosflags with every bit", 0, -1LL, 0xffffffffu, 0xffffffffULL, 0x0u, 0x5555555555555555ULL},
    {"resetiosflags with high bits", 0, 2147418112LL, 0xffffffffu, 0x7fff0000ULL, 0x8000ffffu, 0x5555555555555555ULL},
    {"resetiosflags with no bits", 0, 0LL, 0x1234abcdu, 0x0ULL, 0x1234abcdu, 0x5555555555555555ULL},
    {"setiosflags sets the low bits", 1, 2560LL, 0x1u, 0xa00ULL, 0xa01u, 0x5555555555555555ULL},
    {"setiosflags clears high bits it does not set", 1, 196612LL, 0xffff0000u, 0x30004ULL, 0xfffc0004u, 0x5555555555555555ULL},
    {"setiosflags with every bit", 1, -1LL, 0x12345678u, 0xffffffffULL, 0xffffu, 0x5555555555555555ULL},
    {"setiosflags with no bits", 1, 0LL, 0x9abcdef0u, 0x0ULL, 0x9abcdef0u, 0x5555555555555555ULL},
    {"setbase 8", 2, 8LL, 0xffffffffu, 0x8ULL, 0xfffff5ffu, 0x5555555555555555ULL},
    {"setbase 10", 2, 10LL, 0xc01u, 0xaULL, 0x201u, 0x5555555555555555ULL},
    {"setbase 16", 2, 16LL, 0x201u, 0x10ULL, 0x801u, 0x5555555555555555ULL},
    {"setbase 0", 2, 0LL, 0xe05u, 0x0ULL, 0x5u, 0x5555555555555555ULL},
    {"setbase 2", 2, 2LL, 0x405u, 0x2ULL, 0x5u, 0x5555555555555555ULL},
    {"setbase -8", 2, -8LL, 0x405u, 0xfffffff8ULL, 0x5u, 0x5555555555555555ULL},
    {"setbase 24", 2, 24LL, 0x5u, 0x18ULL, 0x5u, 0x5555555555555555ULL},
    {"setprecision 6", 3, 6LL, 0x1234u, 0x6ULL, 0x1234u, 0x6ULL},
    {"setprecision 0", 3, 0LL, 0x1234u, 0x0ULL, 0x1234u, 0x0ULL},
    {"setprecision -1", 3, -1LL, 0x1234u, 0xffffffffffffffffULL, 0x1234u, 0xffffffffffffffffULL},
    {"setprecision above 32 bits", 3, 1250999896491LL, 0x1234u, 0x123456789abULL, 0x1234u, 0x123456789abULL},
};

SceV2StreamManipulator Make(int kind, std::int64_t argument) {
    switch (kind) {
    case 0: return _ZNSt8__sce_v213resetiosflagsEi_nid_postfix(static_cast<std::int32_t>(argument));
    case 1: return _ZNSt8__sce_v211setiosflagsEi_nid_postfix(static_cast<std::int32_t>(argument));
    case 2: return _ZNSt8__sce_v27setbaseEi_nid_postfix(static_cast<std::int32_t>(argument));
    default: return _ZNSt8__sce_v212setprecisionEx_nid_postfix(argument);
    }
}

}

int main() {
    for (const auto& entry : Cases) {
        std::uint64_t words[9];
        for (int i = 0; i < 9; ++i) words[i] = 0x1111111111111111ULL * static_cast<std::uint64_t>(i + 1);
        auto* bytes = reinterpret_cast<unsigned char*>(words);
        std::memcpy(bytes + 0x18, &entry.flags, sizeof(entry.flags));
        const SceV2StreamManipulator manipulator = Make(entry.kind, entry.argument);
        manipulator.function(reinterpret_cast<GuestLocale::SceV2IosBase*>(words), manipulator.argument);
        std::uint32_t flags;
        std::uint32_t high;
        std::uint64_t precision;
        std::memcpy(&flags, bytes + 0x18, sizeof(flags));
        std::memcpy(&high, bytes + 0x1c, sizeof(high));
        std::memcpy(&precision, bytes + 0x20, sizeof(precision));
        bool untouched = high == 0x44444444u;
        for (int i = 0; i < 9; ++i)
            if (i != 3 && i != 4 && words[i] != 0x1111111111111111ULL * static_cast<std::uint64_t>(i + 1)) untouched = false;
        bool nullThrows = false;
        try {
            manipulator.function(nullptr, manipulator.argument);
        } catch (const std::invalid_argument&) {
            nullThrows = true;
        }
        if (manipulator.argument == entry.returned && flags == entry.resultFlags && precision == entry.resultPrecision && untouched && nullThrows) continue;
        std::fprintf(stderr, "sce v2 iomanip: %s: argument %llx flags %x precision %llx\n", entry.label,
            static_cast<unsigned long long>(manipulator.argument), flags, static_cast<unsigned long long>(precision));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

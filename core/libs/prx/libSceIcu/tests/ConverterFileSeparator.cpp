#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "ucnv_bld.h"
#include "unicode/ucnv.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <source_location>
#include <vector>

namespace {

constexpr std::size_t Page = 0x4000;
constexpr std::size_t MappingBytes = Page * 2;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU file separator check failed at line %u\n", location.line());
    std::abort();
}

struct Converter {
    UConverter* native = nullptr;
    UConverterSharedData* originalShared = nullptr;
    UConverterSharedData shared{};
    UConverterStaticData staticData{};

    explicit Converter(const char* name = nullptr) {
        UErrorCode error = U_ZERO_ERROR;
        native = ucnv_open("UTF-8", &error);
        Require(U_SUCCESS(error) && native != nullptr);
        if (!name) return;
        originalShared = native->sharedData;
        shared = *originalShared;
        staticData = *originalShared->staticData;
        Require(std::strlen(name) < sizeof(staticData.name));
        std::strcpy(staticData.name, name);
        shared.staticData = &staticData;
        native->sharedData = &shared;
    }

    ~Converter() {
        if (originalShared) native->sharedData = originalShared;
        ucnv_close(native);
    }

    const Icu::Converter* Guest() const { return reinterpret_cast<const Icu::Converter*>(native); }
};

struct Mapping {
    std::int64_t physical = 0;
    char16_t* data = nullptr;

    Mapping() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, MappingBytes, 0, 0, &physical) == 0);
        void* address = nullptr;
        Require(sceKernelMapDirectMemory(&address, MappingBytes, 3, 0, physical, 0) == 0);
        data = static_cast<char16_t*>(address);
        Require(data != nullptr);
        Require(GuestArena::GuestArenaContains_nid_postfix(data, MappingBytes));
    }

    ~Mapping() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), MappingBytes) == 0);
        Require(sceKernelReleaseDirectMemory(physical, MappingBytes) == 0);
    }

    void Writable() { Require(sceKernelMprotect(data, MappingBytes, 3) == 0); }
    void ReadOnly() { Require(sceKernelMprotect(data, MappingBytes, 1) == 0); }

    void Fill(char16_t value) {
        Writable();
        GuestArena::HostWrite access(data, MappingBytes);
        Require(access.Open());
        std::fill_n(data, MappingBytes / sizeof(char16_t), value);
    }
};

void Apply(const Converter& converter, char16_t* source, std::int32_t length, int marker) {
    errno = marker;
    ucnv_fixFileSeparator_nid_postfix(converter.Guest(), source, length);
    Require(errno == marker);
}

void AmbiguousMappings(Mapping& mapping) {
    const std::array<std::pair<const char*, char16_t>, 2> cases{{
        {"ibm-943_P130-1999", 0x00a5},
        {"ibm-949_P110-1999", 0x20a9},
    }};
    for (const auto& [name, variant] : cases) {
        Converter converter(name);
        Require(ucnv_isAmbiguous(converter.native));
        mapping.Fill(0x4a4a);
        auto* source = mapping.data + 7;
        constexpr std::size_t Count = 1030;
        source[0] = variant;
        source[1] = 0x005c;
        source[2] = variant == 0x00a5 ? 0x20a9 : 0x00a5;
        source[1023] = variant;
        source[1024] = variant;
        source[Count - 1] = variant;
        auto expected = std::vector<char16_t>(source, source + Count);
        for (auto& unit : expected)
            if (unit == variant) unit = 0x005c;
        Apply(converter, source, Count, 51);
        Require(std::equal(expected.begin(), expected.end(), source));
        Require(source[-1] == 0x4a4a && source[Count] == 0x4a4a);
    }
}

void NonAmbiguous(Mapping& mapping) {
    Converter converter;
    Require(!ucnv_isAmbiguous(converter.native));
    mapping.Fill(0x5b5b);
    auto* source = mapping.data + 11;
    const std::array<char16_t, 6> initial{0x00a5, 0x20a9, 0x005c, 0, 0xffff, 0xd800};
    std::copy(initial.begin(), initial.end(), source);
    Apply(converter, source, static_cast<std::int32_t>(initial.size()), 52);
    Require(std::equal(initial.begin(), initial.end(), source));
    Require(source[-1] == 0x5b5b && source[initial.size()] == 0x5b5b);
}

void NoAccess(Mapping& mapping) {
    Converter ambiguous("ibm-943_P130-1999");
    Converter ordinary;
    auto* invalidSource = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    auto* invalidConverter = reinterpret_cast<const Icu::Converter*>(std::uintptr_t{0x10});
    for (const auto length : {std::numeric_limits<std::int32_t>::min(), -1, 0}) {
        errno = 60 + length % 7;
        const int marker = errno;
        ucnv_fixFileSeparator_nid_postfix(invalidConverter, invalidSource, length);
        Require(errno == marker);
    }
    errno = 61;
    ucnv_fixFileSeparator_nid_postfix(nullptr, invalidSource, 4);
    Require(errno == 61);
    errno = 62;
    ucnv_fixFileSeparator_nid_postfix(invalidConverter, nullptr, 4);
    Require(errno == 62);
    Apply(ordinary, invalidSource, 4, 63);

    mapping.Fill(0x6c6c);
    auto* source = mapping.data + 17;
    source[0] = 0x00a5;
    source[1] = 0x20a9;
    const std::array<char16_t, 2> before{source[0], source[1]};
    mapping.ReadOnly();
    Apply(ordinary, source, 2, 64);
    Apply(ambiguous, source, 0, 65);
    Apply(ambiguous, source, -1, 66);
    Require(std::equal(before.begin(), before.end(), source));
}

void RejectedDestinations(Mapping& mapping) {
    Converter converter("ibm-943_P130-1999");
    auto* invalid = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    errno = 71;
    bool rejected = false;
    try {
        ucnv_fixFileSeparator_nid_postfix(converter.Guest(), invalid, 1);
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected && errno == 71);

    mapping.Fill(0x00a5);
    auto* source = mapping.data + 23;
    mapping.ReadOnly();
    errno = 72;
    rejected = false;
    try {
        ucnv_fixFileSeparator_nid_postfix(converter.Guest(), source, 2);
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected && errno == 72);
    Require(source[0] == 0x00a5 && source[1] == 0x00a5);

    mapping.Fill(0x1234);
    source = mapping.data + 23;
    mapping.ReadOnly();
    Apply(converter, source, 2, 73);
    Require(source[0] == 0x1234 && source[1] == 0x1234);
}

} // namespace

int main() {
    Mapping mapping;
    AmbiguousMappings(mapping);
    NonAmbiguous(mapping);
    NoAccess(mapping);
    RejectedDestinations(mapping);
}

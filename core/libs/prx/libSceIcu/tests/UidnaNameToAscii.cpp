#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uidna_nameToASCII_nid_postfix), std::int32_t (APS5_VABI*)(const UIDNA*, const char16_t*, std::int32_t, char16_t*, std::int32_t, UIDNAInfo*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU IDNA name to ASCII check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

int main() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto idnaStorage = Filler();
    const auto* idna = reinterpret_cast<const UIDNA*>(idnaStorage.data());
    std::array<char16_t, 4> name{u'a', u'b', u'c', 0};
    std::array<char16_t, 4> dest{u'x', u'y', u'z', 0};
    std::array<std::int16_t, 8> info{};
    info[0] = 16;
    auto* pInfo = reinterpret_cast<UIDNAInfo*>(info.data());
    const auto probe = info;
    const auto probeName = name;
    const auto probeDest = dest;

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, nullptr, 5, dest.data(), -1, nullptr, &status) == 0);
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, nullptr, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        std::array<std::int16_t, 8> small{};
        small[0] = 15;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, reinterpret_cast<UIDNAInfo*>(small.data()), &status) == 0);
        Require(status == IllegalArgument);
        Require(small[0] == 15 && small[1] == 0);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, nullptr, 3, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), -2, dest.data(), 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, nullptr, 4, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), -1, pInfo, &status) == 0);
        Require(status == IllegalArgument);

        status = incoming;
        Require(uidna_nameToASCII_nid_postfix(idna, name.data(), 3, name.data(), 3, pInfo, &status) == 0);
        Require(status == IllegalArgument);
        Require(name == probeName && dest == probeDest);
        Require(info == probe);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, name.data(), -1, dest.data(), 0, pInfo, &status); });
        Require(status == incoming);
        Require(name == probeName && dest == probeDest);

        status = incoming;
        RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, nullptr, 0, nullptr, 0, pInfo, &status); });
        Require(status == incoming);
        Require(Unchanged(idnaStorage));
    }

    RequireThrows([&] { uidna_nameToASCII_nid_postfix(idna, name.data(), 3, dest.data(), 4, pInfo, nullptr); });
    Require(name == probeName && dest == probeDest);

    std::puts("ICU IDNA name to ASCII tests passed");
}

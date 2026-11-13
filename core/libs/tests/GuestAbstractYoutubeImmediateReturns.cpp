#include "prx/libSceAbstractYoutube/libSceAbstractYoutube.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceAbstractYoutube immediate returns check failed at line %u\n", location.line());
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

constexpr int Error0005 = static_cast<int>(0x809E0005u);

} // namespace

int main() {
    auto object = Filled();
    auto first = Filled();
    auto second = Filled();
    auto third = Filled();
    auto* self = object.data();
    auto* one = reinterpret_cast<std::int64_t*>(first.data());
    auto* two = reinterpret_cast<std::int64_t*>(second.data());
    auto* three = reinterpret_cast<std::int64_t*>(third.data());

    Require(_ZNK15AbstractStorage14YoutubeContent13GetCapabilityEv(self) == 0x6C0A);
    Require(_ZNK15AbstractStorage14YoutubeContent13GetCapabilityEv(self) == 0x6C0A);
    Require(_ZNK15AbstractStorage13YoutubeFolder13GetCapabilityEv(self) == 0x1000000000);
    Require(_ZNK15AbstractStorage14YoutubeStorage13GetCapabilityEv(self) == 0x180000000000000);

    Require(_ZNK15AbstractStorage14YoutubeContent10GetServiceEv(self) == 2);
    Require(_ZNK15AbstractStorage13YoutubeFolder10GetServiceEv(self) == 2);
    Require(_ZNK15AbstractStorage14YoutubeStorage10GetServiceEv(self) == 2);
    Require(_ZNK15AbstractStorage14YoutubeService14GetServiceTypeEv(self) == 2);

    Require(_ZNK15AbstractStorage14YoutubeContent7GetStatEPNS_4StatE(self, first.data()) == Error0005);
    Require(_ZNK15AbstractStorage13YoutubeFolder7GetStatEPNS_4StatE(self, first.data()) == Error0005);
    Require(_ZNK15AbstractStorage14YoutubeStorage7GetStatEPNS_4StatE(self, first.data()) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent4ReadEPvlPl(self, first.data(), 64, two) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent8SeekTimeElPl(self, 1000, one) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent5FlushEv(self) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent6RemoveEv(self) == Error0005);
    Require(_ZN15AbstractStorage13YoutubeFolder6RemoveEb(self, true) == Error0005);
    Require(_ZN15AbstractStorage13YoutubeFolder6RemoveEb(self, false) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeStorage14GetStorageSizeEPlS1_S1_(self, one, two, three) == Error0005);

    Require(_ZNK15AbstractStorage14YoutubeContent7GetStatEPNS_4StatE(nullptr, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent4ReadEPvlPl(nullptr, nullptr, 0, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeStorage14GetStorageSizeEPlS1_S1_(nullptr, nullptr, nullptr, nullptr) == Error0005);

    Require(Unchanged(object));
    Require(Unchanged(first));
    Require(Unchanged(second));
    Require(Unchanged(third));
}

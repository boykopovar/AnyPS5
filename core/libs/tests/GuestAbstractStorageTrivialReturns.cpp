#include "prx/libSceAbstractStorage/libSceAbstractStorage.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceAbstractStorage trivial returns check failed at line %u\n", location.line());
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

constexpr int Error0002 = static_cast<int>(0x809E0002u);
constexpr int Error0005 = static_cast<int>(0x809E0005u);

} // namespace

int main() {
    auto object = Filled();
    auto first = Filled();
    auto second = Filled();

    _ZN15AbstractStorage12SerializableD1Ev(object.data());
    _ZN15AbstractStorage12SerializableD2Ev(object.data());

    Require(_ZN15AbstractStorage14MemfileContent6RemoveEv(object.data()) == Error0002);
    std::int64_t position = 0x1234;
    Require(_ZN15AbstractStorage14MemfileContent8SeekTimeElPl(object.data(), 5, &position) == Error0002);
    Require(position == 0x1234);
    Require(_ZN15AbstractStorage14MemfileContent8SeekTimeElPl(object.data(), -1, nullptr) == Error0002);
    Require(_ZNK15AbstractStorage14MemfileContent7GetStatEPNS_4StatE(object.data(), first.data()) == Error0002);
    Require(_ZNK15AbstractStorage14MemfileContent7GetStatEPNS_4StatE(object.data(), nullptr) == Error0002);
    Require(_ZNK15AbstractStorage14MemfileContent10GetServiceEv(object.data()) == -1);
    Require(_ZNK15AbstractStorage14MemfileContent13GetCapabilityEv(object.data()) == 0x17C1A);

    Require(_ZN15AbstractStorage4Item11SetMetadataEPN23sceMetadataReaderWriter8MetadataE(object.data(), first.data()) == Error0005);
    Require(_ZN15AbstractStorage4Item13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_(object.data(), first.data(), second.data()) == Error0005);

    Require(_ZNK15AbstractStorage7Storage7GetTypeEv(object.data()) == 1);
    Require(_ZNK15AbstractStorage6Folder7GetTypeEv(object.data()) == 2);
    Require(_ZNK15AbstractStorage7Content7GetTypeEv(object.data()) == 3);

    Require(Unchanged(object));
    Require(Unchanged(first));
    Require(Unchanged(second));
}

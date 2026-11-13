#include "prx/libSceAbstractYoutube/libSceAbstractYoutube.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libSceAbstractYoutube immediate returns check failed at line %u\n", location.line());
    std::abort();
}

constexpr int Error0005 = static_cast<int>(0x809E0005u);

} // namespace

int main() {
    Require(_ZNK15AbstractStorage14YoutubeContent7GetStatEPNS_4StatE(nullptr, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent4ReadEPvlPl(nullptr, nullptr, 0, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent8SeekTimeElPl(nullptr, 0, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent5FlushEv(nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeContent6RemoveEv(nullptr) == Error0005);
    Require(_ZNK15AbstractStorage14YoutubeContent10GetServiceEv(nullptr) == 2);
    Require(_ZNK15AbstractStorage14YoutubeContent13GetCapabilityEv(nullptr) == 0x6C0Au);
    Require(_ZNK15AbstractStorage13YoutubeFolder7GetStatEPNS_4StatE(nullptr, nullptr) == Error0005);
    Require(_ZN15AbstractStorage13YoutubeFolder6RemoveEb(nullptr, false) == Error0005);
    Require(_ZN15AbstractStorage13YoutubeFolder4OpenENS_8ItemTypeE() == Error0005);
    Require(_ZN15AbstractStorage13YoutubeFolder5CloseEv() == Error0005);
    Require(_ZNK15AbstractStorage13YoutubeFolder10GetServiceEv(nullptr) == 2);
    Require(_ZNK15AbstractStorage13YoutubeFolder13GetCapabilityEv(nullptr) == 0x1000000000ull);
    Require(_ZNK15AbstractStorage14YoutubeStorage7GetStatEPNS_4StatE(nullptr, nullptr) == Error0005);
    Require(_ZN15AbstractStorage14YoutubeStorage14GetStorageSizeEPlS1_S1_(nullptr, nullptr, nullptr, nullptr) == Error0005);
    Require(_ZNK15AbstractStorage14YoutubeStorage10GetServiceEv(nullptr) == 2);
    Require(_ZNK15AbstractStorage14YoutubeStorage13GetCapabilityEv(nullptr) == 0x180000000000000ull);
    Require(_ZNK15AbstractStorage14YoutubeService14GetServiceTypeEv(nullptr) == 2);
    return 0;
}

#include "prx/libSceIpmi/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceIpmi config estimator: %s\n", message);
    std::abort();
}

}

int main() {
    alignas(8) unsigned char config[0x40];
    std::memset(config, 0, sizeof(config));

    Require(_ZN4IPMI7Session6Config25estimateSessionMemorySizeEv(config) == 0, "Session::Config size is zero");
    Require(_ZN4IPMI6Client6Config24estimateClientMemorySizeEv(config) == 0x1000, "Client::Config size is 0x1000");

    const std::uint64_t first[] = {0x200, 0x300};
    Require(_ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(first) == 0x400, "Server::Config takes the larger first value");
    const std::uint64_t second[] = {0x500, 0x100};
    Require(_ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(second) == 0x600, "Server::Config takes the larger second value");

    alignas(8) unsigned char object[0x40];
    std::memset(object, 0x5a, sizeof(object));
    Require(_ZN4IPMI4impl10ClientImpl12tryGetResultEjPiPvPmm(object, 0, nullptr, nullptr, nullptr, 0) == 0,
            "ClientImpl::tryGetResult returns zero");
    Require(_ZN4IPMI4impl10ClientImpl17invokeAsyncMethodEjPKvmPjPKNS_6Client12EventNotifeeE(object, 0, nullptr, 0, nullptr, nullptr) == 0,
            "ClientImpl::invokeAsyncMethod returns zero");
    Require(_ZN4IPMI4impl11SessionImpl11tryDispatchEPvm(object, nullptr, 0) == 0, "SessionImpl::tryDispatch returns zero");
    for (std::size_t index = 0; index < sizeof(object); ++index) {
        Require(object[index] == 0x5a, "the query leaves the object untouched");
    }

    std::puts("libSceIpmi config estimator checks passed");
    return 0;
}

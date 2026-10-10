#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id);
int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction);
}

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80550003u);
constexpr int SignedOut = static_cast<int>(0x80550006u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpManager: %s\n", message);
        std::abort();
    }
}

}

int main() {
    NpId npId{};
    std::memset(&npId, 0x5a, sizeof(npId));
    NpId untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    Require(sceNpGetNpId(0x10000, &npId) == SignedOut, "sceNpGetNpId must report the user as signed out");
    Require(std::memcmp(&npId, &untouched, sizeof(npId)) == 0, "sceNpGetNpId must leave the NpId untouched");
    Require(sceNpGetNpId(0x10000, nullptr) == InvalidArgument, "sceNpGetNpId must reject a null NpId");
    NpContentRestriction restriction{};
    Require(sceNpSetContentRestriction(nullptr) == InvalidArgument, "sceNpSetContentRestriction must reject a null restriction");
    Require(sceNpSetContentRestriction(&restriction) == 0, "sceNpSetContentRestriction must accept a restriction");
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstring>

extern "C" {
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidArgument = static_cast<int>(0x80550003u);
constexpr int signedOut = static_cast<int>(0x80550006u);

const Case signedOutUser{"GetNpId_InitialUser_ReportsSignedOutAndLeavesNpIdUntouched", [] {
    NpId npId{};
    std::memset(&npId, 0x5a, sizeof(npId));
    NpId untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    RequireEqual(sceNpGetNpId(0x10000, &npId), signedOut, "sceNpGetNpId result");
    Require(std::memcmp(&npId, &untouched, sizeof(npId)) == 0, "sceNpGetNpId must leave the NpId untouched");
}};

const Case nullNpId{"GetNpId_NullNpId_ReturnsInvalidArgument", [] {
    RequireEqual(sceNpGetNpId(0x10000, nullptr), invalidArgument, "sceNpGetNpId with a null NpId");
}};

} // namespace

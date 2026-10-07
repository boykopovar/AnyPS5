#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpCreateRequest(void);
int APS5_VABI sceNpDeleteRequest(int req_id);
int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArgument = static_cast<int>(0x80550003);
constexpr int kErrSignedOut = static_cast<int>(0x80550006);

}

int main() {
    const int request = sceNpCreateRequest();
    Require(request > 0);
    const char onlineId[16] = "user";
    Require(sceNpCheckNpAvailability(request, nullptr, nullptr) == kErrInvalidArgument);
    Require(sceNpCheckNpAvailability(request, onlineId, nullptr) == kErrSignedOut);
    Require(sceNpDeleteRequest(request) == 0);
}

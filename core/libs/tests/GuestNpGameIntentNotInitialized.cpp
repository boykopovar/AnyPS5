#include "prx/libSceNpGameIntent/libSceNpGameIntent.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x80553802);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void CheckNotInitialized(const char* when) {
    Require(sceNpGameIntentGetPermittedIntentTypes() == NotInitialized, when);
    Require(sceNpGameIntentLaunchAppForPinvoke() == NotInitialized, when);
    Require(sceNpGameIntentRemoveLaunchCallback() == NotInitialized, when);
    Require(sceNpGameIntentRequest() == NotInitialized, when);
}

} // namespace

int main() {
    CheckNotInitialized("the four answer 0x80553802 before sceNpGameIntentInitialize");
    const unsigned char param[0x80] = {};
    Require(sceNpGameIntentInitialize(param) == 0, "sceNpGameIntentInitialize succeeds");
    Require(sceNpGameIntentTerminate() == 0, "sceNpGameIntentTerminate succeeds");
    CheckNotInitialized("the four answer 0x80553802 again after sceNpGameIntentTerminate");
    return 0;
}

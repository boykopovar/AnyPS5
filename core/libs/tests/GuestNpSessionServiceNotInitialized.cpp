#include "prx/libSceNpGameIntent/libSceNpGameIntent.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x8055ca02);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using Export = decltype(&sceNpSessionServiceTerminate);

const std::array<Export, 5> GatedExports = {
    sceNpSessionServiceGetJoinedPartySession,
    sceNpSessionServiceGetRunningGamePlayerSession,
    sceNpSessionServiceSendPlayerSessionInvitations,
    sceNpSessionServiceSendPlayerSessionRequestToJoin,
    sceNpSessionServiceTerminate
};

} // namespace

int main() {
    for (std::size_t i = 0; i < GatedExports.size(); ++i) {
        if (GatedExports[i]() == NotInitialized) continue;
        std::fprintf(stderr, "export %zu\n", i);
        Require(false, "an export answers 0x8055ca02 before initialization");
    }
    return 0;
}

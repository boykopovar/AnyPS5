#include "prx/libSceNpManager/libSceNpManager.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int AsmClientExtNotInitialized = static_cast<int>(0x8055CE07u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    for (const auto call : std::array<decltype(&sceNpAsmClientExtTerminate), 5>{
             sceNpAsmClientExtAbortRequest, sceNpAsmClientExtCreateRequest, sceNpAsmClientExtDeleteRequest,
             sceNpAsmClientExtGetPlayingTitleNpTitleToken, sceNpAsmClientExtTerminate}) {
        Require(call() == AsmClientExtNotInitialized, "an AsmClientExt call must report that the client is not initialized");
        Require(call() == AsmClientExtNotInitialized, "a second call must give the same answer");
    }
}

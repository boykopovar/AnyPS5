#include "prx/libSceNpTrophy/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    constexpr int DebugResult = static_cast<int>(0x80551692u);
    struct Call {
        const char* name;
        int (APS5_VABI* function)();
    };
    const Call calls[] = {
        {"sceNpTrophySystemDbgCtl", sceNpTrophySystemDbgCtl},
        {"sceNpTrophySystemGetDbgParam", sceNpTrophySystemGetDbgParam},
        {"sceNpTrophySystemGetDbgParamInt", sceNpTrophySystemGetDbgParamInt},
        {"sceNpTrophySystemSetDbgParam", sceNpTrophySystemSetDbgParam},
        {"sceNpTrophySystemSetDbgParamInt", sceNpTrophySystemSetDbgParamInt},
    };
    for (const Call& call : calls) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (call.function() != DebugResult) {
                std::fprintf(stderr, "%s did not return 0x80551692\n", call.name);
                return 1;
            }
        }
    }
    std::puts("NpTrophy system debug tests passed");
    return 0;
}

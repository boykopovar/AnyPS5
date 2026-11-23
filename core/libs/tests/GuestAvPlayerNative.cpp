#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/ExportLookup.hpp"
#include "tests/CommandLineArgument.hpp"
#include <cstdio>

int main(int argc, char** argv) {
    struct Export {
        void* function;
        const char* name;
    };
    const Export exports[] = {
        {reinterpret_cast<void*>(sceAvPlayerPrintf), "sceAvPlayerPrintf"},
        {reinterpret_cast<void*>(sceAvPlayerVprintf), "sceAvPlayerVprintf"},
    };
    if (!ExportsResolveByName(argc > 1 ? ArgumentPath(argv, 1) : std::filesystem::path(), exports, "sceAvPlayerNotExported")) return 1;
    std::puts("libSceAvPlayer.native pending exports are exported");
    return 0;
}

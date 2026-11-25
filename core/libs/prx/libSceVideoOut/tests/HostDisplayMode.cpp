#include "prx/libSceVideoOut/include/HostDisplayMode.hpp"
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    const std::string failure = "SDL_GetWindowDisplayIndex failed: Couldn't find any displays";
    bool threw = false;
    try {
        static_cast<void>(KeepHostDisplayMode(std::nullopt, std::nullopt, failure));
    } catch (const std::runtime_error& error) {
        threw = failure == error.what();
    }
    Require(threw, "a missing display before any mode was measured did not stop with the lookup failure");

    const HostDisplayMode measured{60, 3};
    const auto first = KeepHostDisplayMode(std::nullopt, measured, {});
    Require(first.refreshRate == 60 && first.resolution == 3, "the first measured mode was not taken");

    const auto kept = KeepHostDisplayMode(first, std::nullopt, failure);
    Require(kept.refreshRate == 60 && kept.resolution == 3, "the known mode was not kept while no display was found");

    const HostDisplayMode changed{120, 2};
    const auto updated = KeepHostDisplayMode(kept, changed, {});
    Require(updated.refreshRate == 120 && updated.resolution == 2, "a new measurement did not replace the known mode");

    std::puts("video_out_host_display_mode: ok");
    return 0;
}

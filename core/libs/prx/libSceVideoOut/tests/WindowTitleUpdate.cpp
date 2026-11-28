#include "prx/libSceVideoOut/include/WindowTitleUpdate.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    Require(WindowTitleUpdate::ShouldPush(1, false), "first flip must push the initial title");
    Require(WindowTitleUpdate::ShouldPush(2, true), "a closed fps window must push the title");
    Require(!WindowTitleUpdate::ShouldPush(2, false), "a flip without a closed fps window must not push the title");
    Require(!WindowTitleUpdate::ShouldPush(60, false), "steady-state flips must not push the title");
    Require(WindowTitleUpdate::ShouldPush(121, true), "an fps window closed on a later flip must push the title");
    return 0;
}

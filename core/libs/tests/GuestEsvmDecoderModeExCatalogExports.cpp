#include "prx/libSceEsvmDecoderModeEx/libSceEsvmDecoderModeEx.hpp"
#include <cstdio>
#include <stdexcept>

namespace {

int failures = 0;

template <typename TFn>
void RequireThrows(TFn fn, const char *name) {
    try {
        fn();
    } catch (const std::runtime_error &) {
        return;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    ++failures;
}

}

int main() {
    RequireThrows(sceEsvmUseCustomMusicCoreForDecoderMode_nid_no_patch, "sceEsvmUseCustomMusicCoreForDecoderMode");
    return failures == 0 ? 0 : 1;
}

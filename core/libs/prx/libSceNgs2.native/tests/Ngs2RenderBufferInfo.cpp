#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <vector>

int main() {
    const auto invalidBufferInfo = reinterpret_cast<const Ngs2RenderBufferInfo*>(std::uintptr_t{0x10});
    Require(sceNgs2SystemRender(0x1234, invalidBufferInfo, 1) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);

    const auto system = CreateSystem();
    const int savedErrno = errno;
    errno = 73;
    bool invalidInput = false;
    try {
        static_cast<void>(sceNgs2SystemRender(system, invalidBufferInfo, 1));
    } catch (const std::invalid_argument&) {
        invalidInput = true;
    }
    Require(invalidInput && errno == 73);
    errno = savedErrno;

    std::vector<float> output(Grain, 123.0f);
    const Ngs2RenderBufferInfo info{output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

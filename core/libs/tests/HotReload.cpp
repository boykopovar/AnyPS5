#include <cstdlib>
#include "HotReload.hpp"
#include "general/VabiMacros.hpp"

namespace {

int OriginalAdd(int a, int b) {
    return a + b;
}

int ReplacementMultiply(int a, int b) {
    return a * b;
}

int ReplacementSubtract(int a, int b) {
    return a - b;
}

int APS5_VABI SysVOriginal(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

int APS5_VABI SysVReplacement(int a, int b, int c, int d, int e, int f) {
    return (a * 1) + (b * 2) + (c * 3) + (d * 4) + (e * 5) + (f * 6);
}

} // namespace

int main() {
    if (OriginalAdd(3, 4) != 7) std::abort();

    if (!HotReload::PatchFunction(reinterpret_cast<void*>(&OriginalAdd), reinterpret_cast<void*>(&ReplacementMultiply))) {
        std::abort();
    }

    if (OriginalAdd(3, 4) != 12) std::abort();
    if (OriginalAdd(5, 6) != 30) std::abort();

    if (!HotReload::PatchFunction(reinterpret_cast<void*>(&OriginalAdd), reinterpret_cast<void*>(&ReplacementSubtract))) {
        std::abort();
    }

    if (OriginalAdd(10, 4) != 6) std::abort();

    if (!HotReload::RestoreFunction(reinterpret_cast<void*>(&OriginalAdd))) {
        std::abort();
    }

    if (OriginalAdd(3, 4) != 7) std::abort();

    if (SysVOriginal(1, 2, 3, 4, 5, 6) != 21) std::abort();

    if (!HotReload::PatchFunction(reinterpret_cast<void*>(&SysVOriginal), reinterpret_cast<void*>(&SysVReplacement))) {
        std::abort();
    }

    const int expectedSysV = (1 * 1) + (2 * 2) + (3 * 3) + (4 * 4) + (5 * 5) + (6 * 6);
    if (SysVOriginal(1, 2, 3, 4, 5, 6) != expectedSysV) std::abort();

    if (!HotReload::RestoreFunction(reinterpret_cast<void*>(&SysVOriginal))) {
        std::abort();
    }

    if (SysVOriginal(1, 2, 3, 4, 5, 6) != 21) std::abort();

    HotReload::SetEnabled(false);
    if (HotReload::IsEnabled()) std::abort();
    HotReload::SetEnabled(true);
    if (!HotReload::IsEnabled()) std::abort();

    if (HotReload::IsShaderReloadTriggered()) std::abort();
    HotReload::TriggerShaderReload();
    if (!HotReload::IsShaderReloadTriggered()) std::abort();
    if (HotReload::IsShaderReloadTriggered()) std::abort();

    HotReload::RenderStateOverride overrideState;
    overrideState.forceWireframe = true;
    overrideState.disableCull = true;
    overrideState.disableDepthTest = true;
    overrideState.forceDepthWrite = true;
    overrideState.disableBlend = true;
    overrideState.fallbackMagenta = true;
    overrideState.disableDcc = true;
    HotReload::SetRenderStateOverride(overrideState);

    const auto currentOverride = HotReload::GetRenderStateOverride();
    if (!currentOverride.forceWireframe) std::abort();
    if (!currentOverride.disableCull) std::abort();
    if (!currentOverride.disableDepthTest) std::abort();
    if (!currentOverride.forceDepthWrite) std::abort();
    if (!currentOverride.disableBlend) std::abort();
    if (!currentOverride.fallbackMagenta) std::abort();
    if (!currentOverride.disableDcc) std::abort();

    const std::filesystem::path testHotreloadDir("hotreload");
    std::filesystem::create_directories(testHotreloadDir);
    const std::filesystem::path testRenderStateFile = testHotreloadDir / "render_state.txt";
    FILE* f = std::fopen(testRenderStateFile.string().c_str(), "w");
    if (f != nullptr) {
        std::fprintf(f, "force_wireframe=0\n");
        std::fprintf(f, "disable_cull=1\n");
        std::fprintf(f, "disable_depth_test=0\n");
        std::fprintf(f, "disable_blend=1\n");
        std::fprintf(f, "fallback_magenta=1\n");
        std::fclose(f);
    }

    if (!HotReload::CheckRenderStateReload()) std::abort();
    const auto fileOverride = HotReload::GetRenderStateOverride();
    if (fileOverride.forceWireframe) std::abort();
    if (!fileOverride.disableCull) std::abort();
    if (fileOverride.disableDepthTest) std::abort();
    if (!fileOverride.disableBlend) std::abort();
    if (!fileOverride.fallbackMagenta) std::abort();

    std::filesystem::remove(testRenderStateFile);

    std::uint8_t buffer[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    const std::uint8_t rawBytes[4] = {0x01, 0x02, 0x03, 0x04};
    if (!HotReload::ApplyRawPatch(buffer, rawBytes, sizeof(rawBytes))) std::abort();
    if (buffer[0] != 0x01 || buffer[1] != 0x02 || buffer[2] != 0x03 || buffer[3] != 0x04) std::abort();
    if (buffer[4] != 0x90) std::abort();

    return 0;
}


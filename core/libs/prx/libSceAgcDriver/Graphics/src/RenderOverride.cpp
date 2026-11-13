#include "prx/libSceAgcDriver/Graphics/include/RenderOverride.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace AgcDriver::Graphics {
namespace {

std::mutex overrideMutex;
RenderOverride renderOverride;
std::filesystem::file_time_type lastOverrideWrite{};
std::atomic<bool> shaderReload{false};

void applyLine(RenderOverride& state, const char* line) {
    char key[128];
    int value = 0;
    if (std::sscanf(line, "%127[^=]=%d", key, &value) != 2) return;
    char* trimmed = key;
    while (*trimmed == ' ' || *trimmed == '\t') ++trimmed;
    std::size_t length = std::strlen(trimmed);
    while (length > 0 && (trimmed[length - 1] == ' ' || trimmed[length - 1] == '\t' || trimmed[length - 1] == '\r' || trimmed[length - 1] == '\n')) trimmed[--length] = '\0';
    const bool enabled = value != 0;
    if (std::strcmp(trimmed, "force_wireframe") == 0) state.forceWireframe = enabled;
    else if (std::strcmp(trimmed, "disable_cull") == 0) state.disableCull = enabled;
    else if (std::strcmp(trimmed, "disable_depth_test") == 0) state.disableDepthTest = enabled;
    else if (std::strcmp(trimmed, "force_depth_write") == 0) state.forceDepthWrite = enabled;
    else if (std::strcmp(trimmed, "disable_blend") == 0 || std::strcmp(trimmed, "disable_blending") == 0) state.disableBlend = enabled;
    else if (std::strcmp(trimmed, "disable_dcc") == 0) state.disableDcc = enabled;
}

}

bool RenderOverrideEnabled() {
    static const bool enabled = RenderOverrideRequested(std::getenv("APS5_HOT_RELOAD"));
    return enabled;
}

RenderOverride CurrentRenderOverride() {
    if (!RenderOverrideEnabled()) return {};
    std::lock_guard lock(overrideMutex);
    return renderOverride;
}

bool PollRenderOverride() {
    if (!RenderOverrideEnabled()) return false;
    const std::filesystem::path path("hotreload/render_state.txt");
    std::error_code error;
    if (!std::filesystem::exists(path, error)) return false;
    const auto written = std::filesystem::last_write_time(path, error);
    if (error) return false;
    {
        std::lock_guard lock(overrideMutex);
        if (written == lastOverrideWrite) return false;
        lastOverrideWrite = written;
    }
    FILE* file = std::fopen(path.string().c_str(), "r");
    if (file == nullptr) return false;
    RenderOverride next;
    char line[256];
    while (std::fgets(line, sizeof(line), file) != nullptr) applyLine(next, line);
    std::fclose(file);
    {
        std::lock_guard lock(overrideMutex);
        renderOverride = next;
    }
    std::fprintf(stderr, "[HOT-RELOAD] Render state updated from %s:\n", path.string().c_str());
    std::fprintf(stderr, "  force_wireframe=%d disable_cull=%d disable_depth_test=%d force_depth_write=%d disable_blend=%d disable_dcc=%d\n",
        next.forceWireframe, next.disableCull, next.disableDepthTest, next.forceDepthWrite, next.disableBlend, next.disableDcc);
    return true;
}

void TriggerShaderReload() {
    shaderReload.store(true);
}

bool ConsumeShaderReload() {
    return shaderReload.exchange(false);
}

} // namespace AgcDriver::Graphics

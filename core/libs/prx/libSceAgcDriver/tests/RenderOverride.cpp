#include "prx/libSceAgcDriver/Graphics/include/RenderOverride.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdlib.h>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void EnableHotReload() {
#ifdef _WIN32
    _putenv_s("APS5_HOT_RELOAD", "1");
#else
    setenv("APS5_HOT_RELOAD", "1", 1);
#endif
}

void WriteState(const std::filesystem::path& path, const char* text, std::filesystem::file_time_type stamp) {
    {
        std::ofstream file(path, std::ios::trunc);
        Require(static_cast<bool>(file));
        file << text;
    }
    std::filesystem::last_write_time(path, stamp);
}

}

int main() {
    Require(!AgcDriver::Graphics::RenderOverrideRequested(nullptr));
    Require(!AgcDriver::Graphics::RenderOverrideRequested(""));
    Require(!AgcDriver::Graphics::RenderOverrideRequested("0"));
    Require(AgcDriver::Graphics::RenderOverrideRequested("1"));
    EnableHotReload();
    Require(AgcDriver::Graphics::RenderOverrideEnabled());
    Require(!AgcDriver::Graphics::CurrentRenderOverride().Any());
    Require(!AgcDriver::Graphics::PollRenderOverride());
    const auto root = std::filesystem::temp_directory_path() / "anyps5-render-override";
    std::filesystem::remove_all(root);
    const auto directory = root / "hotreload";
    std::filesystem::create_directories(directory);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    const auto path = directory / "render_state.txt";
    const auto first = std::filesystem::file_time_type::clock::now();
    WriteState(path, "force_wireframe=1\ndisable_cull=1\ndisable_blend=1\ndisable_dcc=1\nunknown=1\n", first);
    Require(AgcDriver::Graphics::PollRenderOverride());
    const auto loaded = AgcDriver::Graphics::CurrentRenderOverride();
    Require(loaded.forceWireframe && loaded.disableCull && loaded.disableBlend && loaded.disableDcc);
    Require(!loaded.disableDepthTest && !loaded.forceDepthWrite);
    Require(loaded.Any() && loaded.ChangesDrawState());
    Require(!AgcDriver::Graphics::PollRenderOverride());
    WriteState(path, "disable_depth_test=1\nforce_depth_write=1\ndisable_blend=0\n", first + std::chrono::seconds(2));
    Require(AgcDriver::Graphics::PollRenderOverride());
    const auto replaced = AgcDriver::Graphics::CurrentRenderOverride();
    Require(!replaced.forceWireframe && !replaced.disableCull && !replaced.disableBlend && !replaced.disableDcc);
    Require(replaced.disableDepthTest && replaced.forceDepthWrite && replaced.ChangesDrawState());
    std::filesystem::remove(path);
    Require(!AgcDriver::Graphics::PollRenderOverride());
    Require(AgcDriver::Graphics::CurrentRenderOverride().disableDepthTest);
    WriteState(path, "force_opaque_alpha=1\nfallback_magenta=1\n", first + std::chrono::seconds(4));
    Require(AgcDriver::Graphics::PollRenderOverride());
    Require(!AgcDriver::Graphics::CurrentRenderOverride().Any());
    Require(!AgcDriver::Graphics::ConsumeShaderReload());
    AgcDriver::Graphics::TriggerShaderReload();
    Require(AgcDriver::Graphics::ConsumeShaderReload());
    Require(!AgcDriver::Graphics::ConsumeShaderReload());
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return 0;
}

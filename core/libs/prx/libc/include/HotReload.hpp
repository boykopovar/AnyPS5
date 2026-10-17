#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOTRELOAD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOTRELOAD_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace HotReload {

bool IsEnabled();
void SetEnabled(bool enabled);
bool PatchFunction(void* target, void* replacement);
void* GetOriginalFunction(void* target);
bool RestoreFunction(void* target);
bool ApplyRawPatch(void* target, const void* bytes, std::size_t size);
std::size_t CheckRawPatches();
std::size_t LoadPatchModules(const std::filesystem::path& patchDirectory);
bool HandleNotImplemented(const char* funcName, void* callerAddress);
struct RenderStateOverride {
    bool forceWireframe = false;
    bool disableCull = false;
    bool disableDepthTest = false;
    bool forceDepthWrite = false;
    bool disableBlend = false;
    bool fallbackMagenta = false;
    bool disableDcc = false;
};

void TriggerShaderReload();
bool IsShaderReloadTriggered();

RenderStateOverride GetRenderStateOverride();
void SetRenderStateOverride(const RenderStateOverride& overrideState);
bool CheckRenderStateReload();

} // namespace HotReload

extern "C" {
bool HotReloadIsShaderReloadTriggered_nid_no_patch();
HotReload::RenderStateOverride HotReloadGetRenderStateOverride_nid_no_patch();
bool HotReloadCheckRenderStateReload_nid_no_patch();
}

namespace HotReload {

inline bool IsShaderReloadTriggeredExport() {
    return HotReloadIsShaderReloadTriggered_nid_no_patch();
}

inline RenderStateOverride GetRenderStateOverrideExport() {
    return HotReloadGetRenderStateOverride_nid_no_patch();
}

inline bool CheckRenderStateReloadExport() {
    return HotReloadCheckRenderStateReload_nid_no_patch();
}

} // namespace HotReload

#endif



#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDEROVERRIDE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RENDEROVERRIDE_HPP

#include <cstring>

namespace AgcDriver::Graphics {

struct RenderOverride {
    bool forceWireframe = false;
    bool disableCull = false;
    bool disableDepthTest = false;
    bool forceDepthWrite = false;
    bool disableBlend = false;
    bool disableDcc = false;

    bool Any() const {
        return forceWireframe || disableCull || disableDepthTest || forceDepthWrite || disableBlend || disableDcc;
    }

    bool ChangesDrawState() const {
        return disableCull || disableDepthTest || forceDepthWrite || disableBlend;
    }
};

inline bool RenderOverrideRequested(const char* value) {
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

bool RenderOverrideEnabled();
RenderOverride CurrentRenderOverride();
bool PollRenderOverride();
void TriggerShaderReload();
bool ConsumeShaderReload();

} // namespace AgcDriver::Graphics

#endif

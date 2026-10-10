#include "SDL_config.h"

#include <Testing/Test.hpp>

namespace {

const Testing::Case hidapi{"SdlBuild_Joysticks_UseHidapi", [] {
#ifdef SDL_JOYSTICK_HIDAPI
    Testing::Require(true, "SDL drives controllers through HIDAPI");
#else
    Testing::Fail("SDL is built without HIDAPI, so a DualSense gets no light bar, touchpad or trigger effects");
#endif
}};

} // namespace

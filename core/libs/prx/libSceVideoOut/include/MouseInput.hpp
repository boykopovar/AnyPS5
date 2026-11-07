#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_MOUSEINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_MOUSEINPUT_HPP

#include "SDL_events.h"
#include <cstdint>

class MouseInput {
public:
    void HandleEvent(const SDL_Event& event, unsigned windowId);
    void SetPresentation(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t presentedWidth, std::uint32_t presentedHeight);
    bool CaptureWanted(bool allowed) const;
    void UpdateCapture(bool allowed);

private:
    std::int32_t scaled(std::int32_t delta, double scale, double& remainder);

    bool focused = true;
    bool captured = false;
    bool suspended = false;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double remainderX = 0.0;
    double remainderY = 0.0;
};

#endif

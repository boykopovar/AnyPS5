#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_DISPLAYWINDOW_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_DISPLAYWINDOW_HPP

#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include "SDL.h"

inline constexpr std::uint32_t DisplayWindowMinimumWidth = 320;
inline constexpr std::uint32_t DisplayWindowMinimumHeight = 180;
inline constexpr std::uint32_t DisplayWindowInitialSizePercent = 60;

namespace frame_watchdog {

void Beat() noexcept;
bool ParseHangSeconds(const char* text, std::uint32_t& seconds, std::string& error);

struct HangState {
    std::int64_t beat = 0;
    bool reported = false;
};

bool HangReportDue(HangState& state, std::int64_t beat, std::int64_t now, std::int64_t thresholdNanoseconds);
void Start(std::uint32_t seconds);
void Stop() noexcept;
void StartFromEnvironment();

}

class DisplayWindow {
public:
    DisplayWindow() = default;
    ~DisplayWindow();
    DisplayWindow(const DisplayWindow&) = delete;
    DisplayWindow& operator=(const DisplayWindow&) = delete;

    void Ensure(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
    void Destroy() noexcept;
    SDL_Window* Handle() const;
    void DrawableSize(std::uint32_t& width, std::uint32_t& height) const;
    void UpdateTitle();
    std::uint64_t FrameNumber() const { return frameNum; }
    void ToggleFullscreen();

private:
    void create(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
    void updateAspectRatio(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
    void installSubclass();
    void removeSubclass() noexcept;
    void applyAspectRatio(void* hwnd, std::uintptr_t edge, void* rect) const;

    static std::intptr_t windowProc(void* hwnd, unsigned int message, std::uintptr_t wParam, std::intptr_t lParam, std::uintptr_t subclassId, std::uintptr_t referenceData);

    SDL_Window* window = nullptr;
    std::unique_ptr<IHostFullscreenWindow> hostWindow;
    std::unique_ptr<HostFullscreenState> fullscreen;
    std::uint32_t aspectWidth = 0;
    std::uint32_t aspectHeight = 0;
    std::uint64_t frameNum = 0;
};

#endif

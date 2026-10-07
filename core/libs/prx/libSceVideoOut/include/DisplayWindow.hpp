#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_DISPLAYWINDOW_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_DISPLAYWINDOW_HPP

#include <cstdint>
#include "SDL.h"

inline constexpr std::uint32_t DisplayWindowMinimumWidth = 320;
inline constexpr std::uint32_t DisplayWindowMinimumHeight = 180;
inline constexpr std::uint32_t DisplayWindowInitialSizePercent = 60;

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
    void ToggleFullscreen();

private:
    void create(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
    void updateAspectRatio(std::uint32_t sourceWidth, std::uint32_t sourceHeight);
    void installSubclass();
    void removeSubclass() noexcept;
    void applyAspectRatio(void* hwnd, std::uintptr_t edge, void* rect) const;

    static std::intptr_t windowProc(void* hwnd, unsigned int message, std::uintptr_t wParam, std::intptr_t lParam,
                                    std::uintptr_t subclassId, std::uintptr_t referenceData);

    SDL_Window* window = nullptr;
    std::uint32_t aspectWidth = 0;
    std::uint32_t aspectHeight = 0;
};

#endif

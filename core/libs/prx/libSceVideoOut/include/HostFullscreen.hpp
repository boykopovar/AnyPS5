#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTFULLSCREEN_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTFULLSCREEN_HPP

enum class HostFullscreenMode {
    Windowed,
    Borderless,
};

HostFullscreenMode ParseHostFullscreenMode(const char* value, bool* unrecognized = nullptr);

struct HostWindowSize {
    int width = 0;
    int height = 0;
    friend bool operator==(const HostWindowSize&, const HostWindowSize&) = default;
};

class IHostFullscreenWindow {
public:
    virtual ~IHostFullscreenWindow() = default;
    virtual bool IsFullscreen() const = 0;
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual HostWindowSize Size() const = 0;
    virtual void SetSize(HostWindowSize size) = 0;
    virtual void ShowCursor(bool show) = 0;
};

class HostFullscreenState {
public:
    explicit HostFullscreenState(IHostFullscreenWindow& window);

    void Adopt(HostFullscreenMode mode, HostWindowSize windowedSize);
    void Toggle();
    void Refresh(bool titleReadsMouse);
    void Release();

private:
    IHostFullscreenWindow& window;
    HostWindowSize windowedSize;
    bool cursorHidden = false;
};

#endif

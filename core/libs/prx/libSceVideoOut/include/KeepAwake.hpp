#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_KEEPAWAKE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_KEEPAWAKE_HPP

bool KeepAwakeEnabledFromValue(const char* value);
bool KeepAwakeEnabled();

struct KeepAwakeBackend {
    void (*disable)();
    void (*enable)();
    bool (*isEnabled)();
};

class HostKeepAwake {
public:
    explicit HostKeepAwake(const KeepAwakeBackend& backend);
    void Acquire();
    void Release() noexcept;

private:
    const KeepAwakeBackend& backend;
    bool held = false;
};

const KeepAwakeBackend& SdlKeepAwakeBackend();

#endif

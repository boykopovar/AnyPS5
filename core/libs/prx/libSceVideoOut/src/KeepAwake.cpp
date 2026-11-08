#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include <cstring>

bool KeepAwakeEnabledFromValue(const char* value) {
    return value == nullptr || std::strcmp(value, "0") != 0;
}

HostKeepAwake::HostKeepAwake(const KeepAwakeBackend& backend) : backend(backend) {
}

void HostKeepAwake::Acquire() {
    if (held) return;
    backend.disable();
    held = true;
}

void HostKeepAwake::Release() noexcept {
    if (!held) return;
    backend.enable();
    held = false;
}

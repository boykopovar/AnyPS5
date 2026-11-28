#ifndef CORE_LIBS_PRX_LIBSCEPAD_TOGGLELATCH_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_TOGGLELATCH_HPP

#include <cstdint>

namespace Pad {

class ToggleLatch {
public:
    void Toggle(std::uint32_t buttons) {
        latched ^= buttons;
    }

    bool Held(std::uint32_t buttons) const {
        return (latched & buttons) != 0;
    }

    void Clear() {
        latched = 0;
    }

private:
    std::uint32_t latched = 0;
};

}

#endif

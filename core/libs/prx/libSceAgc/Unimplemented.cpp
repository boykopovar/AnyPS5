#include "prx/libSceAgc/Acb/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include <algorithm>
#include <limits>

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbInitializeDefaultHardwareState(CommandBuffer* buffer) {
    const auto commands = Agc::Command::GetDefaultComputeCommands();
    if (commands.empty()) return nullptr;
    Agc::Command::Require(buffer != nullptr, __func__, "null command buffer");
    const auto count = static_cast<std::uint32_t>(commands.size());
    const auto available = [&] {
        const auto up = reinterpret_cast<std::uintptr_t>(buffer->cursor_up);
        const auto down = reinterpret_cast<std::uintptr_t>(buffer->cursor_down);
        Agc::Command::Require(((up | down) & 3u) == 0 && up <= down, __func__, "invalid command buffer cursors");
        const auto words = (down - up) / sizeof(std::uint32_t);
        return words >= buffer->reserved_dw ? words - buffer->reserved_dw : 0;
    };
    if (available() < count) {
        if (buffer->callback == nullptr || count > std::numeric_limits<std::uint32_t>::max() - buffer->reserved_dw) return nullptr;
        if (!buffer->callback(buffer, count + buffer->reserved_dw, buffer->user_data) || available() < count) return nullptr;
    }
    auto* output = Agc::Command::Allocate(buffer, count, __func__);
    std::copy(commands.begin(), commands.end(), output);
    return output;
}

}

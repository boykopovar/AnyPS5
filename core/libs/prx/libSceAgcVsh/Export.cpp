#include "prx/libSceAgcVsh/include/Api.hpp"

#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace {

std::uint32_t StreamAvailable(const CommandBuffer* buffer) {
    const auto dwords = static_cast<std::uint64_t>(
        reinterpret_cast<const std::byte*>(buffer->cursor_down) - reinterpret_cast<const std::byte*>(buffer->cursor_up)) /
        sizeof(std::uint32_t);
    return dwords > buffer->reserved_dw ? static_cast<std::uint32_t>(dwords - buffer->reserved_dw) : 0;
}

bool StreamEnsure(CommandBuffer* buffer, std::uint32_t need) {
    if (StreamAvailable(buffer) >= need) return true;
    return buffer->callback != nullptr && buffer->callback(buffer, buffer->reserved_dw + need, buffer->user_data) &&
           StreamAvailable(buffer) >= need;
}

std::uint32_t* StreamTake(CommandBuffer* buffer, std::uint32_t count) {
    auto* packet = reinterpret_cast<std::uint32_t*>(
        (reinterpret_cast<std::uintptr_t>(buffer->cursor_up) + 3u) & ~static_cast<std::uintptr_t>(3u));
    buffer->cursor_up = packet + count;
    if (packet == nullptr) return nullptr;
    AgcDriverPrepareCommandWrite_nid_no_patch(reinterpret_cast<std::uint64_t>(packet), count * sizeof(std::uint32_t));
    return packet;
}

}

extern "C" {

int APS5_VABI sceAgcSuspendPointAndCheckStatus_nid_no_patch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("b+fis+WZ3Ig", sceAgcSuspendPointAndCheckStatus_nid_no_patch);

APS5_EXPORT("Aozdh0bCcO0", sceAgcVshUnknown_Aozdh0bCcO0);
std::uint32_t* APS5_VABI sceAgcVshUnknown_Aozdh0bCcO0(CommandBuffer* buffer) {
    if (buffer == nullptr) return nullptr;
    auto emit = [buffer](std::uint32_t need, std::initializer_list<std::uint32_t> dwords) {
        if (!StreamEnsure(buffer, need)) return;
        auto* packet = StreamTake(buffer, static_cast<std::uint32_t>(dwords.size()));
        if (packet != nullptr) std::copy(dwords.begin(), dwords.end(), packet);
    };
    if (!StreamEnsure(buffer, 0x27)) return nullptr;
    auto* first = StreamTake(buffer, 5);
    if (first != nullptr) {
        first[0] = 0xc0032200u;
        first[1] = 0xf8110000u;
        first[2] = 0x0000000fu;
        first[3] = 0x00000000u;
        first[4] = 0x22000000u;
    }
    emit(2, {0xc0001200u, 0x00000002u});
    emit(5, {0xc0033700u, 0x038f048eu, 0x070cc102u, 0x00000008u, 0x00000000u});
    emit(6, {0xc0044000u, 0x038f048eu, 0x0c02b303u, 0x0c410807u, 0x00001006u, 0x0000000fu});
    emit(6, {0xc0044000u, 0x100e4100u, 0x0d430286u, 0x07834d06u, 0x058d068cu, 0x0000000fu});
    emit(6, {0xc0044000u, 0x0000002cu, 0x00000624u, 0xffff0458u, 0x000002ccu, 0x0000000fu});
    emit(9, {0xc0071e00u, 0x100e4100u, 0x0d430286u, 0x07834d06u, 0x058d068cu, 0x038f048eu, 0x0c02a803u, 0x0c410807u, 0x00001006u});
    return first;
}

}

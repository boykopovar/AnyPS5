#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace AgcDriver::DriverDetail {

void Driver::noteLabelStore(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue) {
    if (bytes.empty() || bytes.size() > 64 || bytes.size() % 4 != 0 || address % 4 != 0) return;
    std::lock_guard lock(labelStoresMutex);
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        auto& history = labelStores[address + offset];
        std::shift_right(history.begin(), history.end(), 1);
        history[0].stamp = stamp;
        history[0].queue = queue;
        std::memcpy(&history[0].value, bytes.data() + offset, 4);
    }
}

bool Driver::storedSince(std::span<const std::uint32_t> packet, std::uint64_t address, std::size_t bytes, std::uint64_t received, std::uint32_t* writer) {
    std::lock_guard lock(labelStoresMutex);
    const auto low = labelStores.find(address);
    if (low == labelStores.end()) return false;
    const auto high = bytes == 8 ? labelStores.find(address + 4) : labelStores.end();
    if (bytes == 8 && high == labelStores.end()) return false;
    for (const auto& store : low->second) {
        if (store.stamp <= received) continue;
        std::uint64_t value = store.value;
        if (bytes == 8) {
            const auto upper = std::find_if(high->second.begin(), high->second.end(), [&](const LabelStore& other) { return other.stamp == store.stamp; });
            if (upper == high->second.end()) continue;
            value |= static_cast<std::uint64_t>(upper->value) << 32u;
        }
        if (Pm4::WaitComparesValue(packet, value)) {
            if (writer != nullptr) *writer = store.queue;
            return true;
        }
    }
    return false;
}

void Driver::traceLabel(std::span<const std::uint32_t> packet, std::uint32_t queue) {
    static const std::uint64_t watched = [] {
        const char* value = std::getenv("APS5_TRACE_LABEL");
        return value ? std::strtoull(value, nullptr, 16) : 0ull;
    }();
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    std::uint64_t target = 0;
    const char* kind = nullptr;
    if (opcode == 0x49 && packet.size() >= 7) { target = packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u); kind = "RELEASE_MEM"; }
    else if (opcode == 0x37 && packet.size() >= 5) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WRITE_DATA"; }
    else if ((opcode == 0x3c || opcode == 0x93) && packet.size() >= 7) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WAIT_REG_MEM"; }
    else if (opcode == 0x40 && packet.size() >= 6) { target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u); kind = "COPY_DATA"; }
    std::uint64_t length = 4;
    if (opcode == 0x50 && packet.size() >= 7) {
        target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u);
        length = packet[6] & 0x3ffffffu;
        kind = "DMA_DATA";
    }
    if (kind == nullptr) return;
    if (std::string_view(kind) != "WAIT_REG_MEM") {
        static std::mutex historyMutex;
        std::lock_guard lock(historyMutex);
        auto& entry = writeHistory()[writeCursor()++ % writeHistory().size()];
        entry = {target, length, queue, opcode};
    }
    if (watched == 0 || target + length + 0x100 < watched || target > watched + 0x100) return;
    std::fprintf(stderr, "[label] %lld ms queue 0x%x %s 0x%llx:", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), queue, kind, static_cast<unsigned long long>(target));
    for (std::size_t i = 1; i < packet.size() && i < 9; ++i) std::fprintf(stderr, " %08x", packet[i]);
    std::fprintf(stderr, "\n");
}

std::array<WriteRecord, 16384>& Driver::writeHistory() {
    static std::array<WriteRecord, 16384> history{};
    return history;
}

std::size_t& Driver::writeCursor() {
    static std::size_t cursor = 0;
    return cursor;
}

}

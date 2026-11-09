#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/PacketHistory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"

namespace AgcDriver::DriverDetail {

void PacketHistory::Print(std::FILE* file, const char* format) const {
    const auto shown = std::min(count, offsets.size());
    const auto first = count > shown ? count % offsets.size() : 0;
    for (std::size_t i = 0; i < shown; ++i) std::fprintf(file, format, Format(offsets[(first + i) % offsets.size()]).c_str());
}

void PacketHistory::Record(std::size_t offset) {
    offsets[count % offsets.size()] = offset;
    ++count;
}

std::string PacketHistory::Format(std::size_t offset) const {
    const auto header = commands[offset];
    const auto words = std::min(Pm4::PacketWords(header), commands.size() - offset);
    char line[160];
    int length = std::snprintf(line, sizeof(line), "%s", Pm4::Name(header).c_str());
    for (std::size_t i = 1; i < words && i < 9 && length < 140; ++i) length += std::snprintf(line + length, sizeof(line) - length, " %08x", commands[offset + i]);
    return line;
}

}

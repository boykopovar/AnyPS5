#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <cstdlib>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

void Driver::dumpPackets(std::span<const std::uint32_t> commands, const std::uint32_t* guest) {
    std::map<std::string, std::pair<std::size_t, std::string>> summary;
    std::vector<std::pair<std::size_t, std::uint32_t>> walk;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) { ++cursor; continue; }
        if ((header & 0xc0000000u) != 0xc0000000u) break;
        const auto count = Pm4::PacketWords(header);
        if (count > commands.size() - cursor) break;
        walk.emplace_back(cursor, header);
        auto& entry = summary[Pm4::Name(header)];
        if (entry.first++ == 0) {
            try {
                Pm4::Validate(commands.subspan(cursor, count), 0);
            } catch (const std::exception& error) {
                entry.second = error.what();
            }
        }
        cursor += count;
    }
    std::fprintf(stderr, "[gpu] rejected submission of %zu dwords at guest %p:\n", commands.size(), static_cast<const void*>(guest));
    for (const auto& [name, entry] : summary) std::fprintf(stderr, "[gpu]   %-28s x%-5zu %s\n", name.c_str(), entry.first, entry.second.c_str());

    for (std::size_t i = walk.size() > 24 ? walk.size() - 24 : 0; i < walk.size(); ++i) {
        const auto [offset, header] = walk[i];
        std::fprintf(stderr, "[gpu]   at DWORD %-6zu header 0x%08x %-24s %zu dwords:", offset, header, Pm4::Name(header).c_str(), Pm4::PacketWords(header));
        for (std::size_t j = offset + 1; j < std::min(commands.size(), offset + std::min<std::size_t>(Pm4::PacketWords(header), 12)); ++j) std::fprintf(stderr, " %08x", commands[j]);
        std::fprintf(stderr, "\n");
    }
    static const bool dumpRejected = std::getenv("APS5_DUMP_REJECTED") != nullptr;
    if (!dumpRejected) return;
    static int dumps = 0;
    char fileName[64];
    std::snprintf(fileName, sizeof(fileName), "rejected_submission_%d.bin", dumps++);
    if (FILE* file = std::fopen(fileName, "wb")) {
        std::fwrite(commands.data(), sizeof(std::uint32_t), commands.size(), file);
        std::fclose(file);
        std::fprintf(stderr, "[gpu] rejected submission written to %s\n", fileName);
    }
}

void Driver::validate(const Submission& submission, const std::uint32_t* guest) {
    const std::span<const std::uint32_t> commands = submission.commands;
    const auto queue = submission.queue;
    std::vector<std::size_t> guarded;
    for (std::size_t cursor = 0; cursor < commands.size();) {
        std::erase_if(guarded, [cursor](std::size_t end) { return end <= cursor; });
        const auto header = commands[cursor];
        if (Pm4::FillerPacket(header)) { ++cursor; continue; }
        if ((header & 0xc0000000u) != 0xc0000000u) {
            char what[96];
            std::snprintf(what, sizeof(what), "unsupported PM4 packet type: header 0x%08x at DWORD %zu of %zu", header, cursor, commands.size());

            dumpPackets(commands, guest);
            std::fprintf(stderr, "[gpu] dwords %zu..%zu:", cursor >= 8 ? cursor - 8 : 0, std::min(commands.size(), cursor + 8));
            for (std::size_t i = cursor >= 8 ? cursor - 8 : 0; i < std::min(commands.size(), cursor + 8); ++i) std::fprintf(stderr, " %08x", commands[i]);
            std::fprintf(stderr, "\n");
            throw std::runtime_error(std::string("AGC driver: ") + what);
        }
        const auto count = Pm4::PacketWords(header);
        require(count <= commands.size() - cursor, "truncated PM4 packet");
        try {
            Pm4::Validate(commands.subspan(cursor, count), queue);
            if (header == FlipPacketHeader && !guarded.empty()) throw std::runtime_error("a flip inside a conditional execution range (conditional flip reservation) is not implemented");
            if (((header >> 8u) & 0xffu) == 0x22u) {
                const auto end = submission.conditionalEnds.find(cursor);
                if (end == submission.conditionalEnds.end()) throw std::runtime_error("conditional execution range is not made of whole packets of its command buffer");
                guarded.push_back(end->second);
            }
        } catch (const std::exception& error) {
            dumpPackets(commands, guest);
            throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what());
        }
        cursor += count;
    }
}

}

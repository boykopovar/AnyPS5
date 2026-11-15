#include "prx/libSceGnmDriver/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void WriteSfo(const std::filesystem::path& root, std::uint32_t attribute) {
    const auto metadata = root / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    const std::array<std::string, 3> keys{"TITLE", "TITLE_ID", "ATTRIBUTE"};
    const std::array<std::string, 2> text{"Gnm init test", "CUSA00000"};
    std::vector<std::uint8_t> entries(20 + keys.size() * 16);
    std::vector<std::uint8_t> names;
    std::vector<std::uint8_t> data;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto offset = 20 + i * 16;
        const auto nameOffset = static_cast<std::uint16_t>(names.size());
        std::memcpy(entries.data() + offset, &nameOffset, sizeof(nameOffset));
        const std::uint16_t format = i == 2 ? 0x0404 : 0x0204;
        std::memcpy(entries.data() + offset + 2, &format, sizeof(format));
        const auto length = static_cast<std::uint32_t>(i == 2 ? sizeof(attribute) : text[i].size() + 1);
        Put32(entries, offset + 4, length);
        Put32(entries, offset + 8, length);
        Put32(entries, offset + 12, static_cast<std::uint32_t>(data.size()));
        names.insert(names.end(), keys[i].begin(), keys[i].end());
        names.push_back(0);
        if (i == 2) {
            const auto* raw = reinterpret_cast<const std::uint8_t*>(&attribute);
            data.insert(data.end(), raw, raw + sizeof(attribute));
        } else {
            data.insert(data.end(), text[i].begin(), text[i].end());
            data.push_back(0);
        }
    }
    while (names.size() % 4 != 0) names.push_back(0);
    std::memcpy(entries.data(), "\0PSF", 4);
    Put32(entries, 4, 0x101);
    Put32(entries, 8, static_cast<std::uint32_t>(entries.size()));
    Put32(entries, 12, static_cast<std::uint32_t>(entries.size() + names.size()));
    Put32(entries, 16, static_cast<std::uint32_t>(keys.size()));
    entries.insert(entries.end(), names.begin(), names.end());
    entries.insert(entries.end(), data.begin(), data.end());
    std::ofstream file(metadata / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(entries.data()), static_cast<std::streamsize>(entries.size()));
    Require(static_cast<bool>(file), "could not write param.sfo fixture");
}

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8);
}

bool WholePackets(const std::array<std::uint32_t, 256>& words) {
    std::size_t at = 0;
    while (at < words.size()) {
        if ((words[at] >> 30) != 3) return false;
        at += ((words[at] >> 16) & 0x3fffu) + 2;
    }
    return at == words.size();
}

void TestHardware350(bool neo) {
    std::array<std::uint32_t, 256> words{};
    words.fill(0xdeadbeefu);
    Require(sceGnmDrawInitDefaultHardwareState350_nid_disambig1(words.data(), 255) == 0 && words[0] == 0xdeadbeefu, "350 short buffer was modified");
    Require(sceGnmDrawInitDefaultHardwareState350_nid_disambig1(nullptr, 256) == static_cast<std::uint32_t>(-1), "350 null buffer was accepted");
    Require(sceGnmDrawInitDefaultHardwareState350_nid_disambig1(words.data(), 256) == 256, "350 returned the wrong length");
    Require(words[0] == Header(0x28, 2) && words[3] == Header(0x12, 1) && words[5] == Header(0x58, 6), "350 preamble differs");
    Require(words.back() == 0 && WholePackets(words), "350 packets or padding are invalid");
    bool sh219 = false, sh21a = false, contextEb = false, context2aa = false;
    for (std::size_t at = 0; at < words.size(); at += ((words[at] >> 16) & 0x3fffu) + 2) {
        const auto opcode = (words[at] >> 8) & 0xffu;
        if (opcode == 0x76 && words[at + 1] == 0x219) sh219 = true;
        if (opcode == 0x76 && words[at + 1] == 0x21a) sh21a = true;
        if (opcode == 0x69 && words[at + 1] == 0xeb) contextEb = true;
        if (opcode == 0x69 && words[at + 1] == 0x2aa) context2aa = words[at + 2] == 0xff;
    }
    Require(sh219 == neo && sh21a == neo && contextEb == neo && context2aa != neo, "350 mode-specific registers differ");
}

void TestContext400(bool neo) {
    std::array<std::uint32_t, 256> words{};
    words.fill(0xdeadbeefu);
    Require(sceGnmDrawInitToDefaultContextState400_nid_disambig1(words.data(), 255) == 0 && words[0] == 0xdeadbeefu, "400 short buffer was modified");
    Require(sceGnmDrawInitToDefaultContextState400_nid_disambig1(nullptr, 256) == static_cast<std::uint32_t>(-1), "400 null buffer was accepted");
    Require(sceGnmDrawInitToDefaultContextState400_nid_disambig1(words.data(), 256) == 256, "400 returned the wrong length");
    Require(words[0] == Header(0x28, 2) && words[3] == Header(0x12, 1), "400 preamble differs");
    Require(WholePackets(words), "400 packets are invalid");
    const auto prefix = neo ? 102 : 98;
    Require(words[prefix - 2] == (neo ? 0xc09a1000u : 0xc09e1000u) && words[prefix - 1] == 0 && words[prefix] == 0xdeadbeefu, "400 prefix length or NOP differs");
    bool contextEb = false, context2aa = false, uconfig258 = false;
    for (std::size_t at = 0; at < prefix - 2; at += ((words[at] >> 16) & 0x3fffu) + 2) {
        const auto opcode = (words[at] >> 8) & 0xffu;
        if (opcode == 0x69 && words[at + 1] == 0xeb) contextEb = true;
        if (opcode == 0x69 && words[at + 1] == 0x2aa) context2aa = words[at + 2] == 0xff;
        if (opcode == 0x79 && words[at + 1] == 0x40000258u) uconfig258 = true;
    }
    Require(contextEb == neo && context2aa != neo && uconfig258 == neo, "400 mode-specific registers differ");
}

int RunSelf(const std::filesystem::path& program, const char* mode) {
    const std::string arguments = "\"" + program.string() + "\" " + mode;
#ifdef _WIN32
    return std::system(("\"" + arguments + "\"").c_str());
#else
    return std::system(arguments.c_str());
#endif
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const bool neo = std::string(argv[1]) == "--neo";
            Require(neo || std::string(argv[1]) == "--base", "invalid test mode");
            Require(sceKernelIsNeoMode() == neo, "mode fixture did not set sceKernelIsNeoMode");
            TestHardware350(neo);
            TestContext400(neo);
            return 0;
        }
        const auto program = std::filesystem::absolute(argv[0]);
        const auto originalDirectory = std::filesystem::current_path();
        const auto root = std::filesystem::temp_directory_path() / ("gnm_init_modes_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        for (const auto* mode : {"base", "neo"}) {
            const auto directory = root / mode;
            WriteSfo(directory, std::string(mode) == "neo" ? 0x00800000u : 0u);
            std::filesystem::current_path(directory);
            const auto result = RunSelf(program, std::string(mode) == "neo" ? "--neo" : "--base");
            std::filesystem::current_path(originalDirectory);
            Require(result == 0, "mode test child failed");
        }
        std::filesystem::remove_all(root);
        std::puts("gnm initialization mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "gnm initialization mode tests: %s\n", error.what());
        return 1;
    }
}

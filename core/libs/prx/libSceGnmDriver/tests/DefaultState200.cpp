#include "prx/libSceGnmDriver/include/Api.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InitSequences.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::int32_t SupportNeoMode = 0x00800000;

constexpr std::array<std::uint32_t, 12> ClearState{
    0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0, 0xc0055800u, 0x2ec47fc0u, 0xffffffffu, 0, 0, 0, 10,
};

constexpr std::array<std::uint32_t, 118> BaseState{
    0xc0017600u, 0x216, 0xffffffffu, 0xc0017600u, 0x217, 0xffffffffu, 0xc0017600u, 0x215, 0,
    0xc0016900u, 0x2f9, 0x2d, 0xc0016900u, 0x282, 8, 0xc0016900u, 0x280, 0x80008,
    0xc0016900u, 0x281, 0xffff0000u, 0xc0016900u, 0x204, 0, 0xc0016900u, 0x206, 0x43f,
    0xc0016900u, 0x83, 0xffff, 0xc0016900u, 0x317, 0x10, 0xc0016900u, 0x2fa, 0x3f800000u,
    0xc0016900u, 0x2fc, 0x3f800000u, 0xc0016900u, 0x2fb, 0x3f800000u, 0xc0016900u, 0x2fd, 0x3f800000u,
    0xc0016900u, 0x202, 0xcc0010, 0xc0016900u, 0x30e, 0xffffffffu, 0xc0016900u, 0x30f, 0xffffffffu,
    0xc0002f00u, 1,
    0xc0017600u, 7, 0x1701ff, 0xc0017600u, 0x46, 0x1701fd, 0xc0017600u, 0x87, 0x1701ff,
    0xc0017600u, 0xc7, 0x1701fd, 0xc0017600u, 0x107, 0x17, 0xc0017600u, 0x147, 0x1701fd,
    0xc0017600u, 0x47, 0x1c,
    0xc0016900u, 0x1b1, 2, 0xc0016900u, 0x101, 0, 0xc0016900u, 0x100, 0xffffffffu,
    0xc0016900u, 0x103, 0, 0xc0016900u, 0x284, 0, 0xc0016900u, 0x290, 0,
    0xc0016900u, 0x2ae, 0, 0xc0016900u, 0x292, 0, 0xc0016900u, 0x293, 0x6020000,
    0xc0016900u, 0x2f8, 0, 0xc0016900u, 0x2de, 0x1e9,
    0xc0036900u, 0x295, 0x100, 0x100, 4, 0xc0017900u, 0x200, 0xe0000000u,
};

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("Gnm default state 200: " + what);
}

bool WholePackets(const std::uint32_t* words, std::size_t count) {
    std::size_t at = 0;
    while (at < count) {
        if ((words[at] >> 30) != 3) return false;
        at += ((words[at] >> 16) & 0x3fffu) + 2;
    }
    return at == count;
}

template <std::size_t Words>
void CheckState(const std::array<std::uint32_t, Words>& state, const std::string& mode) {
    std::array<std::uint32_t, 0x102> words{};
    words.fill(Untouched);
    Require(sceGnmDrawInitDefaultHardwareState200_nid_disambig1(words.data() + 1, 0xff) == 0, mode + ": a short buffer was accepted");
    Require(std::all_of(words.begin(), words.end(), [](std::uint32_t word) { return word == Untouched; }), mode + ": a short buffer was written");
    Require(sceGnmDrawInitDefaultHardwareState200_nid_disambig1(words.data() + 1, 0x100) == 0x100, mode + ": the state is not 256 dwords");
    Require(words.front() == Untouched && words.back() == Untouched, mode + ": the state wrote outside its 256 dwords");
    const auto* cmd = words.data() + 1;
    Require(std::equal(ClearState.begin(), ClearState.end(), cmd), mode + ": the state does not start with the context reset");
    Require(std::equal(state.begin(), state.end(), cmd + ClearState.size()), mode + ": the register sequence differs");
    const auto written = ClearState.size() + state.size();
    Require(cmd[written] == (0xc0001000u | static_cast<std::uint32_t>((0x100 - written - 2) << 16)), mode + ": the tail is not one NOP to the end");
    Require(std::all_of(cmd + written + 1, cmd + 0x100, [](std::uint32_t word) { return word == 0; }), mode + ": the NOP body is not zero");
    Require(WholePackets(cmd, 0x100), mode + ": the state is not whole PM4 packets");

    words.fill(Untouched);
    Require(sceGnmDrawInitDefaultHardwareState200_nid_disambig1(words.data() + 1, 0x101) == 0x100, mode + ": a longer buffer did not get 256 dwords");
    Require(words.back() == Untouched, mode + ": a longer buffer was written past 256 dwords");
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, 4);
}

void WriteParamSfo(const std::filesystem::path& directory, std::int32_t attribute) {
    struct Entry {
        std::string key;
        std::uint16_t format;
        std::vector<std::uint8_t> data;
    };
    const auto text = [](const std::string& key, const std::string& value) {
        std::vector<std::uint8_t> data(value.begin(), value.end());
        data.push_back(0);
        return Entry{key, 0x0204, data};
    };
    std::vector<std::uint8_t> number(4);
    std::memcpy(number.data(), &attribute, 4);
    const std::vector<Entry> entries{text("TITLE", "Example"), text("TITLE_ID", "CUSA00000"), {"ATTRIBUTE", 0x0404, number}};
    std::vector<std::uint8_t> keys;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> bytes(20 + entries.size() * 16);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto entry = 20 + index * 16;
        const auto keyOffset = static_cast<std::uint16_t>(keys.size());
        std::memcpy(bytes.data() + entry, &keyOffset, 2);
        std::memcpy(bytes.data() + entry + 2, &entries[index].format, 2);
        Put32(bytes, entry + 4, static_cast<std::uint32_t>(entries[index].data.size()));
        Put32(bytes, entry + 8, static_cast<std::uint32_t>(entries[index].data.size()));
        Put32(bytes, entry + 12, static_cast<std::uint32_t>(data.size()));
        keys.insert(keys.end(), entries[index].key.begin(), entries[index].key.end());
        keys.push_back(0);
        data.insert(data.end(), entries[index].data.begin(), entries[index].data.end());
    }
    while (keys.size() % 4 != 0) keys.push_back(0);
    std::memcpy(bytes.data(), "\0PSF", 4);
    Put32(bytes, 4, 0x0101);
    Put32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    Put32(bytes, 12, static_cast<std::uint32_t>(bytes.size() + keys.size()));
    Put32(bytes, 16, static_cast<std::uint32_t>(entries.size()));
    bytes.insert(bytes.end(), keys.begin(), keys.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    const auto metadata = directory / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    std::ofstream file(metadata / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot write the param.sfo fixture");
}

int RunSelf(const char* program, std::string_view mode, const std::filesystem::path& directory) {
    const std::string arguments = "\"" + std::string(program) + "\" " + std::string(mode) + " \"" + directory.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    return std::system(command.c_str());
}

int Child(std::string_view mode, const char* directory) {
    std::filesystem::current_path(directory);
    if (mode == "--base") {
        CheckState(BaseState, "base mode");
        return 0;
    }
    if (mode == "--neo") {
        CheckState(GnmNeo::HardwareState200, "Neo mode");
        return 0;
    }
    if (mode == "--no-metadata") {
        std::array<std::uint32_t, 0x100> words{};
        words.fill(Untouched);
        Require(sceGnmDrawInitDefaultHardwareState200_nid_disambig1(words.data(), 0xff) == 0, "a short buffer without metadata was accepted");
        bool threw = false;
        try {
            sceGnmDrawInitDefaultHardwareState200_nid_disambig1(words.data(), 0x100);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "a title without metadata got a state");
        Require(std::all_of(words.begin(), words.end(), [](std::uint32_t word) { return word == Untouched; }), "a title without metadata had its buffer written");
        return 0;
    }
    return 1;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 3) return Child(argv[1], argv[2]);
        const auto root = std::filesystem::temp_directory_path() / ("gnm_default_state_200_" + std::to_string(std::random_device{}()));
        std::filesystem::remove_all(root);
        const struct {
            const char* name;
            const char* mode;
            bool paramSfo;
            std::int32_t attribute;
        } cases[] = {
            {"base", "--base", true, 0},
            {"other", "--base", true, 0x00004002},
            {"neo", "--neo", true, SupportNeoMode},
            {"none", "--no-metadata", false, 0},
        };
        int failed = 0;
        for (const auto& test : cases) {
            const auto directory = root / test.name;
            std::filesystem::create_directories(directory);
            if (test.paramSfo) WriteParamSfo(directory, test.attribute);
            if (RunSelf(argv[0], test.mode, directory) != 0) {
                std::fprintf(stderr, "Gnm default state 200: case failed: %s\n", test.name);
                failed = 1;
            }
        }
        std::filesystem::remove_all(root);
        if (failed == 0) std::puts("Gnm default state 200 tests passed");
        return failed;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

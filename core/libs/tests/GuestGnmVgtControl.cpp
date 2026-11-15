#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
using SetControl = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using ResetControl = int (APS5_VABI*)(std::uint32_t*, std::uint32_t);

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void* Load(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr, "cannot load GNM provider " + path.string());
    return module;
}

void* Symbol(void* module, const char* name) {
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    auto* address = dlsym(module, name);
#endif
    Require(address != nullptr, std::string("GNM provider is missing ") + name);
    return address;
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void WriteSfo(const std::filesystem::path& directory, bool neo) {
    const std::array<std::string, 3> keys{"TITLE", "TITLE_ID", "ATTRIBUTE"};
    const std::array<std::string, 2> text{"VGT test", "CUSA00000"};
    std::vector<std::uint8_t> names, data;
    std::vector<std::uint8_t> bytes(20u + keys.size() * 16u);
    for (std::size_t i = 0u; i < keys.size(); ++i) {
        while ((data.size() & 3u) != 0u) data.push_back(0u);
        const auto entry = 20u + i * 16u;
        const auto keyOffset = static_cast<std::uint16_t>(names.size());
        const std::uint16_t format = i == 2u ? 0x0404u : 0x0204u;
        const auto size = i == 2u ? 4u : static_cast<std::uint32_t>(text[i].size() + 1u);
        std::memcpy(bytes.data() + entry, &keyOffset, 2u);
        std::memcpy(bytes.data() + entry + 2u, &format, 2u);
        Put32(bytes, entry + 4u, size);
        Put32(bytes, entry + 8u, size);
        Put32(bytes, entry + 12u, static_cast<std::uint32_t>(data.size()));
        names.insert(names.end(), keys[i].begin(), keys[i].end());
        names.push_back(0u);
        if (i == 2u) {
            const std::uint32_t attribute = neo ? 0x00800000u : 0x00004002u;
            const auto at = data.size();
            data.resize(at + 4u);
            std::memcpy(data.data() + at, &attribute, 4u);
        } else {
            data.insert(data.end(), text[i].begin(), text[i].end());
            data.push_back(0u);
        }
    }
    while ((names.size() & 3u) != 0u) names.push_back(0u);
    std::memcpy(bytes.data(), "\0PSF", 4u);
    Put32(bytes, 4u, 0x0101u);
    Put32(bytes, 8u, static_cast<std::uint32_t>(bytes.size()));
    Put32(bytes, 12u, static_cast<std::uint32_t>(bytes.size() + names.size()));
    Put32(bytes, 16u, static_cast<std::uint32_t>(keys.size()));
    bytes.insert(bytes.end(), names.begin(), names.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    std::ofstream file(directory / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot write VGT param.sfo fixture");
}

class Fixture {
public:
    explicit Fixture(const std::string& mode) : previous(std::filesystem::current_path()) {
#ifdef _WIN32
        const auto pid = _getpid();
#else
        const auto pid = getpid();
#endif
        root = std::filesystem::temp_directory_path() / ("gnm_vgt_" + std::to_string(pid));
        const auto metadata = root / "app0" / "sce_sys";
        std::filesystem::create_directories(metadata);
        if (mode == "base" || mode == "neo") WriteSfo(metadata, mode == "neo");
        else if (mode == "json") {
            std::ofstream file(metadata / "param.json");
            file << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"VGT test"}}})";
            Require(static_cast<bool>(file), "cannot write VGT param.json fixture");
        } else Require(mode == "none", "unknown VGT metadata mode");
        std::filesystem::current_path(root);
    }
    ~Fixture() {
        std::filesystem::current_path(previous);
        std::filesystem::remove_all(root);
    }
private:
    std::filesystem::path previous;
    std::filesystem::path root;
};

void CheckPacket(const std::array<std::uint32_t, 8>& words, bool neo, std::uint32_t value) {
    Require(words[2] == (neo ? 0xc0017900u : 0xc0016900u), "VGT packet has wrong register opcode");
    Require(words[3] == (neo ? 0x40000258u : 0x2aau), "VGT packet has wrong register address");
    Require(words[4] == value, "VGT packet has wrong value");
    for (std::size_t i = 0u; i < words.size(); ++i) if (i < 2u || i > 4u) Require(words[i] == Sentinel, "VGT packet overwrote a guard");
}

void CheckRejected(SetControl set, ResetControl reset) {
    std::array<std::uint32_t, 8> words{};
    words.fill(Sentinel);
    const auto unchanged = [&] { Require(std::all_of(words.begin(), words.end(), [](auto word) { return word == Sentinel; }), "rejected VGT call modified memory"); };
    Require(set(nullptr, 3u, 0u, 0u, 0u) == -1, "VGT set accepted null buffer");
    Require(reset(nullptr, 3u) == -1, "VGT reset accepted null buffer");
    for (const auto size : {0u, 1u, 2u, 4u, 256u, UINT32_MAX}) {
        Require(set(words.data() + 2u, size, 0u, 0u, 0u) == -1, "VGT set accepted invalid size");
        unchanged();
        Require(reset(words.data() + 2u, size) == -1, "VGT reset accepted invalid size");
        unchanged();
    }
    for (const auto invalid : {256u, 257u, UINT32_MAX}) {
        Require(set(words.data() + 2u, 3u, invalid, 0u, 0u) == -1, "VGT set accepted oversized group");
        unchanged();
    }
    for (const auto invalid : {2u, 3u, 256u, UINT32_MAX}) {
        Require(set(words.data() + 2u, 3u, 0u, invalid, 0u) == -1, "VGT set accepted invalid partial-wave mode");
        unchanged();
        Require(set(words.data() + 2u, 3u, 0u, 0u, invalid) == -1, "VGT set accepted invalid EOP mode");
        unchanged();
    }
}

void CheckUnknown(SetControl set, ResetControl reset, const std::string& mode) {
    std::array<std::uint32_t, 8> words{};
    words.fill(Sentinel);
    for (const bool resetting : {false, true}) {
        std::string error;
        try {
            if (resetting) reset(words.data() + 2u, 3u);
            else set(words.data() + 2u, 3u, 127u, 1u, 0u);
        } catch (const std::runtime_error& caught) {
            error = caught.what();
        }
        Require(error.find(mode == "none" ? "param.json or param.sfo" : "sceKernelIsNeoMode") != std::string::npos, "unknown VGT mode did not refuse with its metadata error");
        Require(std::all_of(words.begin(), words.end(), [](auto word) { return word == Sentinel; }), "unknown VGT mode modified memory");
    }
}

void CheckWrites(SetControl set, ResetControl reset, bool neo) {
    std::array<std::uint32_t, 8> words{};
    for (std::uint32_t group = 0u; group < 256u; ++group) {
        for (std::uint32_t partial = 0u; partial < 2u; ++partial) {
            for (std::uint32_t eop = 0u; eop < 2u; ++eop) {
                words.fill(Sentinel);
                Require(set(words.data() + 2u, 3u, group, partial, eop) == 0, "valid VGT set failed");
                const auto expected = neo ? (eop != 0u ? 0x740000u | (partial << 16u) | group : 0x6d0000u | group) : (partial << 16u) | group;
                CheckPacket(words, neo, expected);
            }
        }
    }
    words.fill(Sentinel);
    Require(reset(words.data() + 2u, 3u) == 0, "valid VGT reset failed");
    CheckPacket(words, neo, neo ? 0x6d007fu : 0xffu);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 4, "expected provider, metadata mode and library path");
        const std::string provider = argv[1], mode = argv[2];
        Require(provider == "normal" || provider == "neo", "unknown GNM provider");
        const auto library = std::filesystem::absolute(argv[3]);
        Fixture fixture(mode);
        auto* module = Load(library);
        const bool normal = provider == "normal";
        const auto set = reinterpret_cast<SetControl>(Symbol(module, normal ? "sceGnmSetVgtControl_nid_disambig1" : "sceGnmSetVgtControl"));
        const auto reset = reinterpret_cast<ResetControl>(Symbol(module, normal ? "sceGnmResetVgtControl_nid_disambig1" : "sceGnmResetVgtControl"));
        CheckRejected(set, reset);
        if (normal && (mode == "json" || mode == "none")) CheckUnknown(set, reset, mode);
        else CheckWrites(set, reset, !normal || mode == "neo");
        std::printf("GNM VGT %s/%s: packets, refusals and guards passed\n", provider.c_str(), mode.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GNM VGT: %s\n", error.what());
        return 1;
    }
}

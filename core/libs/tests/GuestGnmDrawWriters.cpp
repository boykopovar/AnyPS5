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
#include <span>
#include <string_view>
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
    const std::array<std::string, 2> text{"GNM draw test", "CUSA00000"};
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
    Require(static_cast<bool>(file), "cannot write GNM draw param.sfo fixture");
}

class Fixture {
public:
    explicit Fixture(const std::string& mode) : previous(std::filesystem::current_path()) {
#ifdef _WIN32
        const auto pid = _getpid();
#else
        const auto pid = getpid();
#endif
        root = std::filesystem::temp_directory_path() / ("gnm_draw_" + std::to_string(pid));
        const auto metadata = root / "app0" / "sce_sys";
        std::filesystem::create_directories(metadata);
        if (mode == "base" || mode == "neo") WriteSfo(metadata, mode == "neo");
        else if (mode == "json") {
            std::ofstream file(metadata / "param.json");
            file << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"GNM draw test"}}})";
            Require(static_cast<bool>(file), "cannot write GNM draw param.json fixture");
        } else Require(mode == "none", "unknown GNM draw metadata mode");
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

using DrawIndex = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t);
using DrawAuto = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t);
using DrawOffset = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using DrawIndirect = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using DrawMulti = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using DrawCount = int (APS5_VABI*)(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

template <typename TFunction>
TFunction Resolve(void* module, bool normal, bool patched, const char* name, const char* nid) {
    const auto symbol = patched ? std::string(nid) : std::string(name) + (normal ? "_nid_disambig1" : "");
    return reinterpret_cast<TFunction>(Symbol(module, symbol.c_str()));
}

struct Writers {
    DrawIndex index;
    DrawAuto automatic;
    DrawOffset offset;
    std::array<DrawIndirect, 2> indirect;
    std::array<DrawMulti, 2> multi;
    std::array<DrawCount, 2> count;

    Writers(void* module, bool normal, bool patched) :
        index(Resolve<DrawIndex>(module, normal, patched, "sceGnmDrawIndex", "HlTPoZ-oY7Y")),
        automatic(Resolve<DrawAuto>(module, normal, patched, "sceGnmDrawIndexAuto", "GGsn7jMTxw4")),
        offset(Resolve<DrawOffset>(module, normal, patched, "sceGnmDrawIndexOffset", "oYM+YzfCm2Y")),
        indirect{Resolve<DrawIndirect>(module, normal, patched, "sceGnmDrawIndexIndirect", "ED9-Fjr8Ta4"), Resolve<DrawIndirect>(module, normal, patched, "sceGnmDrawIndirect", "4v+otIIdjqg")},
        multi{Resolve<DrawMulti>(module, normal, patched, "sceGnmDrawIndexIndirectMulti", "5q95ravnueg"), Resolve<DrawMulti>(module, normal, patched, "sceGnmDrawIndirectMulti", "f5QQLp9rzGk")},
        count{Resolve<DrawCount>(module, normal, patched, "sceGnmDrawIndexIndirectCountMulti", "thbPcG7E7qk"), Resolve<DrawCount>(module, normal, patched, "sceGnmDrawIndirectCountMulti", "cUCo8OvArrw")} {}
};

std::uint64_t checked = 0u;

template <typename TCall>
void Writes(std::span<const std::uint32_t> expected, TCall call) {
    std::array<std::uint32_t, 24> words{};
    words.fill(Sentinel);
    Require(call(words.data() + 4u, static_cast<std::uint32_t>(expected.size())) == 0, "valid draw writer refused input");
    for (std::size_t i = 0u; i < words.size(); ++i) {
        const auto wanted = i >= 4u && i < 4u + expected.size() ? expected[i - 4u] : Sentinel;
        Require(words[i] == wanted, "draw packet or preserved padding/guard mismatch at word " + std::to_string(i));
    }
    ++checked;
}

template <typename TCall>
void Refuses(TCall call) {
    std::array<std::uint32_t, 24> words{};
    words.fill(Sentinel);
    Require(call(words.data() + 4u) == -1, "invalid draw writer input was accepted");
    Require(std::all_of(words.begin(), words.end(), [](auto word) { return word == Sentinel; }), "refused draw writer modified memory");
    ++checked;
}

template <typename TCall>
void Unknown(TCall call, const std::string& mode) {
    std::array<std::uint32_t, 24> words{};
    words.fill(Sentinel);
    std::string error;
    try {
        call(words.data() + 4u);
    } catch (const std::runtime_error& caught) {
        error = caught.what();
    }
    Require(error.find(mode == "none" ? "param.json or param.sfo" : "sceKernelIsNeoMode") != std::string::npos, "unknown draw mode was not refused with its metadata error");
    Require(std::all_of(words.begin(), words.end(), [](auto word) { return word == Sentinel; }), "unknown draw mode modified memory");
    ++checked;
}

void CheckUnknown(const Writers& writer, const std::string& mode) {
    Unknown([&](auto* p) { return writer.index(p, 10u, 3u, 0x1234567800000042ull, 0u, 0u); }, mode);
    Unknown([&](auto* p) { return writer.automatic(p, 7u, 3u, 0u); }, mode);
    Unknown([&](auto* p) { return writer.offset(p, 9u, 5u, 3u, 0u); }, mode);
    for (std::size_t i = 0u; i < 2u; ++i) {
        Unknown([&](auto* p) { return writer.indirect[i](p, 9u, 4u, 2u, 1u, 2u, 0u); }, mode);
        Unknown([&](auto* p) { return writer.multi[i](p, 11u, 4u, 3u, 2u, 1u, 2u, 0u); }, mode);
        Unknown([&](auto* p) { return writer.count[i](p, 16u, 4u, 3u, 0x1234567800000042ull, 2u, 1u, 2u, 0u); }, mode);
    }
}

void CheckRejected(const Writers& writer, bool unknown, const std::string& mode) {
    for (const auto size : {0u, 1u, 6u, 8u, 12u, 17u, UINT32_MAX}) {
        Refuses([&](auto* p) { return writer.index(p, size, 3u, 2u, 0u, 0u); });
        Refuses([&](auto* p) { return writer.automatic(p, size, 3u, 0u); });
        Refuses([&](auto* p) { return writer.offset(p, size, 3u, 4u, 0u); });
        for (std::size_t i = 0u; i < 2u; ++i) {
            Refuses([&](auto* p) { return writer.indirect[i](p, size, 0u, 2u, 0u, 0u, 0u); });
            Refuses([&](auto* p) { return writer.multi[i](p, size, 0u, 1u, 2u, 0u, 0u, 0u); });
            if (unknown && i == 0u) Unknown([&](auto* p) { return writer.count[i](p, size, 0u, 1u, 0u, 2u, 0u, 0u, 0u); }, mode);
            else Refuses([&](auto* p) { return writer.count[i](p, size, 0u, 1u, 0u, 2u, 0u, 0u, 0u); });
        }
    }
    Refuses([&](auto*) { return writer.index(nullptr, 10u, 1u, 2u, 0u, 0u); });
    Refuses([&](auto*) { return writer.automatic(nullptr, 7u, 1u, 0u); });
    Refuses([&](auto*) { return writer.offset(nullptr, 9u, 0u, 1u, 0u); });
    for (std::size_t i = 0u; i < 2u; ++i) {
        Refuses([&](auto*) { return writer.indirect[i](nullptr, 9u, 0u, 2u, 0u, 0u, 0u); });
        Refuses([&](auto*) { return writer.multi[i](nullptr, 11u, 0u, 1u, 2u, 0u, 0u, 0u); });
        if (unknown && i == 0u) Unknown([&](auto*) { return writer.count[i](nullptr, 16u, 0u, 1u, 0u, 2u, 0u, 0u, 0u); }, mode);
        else Refuses([&](auto*) { return writer.count[i](nullptr, 16u, 0u, 1u, 0u, 2u, 0u, 0u, 0u); });
        for (const auto invalid : {16u, 17u, UINT32_MAX}) {
            for (const bool vertex : {false, true}) {
                const auto v = vertex ? invalid : 0u, instance = vertex ? 0u : invalid;
                Refuses([&](auto* p) { return writer.indirect[i](p, 9u, 0u, 2u, v, instance, 0u); });
                Refuses([&](auto* p) { return writer.multi[i](p, 11u, 0u, 1u, 2u, v, instance, 0u); });
                if (unknown && i == 0u) Unknown([&](auto* p) { return writer.count[i](p, 16u, 0u, 1u, 0u, 2u, v, instance, 0u); }, mode);
                else Refuses([&](auto* p) { return writer.count[i](p, 16u, 0u, 1u, 0u, 2u, v, instance, 0u); });
            }
        }
        for (const auto stage : {7u, 8u, UINT32_MAX}) {
            Refuses([&](auto* p) { return writer.indirect[i](p, 9u, 0u, stage, 0u, 0u, 0u); });
            Refuses([&](auto* p) { return writer.multi[i](p, 11u, 0u, 1u, stage, 0u, 0u, 0u); });
            if (unknown && i == 0u) Unknown([&](auto* p) { return writer.count[i](p, 16u, 0u, 1u, 0u, stage, 0u, 0u, 0u); }, mode);
            else Refuses([&](auto* p) { return writer.count[i](p, 16u, 0u, 1u, 0u, stage, 0u, 0u, 0u); });
        }
    }
    for (const auto stage : {0u, 1u, 3u, 5u}) {
        Refuses([&](auto* p) { return writer.multi[0](p, 11u, 0u, 1u, stage, 0u, 0u, 0u); });
        if (unknown) Unknown([&](auto* p) { return writer.count[0](p, 16u, 0u, 1u, 0u, stage, 0u, 0u, 0u); }, mode);
        else Refuses([&](auto* p) { return writer.count[0](p, 16u, 0u, 1u, 0u, stage, 0u, 0u, 0u); });
    }
    for (const auto address : std::array<std::uint64_t, 4>{0u, 1u, 3u, UINT64_MAX}) Refuses([&](auto* p) { return writer.index(p, 10u, 1u, address, 0u, 0u); });
    for (std::uint32_t bit = 1u; bit < 29u; ++bit) {
        Refuses([&](auto* p) { return writer.index(p, 10u, 1u, 2u, 1u << bit, 0u); });
        Refuses([&](auto* p) { return writer.automatic(p, 7u, 1u, 1u << bit); });
    }
}

void CheckDirect(const Writers& writer, bool neo) {
    for (std::uint32_t high = 0u; high < 8u; ++high) {
        for (std::uint32_t predicate = 0u; predicate < 2u; ++predicate) {
            const auto flags = (high << 29u) | predicate;
            const auto initiator = neo ? high << 29u : 0u;
            for (const auto count : {0u, 1u, 0x80000000u, UINT32_MAX}) {
                for (const auto address : {2ull, 0x1234567800000042ull, 0xfffffffffffffffeull}) {
                    const std::array<std::uint32_t, 10> expected{0xc0042700u, count, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), count, initiator, 0xc0021000u, 0u, Sentinel, Sentinel};
                    for (const auto type : {0u, 1u, UINT32_MAX}) Writes(expected, [&](auto* p, auto size) { return writer.index(p, size, count, address, flags, type); });
                }
                const std::array<std::uint32_t, 7> automatic{0xc0012d00u, count, initiator | 2u, 0xc0021000u, 0u, Sentinel, Sentinel};
                Writes(automatic, [&](auto* p, auto size) { return writer.automatic(p, size, count, flags); });
                for (const auto offset : {0u, 0x80000000u, UINT32_MAX}) {
                    const std::array<std::uint32_t, 9> indexed{0xc0033500u | predicate, count, offset, count, initiator, 0xc0021000u, 0u, Sentinel, Sentinel};
                    Writes(indexed, [&](auto* p, auto size) { return writer.offset(p, size, offset, count, flags | 0x1ffffffeu); });
                }
            }
        }
    }
}

void CheckIndirect(const Writers& writer, bool neo) {
    constexpr std::array<std::uint32_t, 7> bases{0u, 0u, 0x4cu, 0u, 0xccu, 0u, 0x14cu};
    for (std::uint32_t stage = 0u; stage < 7u; ++stage) {
        for (std::uint32_t vertex = 0u; vertex < 16u; ++vertex) {
            for (std::uint32_t instance = 0u; instance < 16u; ++instance) {
                const auto v = vertex == 0u ? 0u : bases[stage] + vertex;
                const auto i = instance == 0u ? 0u : bases[stage] + instance;
                for (std::uint32_t high = 0u; high < 8u; ++high) {
                    for (std::uint32_t predicate = 0u; predicate < 2u; ++predicate) {
                        const auto flags = (high << 29u) | predicate | 0x1ffffffeu;
                        for (std::size_t kind = 0u; kind < 2u; ++kind) {
                            const auto initiator = (neo ? high << 29u : 0u) | (kind == 1u ? 2u : 0u);
                            const auto offset = (stage * 4096u + vertex * 16u + instance) ^ 0x80000000u;
                            const std::array<std::uint32_t, 9> single{(kind == 0u ? 0xc0032500u : 0xc0032400u) | predicate, offset, v, i, initiator, 0xc0021000u, 0u, Sentinel, Sentinel};
                            Writes(single, [&](auto* p, auto size) { return writer.indirect[kind](p, size, offset, stage, vertex, instance, flags); });
                            if (kind == 0u && stage != 2u && stage != 4u && stage != 6u) continue;
                            const auto count = high == 0u ? 0u : high == 7u ? UINT32_MAX : high;
                            const auto stride = kind == 0u ? 20u : 16u;
                            const std::array<std::uint32_t, 11> multiple{(kind == 0u ? 0xc0053800u : 0xc0052c00u) | predicate, offset, v, i, count, stride, initiator, 0xc0021000u, 0u, Sentinel, Sentinel};
                            Writes(multiple, [&](auto* p, auto size) { return writer.multi[kind](p, size, offset, count, stage, vertex, instance, flags); });
                            for (const auto countAddress : std::array<std::uint64_t, 3>{0u, 0x1234567800000043ull, UINT64_MAX}) {
                                const std::array<std::uint32_t, 16> counted{0xc0011000u, 0u, Sentinel, (kind == 0u ? 0xc0089d00u : 0xc0089c00u) | predicate, offset, v, i, countAddress != 0u ? 0x40000000u : 0u, count, static_cast<std::uint32_t>(countAddress), static_cast<std::uint32_t>(countAddress >> 32u), stride, initiator, 0xc0011000u, 0u, Sentinel};
                                Writes(counted, [&](auto* p, auto size) { return writer.count[kind](p, size, offset, count, countAddress, stage, vertex, instance, flags); });
                            }
                        }
                    }
                }
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 4 || (argc == 5 && std::string_view(argv[4]) == "--patched"), "expected provider, metadata mode and library path, optionally --patched");
        const std::string provider = argv[1], mode = argv[2];
        Require(provider == "normal" || provider == "neo", "unknown GNM provider");
        const auto library = std::filesystem::absolute(argv[3]);
        Fixture fixture(mode);
        const bool normal = provider == "normal", unknown = normal && (mode == "json" || mode == "none");
        const Writers writer(Load(library), normal, argc == 5);
        CheckRejected(writer, unknown, mode);
        if (unknown) CheckUnknown(writer, mode);
        else {
            const bool neo = !normal || mode == "neo";
            CheckDirect(writer, neo);
            CheckIndirect(writer, neo);
        }
        std::printf("GNM draw %s/%s: %llu packet/refusal checks passed\n", provider.c_str(), mode.c_str(), static_cast<unsigned long long>(checked));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GNM draw: %s\n", error.what());
        return 1;
    }
}

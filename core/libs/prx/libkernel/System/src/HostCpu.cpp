#include "prx/libkernel/System/include/HostCpu.hpp"
#include "prx/libc/include/general/LogMacros.hpp"
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <link.h>
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class Register : std::uint8_t {
    Leaf1Ecx,
    Leaf1Edx,
    Leaf7Ebx,
    Leaf7Ecx,
    LeafDSub1Eax,
    Leaf80000001Ecx,
    Leaf80000008Ebx
};

struct FeatureBit {
    const char* Name;
    Register Source;
    unsigned Bit;
    bool Required;
};

constexpr std::uint32_t kLzcnt = 1u << 0;
constexpr std::uint32_t kBmi1 = 1u << 1;
constexpr std::uint32_t kBmi2 = 1u << 2;
constexpr std::uint32_t kMovbe = 1u << 3;
constexpr std::uint32_t kAvx2 = 1u << 4;
constexpr std::uint32_t kFma = 1u << 5;
constexpr std::uint32_t kF16c = 1u << 6;
constexpr char kRecordName[] = "AnyPS5";

constexpr FeatureBit kFeatures[] = {
    {"SSE", Register::Leaf1Edx, 25, true},
    {"SSE2", Register::Leaf1Edx, 26, true},
    {"SSE3", Register::Leaf1Ecx, 0, true},
    {"SSSE3", Register::Leaf1Ecx, 9, true},
    {"SSE4.1", Register::Leaf1Ecx, 19, true},
    {"SSE4.2", Register::Leaf1Ecx, 20, true},
    {"POPCNT", Register::Leaf1Ecx, 23, true},
    {"LZCNT", Register::Leaf80000001Ecx, 5, true},
    {"BMI1", Register::Leaf7Ebx, 3, true},
    {"BMI2", Register::Leaf7Ebx, 8, true},
    {"MOVBE", Register::Leaf1Ecx, 22, true},
    {"CMPXCHG16B", Register::Leaf1Ecx, 13, true},
    {"LAHF/SAHF", Register::Leaf80000001Ecx, 0, true},
    {"AVX", Register::Leaf1Ecx, 28, true},
    {"AVX2", Register::Leaf7Ebx, 5, true},
    {"FMA", Register::Leaf1Ecx, 12, true},
    {"F16C", Register::Leaf1Ecx, 29, true},
    {"AES", Register::Leaf1Ecx, 25, false},
    {"PCLMULQDQ", Register::Leaf1Ecx, 1, false},
    {"RDRAND", Register::Leaf1Ecx, 30, false},
    {"RDSEED", Register::Leaf7Ebx, 18, false},
    {"ADX", Register::Leaf7Ebx, 19, false},
    {"SHA", Register::Leaf7Ebx, 29, false},
    {"CLFLUSHOPT", Register::Leaf7Ebx, 23, false},
    {"CLWB", Register::Leaf7Ebx, 24, false},
    {"RDPID", Register::Leaf7Ecx, 22, false},
    {"XSAVEOPT", Register::LeafDSub1Eax, 0, false},
    {"XSAVEC", Register::LeafDSub1Eax, 1, false},
    {"SSE4A", Register::Leaf80000001Ecx, 6, false},
    {"PREFETCHW", Register::Leaf80000001Ecx, 8, false},
    {"CLZERO", Register::Leaf80000008Ebx, 0, false},
};

constexpr unsigned kOsxsaveBit = 27;
constexpr std::uint64_t kXcr0SseAvxState = 0x6;

std::uint32_t Value(const HostCpuRegisters& registers, const Register source) {
    switch (source) {
    case Register::Leaf1Ecx:
        return registers.Leaf1Ecx;
    case Register::Leaf1Edx:
        return registers.Leaf1Edx;
    case Register::Leaf7Ebx:
        return registers.Leaf7Ebx;
    case Register::Leaf7Ecx:
        return registers.Leaf7Ecx;
    case Register::LeafDSub1Eax:
        return registers.LeafDSub1Eax;
    case Register::Leaf80000001Ecx:
        return registers.Leaf80000001Ecx;
    case Register::Leaf80000008Ebx:
        return registers.Leaf80000008Ebx;
    }
    throw std::logic_error("unknown CPUID register");
}

bool AvxStateEnabled(const HostCpuRegisters& registers) {
    return (registers.Leaf1Ecx >> kOsxsaveBit & 1) != 0 && (registers.Xcr0 & kXcr0SseAvxState) == kXcr0SseAvxState;
}

std::string DisplayModel(const std::string& model) {
    const auto end = model.find_last_not_of(' ');
    return end == std::string::npos ? std::string("unknown model") : model.substr(0, end + 1);
}

std::optional<std::uint32_t> FeatureMask(const char* name) {
    const std::string_view feature(name);
    if (feature == "LZCNT") return kLzcnt;
    if (feature == "BMI1") return kBmi1;
    if (feature == "BMI2") return kBmi2;
    if (feature == "MOVBE") return kMovbe;
    if (feature == "AVX2") return kAvx2;
    if (feature == "FMA") return kFma;
    if (feature == "F16C") return kF16c;
    return std::nullopt;
}

std::optional<std::uint32_t> ReadNoteExtensions(const std::uint8_t* note, const std::size_t size) {
    std::size_t offset = 0;
    while (offset + 12 <= size) {
        std::uint32_t namesz = 0;
        std::uint32_t descsz = 0;
        std::uint32_t type = 0;
        std::memcpy(&namesz, note + offset, 4);
        std::memcpy(&descsz, note + offset + 4, 4);
        std::memcpy(&type, note + offset + 8, 4);
        offset += 12;
        const std::size_t nameStart = offset;
        if (namesz > size - offset) return std::nullopt;
        offset += (namesz + 3u) & ~std::size_t{3};
        if (offset > size || descsz > size - offset) return std::nullopt;
        const std::size_t descStart = offset;
        offset += (descsz + 3u) & ~std::size_t{3};
        if (type != 1 || namesz != sizeof(kRecordName) || descsz < 8) continue;
        if (std::memcmp(note + nameStart, kRecordName, sizeof(kRecordName)) != 0) continue;
        std::uint32_t version = 0;
        std::uint32_t mask = 0;
        std::memcpy(&version, note + descStart, 4);
        std::memcpy(&mask, note + descStart + 4, 4);
        if (version == 1) return mask;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> RecordedExtensions() {
#ifdef _WIN32
    const auto* image = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) return std::nullopt;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) return std::nullopt;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if (std::memcmp(section.Name, ".aps5cpu", 8) != 0) continue;
        if (section.Misc.VirtualSize < 16) return std::nullopt;
        const auto* record = image + section.VirtualAddress;
        constexpr char magic[8] = {'A', '5', 'C', 'P', 'U', 0, 0, 0};
        if (std::memcmp(record, magic, sizeof(magic)) != 0) return std::nullopt;
        std::uint32_t version = 0;
        std::uint32_t mask = 0;
        std::memcpy(&version, record + 8, 4);
        std::memcpy(&mask, record + 12, 4);
        if (version != 1) return std::nullopt;
        return mask;
    }
    return std::nullopt;
#elif defined(__linux__)
    struct State {
        std::optional<std::uint32_t> Extensions;
    } state;
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& result = *static_cast<State*>(data);
        if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
        for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type != PT_NOTE || header.p_filesz == 0) continue;
            bool mapped = false;
            for (std::uint16_t loadIndex = 0; loadIndex < image->dlpi_phnum; ++loadIndex) {
                const auto& load = image->dlpi_phdr[loadIndex];
                if (load.p_type == PT_LOAD && header.p_vaddr >= load.p_vaddr &&
                    header.p_vaddr - load.p_vaddr <= load.p_memsz &&
                    header.p_filesz <= load.p_memsz - (header.p_vaddr - load.p_vaddr))
                    mapped = true;
            }
            if (!mapped) continue;
            result.Extensions = ReadNoteExtensions(reinterpret_cast<const std::uint8_t*>(image->dlpi_addr + header.p_vaddr), header.p_filesz);
            if (result.Extensions) return 1;
        }
        return 0;
    }, &state);
    return state.Extensions;
#else
    return std::nullopt;
#endif
}

std::string Join(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) {
        if (!text.empty()) text += ", ";
        text += name;
    }
    return text;
}

}

HostCpuRegisters ReadHostCpuRegisters() {
    HostCpuRegisters registers;
#if defined(__x86_64__) || defined(__i386__)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    const unsigned int basic = __get_cpuid_max(0, nullptr);
    if (basic >= 1) {
        __cpuid(1, eax, ebx, ecx, edx);
        registers.Leaf1Ecx = ecx;
        registers.Leaf1Edx = edx;
    }
    if (basic >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        registers.Leaf7Ebx = ebx;
        registers.Leaf7Ecx = ecx;
    }
    if (basic >= 0xD) {
        __cpuid_count(0xD, 1, eax, ebx, ecx, edx);
        registers.LeafDSub1Eax = eax;
    }
    const unsigned int extended = __get_cpuid_max(0x80000000, nullptr);
    if (extended >= 0x80000001) {
        __cpuid(0x80000001, eax, ebx, ecx, edx);
        registers.Leaf80000001Ecx = ecx;
    }
    if (extended >= 0x80000008) {
        __cpuid(0x80000008, eax, ebx, ecx, edx);
        registers.Leaf80000008Ebx = ebx;
    }
    if ((registers.Leaf1Ecx >> kOsxsaveBit & 1) != 0) {
        std::uint32_t low = 0, high = 0;
        __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
        registers.Xcr0 = static_cast<std::uint64_t>(high) << 32 | low;
    }
#endif
    return registers;
}

std::string HostCpuModel() {
#if defined(__x86_64__) || defined(__i386__)
    if (__get_cpuid_max(0x80000000, nullptr) < 0x80000004) return {};
    unsigned int brand[12] = {};
    for (unsigned int leaf = 0; leaf < 3; ++leaf)
        __get_cpuid(0x80000002 + leaf, &brand[leaf * 4], &brand[leaf * 4 + 1], &brand[leaf * 4 + 2], &brand[leaf * 4 + 3]);
    std::string model(reinterpret_cast<const char*>(brand), strnlen(reinterpret_cast<const char*>(brand), sizeof(brand)));
    model.erase(0, model.find_first_not_of(' '));
    return model;
#else
    return {};
#endif
}

std::vector<HostCpuFeature> HostCpuFeatures(const HostCpuRegisters& registers) {
    std::vector<HostCpuFeature> features;
    features.reserve(std::size(kFeatures) + 1);
    for (const auto& feature : kFeatures)
        features.push_back({feature.Name, feature.Required, (Value(registers, feature.Source) >> feature.Bit & 1) != 0});
    features.push_back({"OS AVX state (XCR0)", true, AvxStateEnabled(registers)});
    return features;
}

std::vector<std::string> MissingHostCpuFeatures(const HostCpuRegisters& registers) {
    return MissingHostCpuFeatures(registers, std::nullopt);
}

std::vector<std::string> MissingHostCpuFeatures(const HostCpuRegisters& registers, const std::optional<std::uint32_t> recordedExtensions) {
    std::vector<std::string> missing;
    for (const auto& feature : HostCpuFeatures(registers)) {
        bool required = feature.Required;
        if (recordedExtensions) {
            if (const auto mask = FeatureMask(feature.Name)) required = (*recordedExtensions & *mask) != 0;
        }
        if (required && !feature.Present) missing.emplace_back(feature.Name);
    }
    return missing;
}

namespace {

HostCpuText PackText(const std::string& text) {
    HostCpuText packed{};
    std::memcpy(packed.value, text.data(), std::min(text.size(), sizeof(packed.value) - 1));
    return packed;
}

}

extern "C" {

HostCpuRegisters ReadHostCpuRegisters_nid_no_patch() {
    return ReadHostCpuRegisters();
}

HostCpuText HostCpuModel_nid_no_patch() {
    return PackText(HostCpuModel());
}

HostCpuText MissingHostCpuFeatures_nid_no_patch(const HostCpuRegisters* registers) {
    return PackText(Join(MissingHostCpuFeatures(*registers, RecordedExtensions())));
}

}

void CheckHostCpu(const HostCpuRegisters& registers, const std::string& model) {
    CheckHostCpu(registers, model, RecordedExtensions());
}

void CheckHostCpu(const HostCpuRegisters& registers, const std::string& model, const std::optional<std::uint32_t> recordedExtensions) {
    const auto missing = MissingHostCpuFeatures(registers, recordedExtensions);
    if (missing.empty()) return;
    throw std::runtime_error("The host CPU (" + DisplayModel(model) + ") lacks " + Join(missing) +
        ". The PS5's Zen 2 CPU has these extensions and titles run their instructions natively, so this CPU cannot run them.");
}

#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
namespace {

const bool HostCpuChecked = [] {
    const auto registers = ReadHostCpuRegisters();
    const auto model = HostCpuModel();
    std::vector<std::string> present;
    std::vector<std::string> absent;
    for (const auto& feature : HostCpuFeatures(registers))
        (feature.Present ? present : absent).emplace_back(feature.Name);
    APS5_LOG_OUT("host CPU %s: has %s; lacks %s", DisplayModel(model).c_str(), Join(present).c_str(), absent.empty() ? "none" : Join(absent).c_str());
    CheckHostCpu(registers, model);
    return true;
}();

}
#endif

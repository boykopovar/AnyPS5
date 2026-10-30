#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Untouched = 0xcafebabeu;

alignas(256) constexpr std::array<std::uint32_t, 64> Caller{
    0x7e020280u, 0xf4080400u, 0xfa000000u, 0x8f148402u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu,
    0xbe8e0308u, 0xbe8f0309u, 0xbf068103u, 0xbe8e210eu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
    0xbf9f0000u, 0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 64> HoistedCaller{
    0x7e020280u, 0xf4080400u, 0xfa000000u, 0xbf8cc07fu, 0xbf068203u, 0xbf850001u, 0x7e020280u, 0x8f148402u,
    0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbf068103u, 0xbe8e210eu, 0x34040082u,
    0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u, 0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 64> SliceCaller{
    0x7e020280u, 0xf40c0600u, 0xfa000010u, 0x8f148402u, 0xbf8cc07fu, 0xf428020eu, 0x28000000u, 0xbf8cc07fu,
    0xbe8e0308u, 0xbe8f0309u, 0xbf068103u, 0xbe8e210eu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
    0xbf9f0000u, 0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 3> SelectSixteen{0x85159091u, 0x7e020215u, 0xbe80200eu};
alignas(256) constexpr std::array<std::uint32_t, 3> SelectThirtyTwo{0x8515a0a1u, 0x7e020215u, 0xbe80200eu};
alignas(16) std::array<std::uint32_t, 8> Table{};
alignas(16) std::array<std::uint32_t, 12> Descriptors{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

void Fill() {
    const auto first = reinterpret_cast<std::uintptr_t>(SelectSixteen.data());
    const auto second = reinterpret_cast<std::uintptr_t>(SelectThirtyTwo.data());
    Table = {static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), 0u, 0u, static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u), 0u, 0u};
    const auto table = reinterpret_cast<std::uintptr_t>(Table.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(table), static_cast<std::uint32_t>((table >> 32u) & 0xffffu) | (16u << 16u), 2u, 0x31016facu};
    Descriptors = {};
    std::copy(descriptor.begin(), descriptor.end(), Descriptors.begin());
    std::copy(descriptor.begin(), descriptor.end(), Descriptors.begin() + 8);
}

std::array<std::uint32_t, 8> UserData(std::uint32_t index, bool scc) {
    const auto descriptors = reinterpret_cast<std::uintptr_t>(Descriptors.data());
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(descriptors), static_cast<std::uint32_t>(descriptors >> 32u), index, scc ? 1u : 0u,
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

std::vector<std::uint32_t> Commands(const std::array<std::uint32_t, 8>& data, bool wave32, const std::uint32_t* caller) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(caller);
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(data.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u | (wave32 ? 0x8000u : 0u)});
    return words;
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

std::string Dispatch(std::uint32_t index, bool scc, bool wave32, const std::uint32_t* caller = Caller.data()) {
    Output.fill(Untouched);
    const auto words = Commands(UserData(index, scc), wave32, caller);
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    StderrCapture log;
    std::string failure;
    try {
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    return log.Finish() + failure;
}

void Check(std::uint32_t index, bool scc, bool wave32, std::uint32_t expected, const std::string& what, const std::uint32_t* caller = Caller.data()) {
    const auto log = Dispatch(index, scc, wave32, caller);
    Require(log.find("no linked callee") == std::string::npos, what + ": a linked callee was reported missing: " + log);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(Output[lane] == expected, what + (wave32 ? " (wave32)" : " (wave64)") + ": lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Fill();
        for (const bool wave32 : {false, true}) {
            Check(0u, false, wave32, 16u, "the first record's callee with SCC 0");
            Check(0u, true, wave32, 17u, "the first record's callee with SCC 1");
            Check(1u, false, wave32, 32u, "the second record's callee with SCC 0");
            Check(1u, true, wave32, 33u, "the second record's callee with SCC 1");
            Check(0u, false, wave32, 16u, "the first record's callee through a descriptor loaded before a branch", HoistedCaller.data());
            Check(1u, true, wave32, 33u, "the second record's callee through a descriptor loaded before a branch", HoistedCaller.data());
            Check(0u, false, wave32, 16u, "the first record's callee through the second half of an s_load_dwordx8", SliceCaller.data());
            Check(1u, true, wave32, 33u, "the second record's callee through the second half of an s_load_dwordx8", SliceCaller.data());
            const auto log = Dispatch(2u, false, wave32);
            Require(log.find("called address 0x0 at program counter") != std::string::npos && log.find("which no linked callee of that call matches") != std::string::npos,
                std::string("a record past the table did not report an unlinked call") + (wave32 ? " (wave32)" : " (wave64)") + ": " + log);
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                Require(Output[lane] == Untouched, "a lane stored after the unlinked call: lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]));
            }
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("compute table call tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception&) {
        }
        return 1;
    }
}

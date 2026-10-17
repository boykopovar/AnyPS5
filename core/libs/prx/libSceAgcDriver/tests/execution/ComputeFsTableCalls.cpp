#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "SceShaders.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Marker = 0x55u;
constexpr std::uint64_t FsTable = 0xfe0040000ull;

alignas(256) constexpr std::array<std::uint32_t, 32> Caller{
    0x34040082u, 0x7e0202ffu, 0x00000055u, 0xbeea03ffu, 0xe0040000u, 0xbeeb038fu, 0xf4080235u, 0xfa000000u, 0xbf8cc07fu, 0xf4240384u,
    0xfa000020u, 0xbf8cc07fu, 0xbf130e80u, 0xbf840006u, 0xf4240404u, 0xfa000028u, 0xbf8cc07fu, 0xbe8e210eu, 0xe0701000u, 0x80010102u,
    0xbf810000u, 0xbf9f0000u};
alignas(256) std::array<std::uint32_t, Threads> Output{};

std::vector<std::uint32_t> Commands() {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Caller.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{8u << 1u};
    registers(0x213, resource);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 8> data{0u, 0u, 0u, 0u, static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu),
        static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    return words;
}

std::string Dispatch() {
    Output.fill(Untouched);
    const auto words = Commands();
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    std::string failure;
    try {
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    return failure;
}

void CheckTable() {
    AgcDriverWaitIdle_nid_postfix();
    Require(AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(FsTable), 0x60u), "the FS table at 0xfe0040000 is not mapped");
    std::array<std::uint64_t, 12> slots{};
    std::memcpy(slots.data(), reinterpret_cast<const void*>(FsTable), sizeof(slots));
    const auto records = slots[0] & 0xffffffffffffull;
    Require(slots[0] >> 48u == 16u, "FS table slot 0 does not have a stride of 16");
    Require(slots[1] == 0x0000520400000008ull, "FS table slot 0 does not describe eight records with dword3 0x5204");
    for (std::size_t slot = 2; slot < slots.size(); ++slot) Require(slots[slot] == 0u, "an FS table slot other than the first is not zero");
    Require(records % 16u == 0u && AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(records), 128u), "the FS table records are not readable");
    std::array<std::uint64_t, 16> table{};
    std::memcpy(table.data(), reinterpret_cast<const void*>(records), sizeof(table));
    for (std::size_t record = 0; record < 8u; ++record) {
        Require(table[record * 2u] != 0u && table[record * 2u] == table[2u] && table[record * 2u + 1u] == 0u, "an FS table record does not hold the routine and a zero");
    }
    std::array<std::uint32_t, 2> routine{};
    Require(AgcDriver::GuestMemory::Accessible(reinterpret_cast<const void*>(table[2]), sizeof(routine)), "the FS routine is not readable");
    std::memcpy(routine.data(), reinterpret_cast<const void*>(table[2]), sizeof(routine));
    Require(routine[0] == 0xbe80200eu, "the FS routine does not start with s_setpc_b64 s[14:15]");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        CheckTable();
        const auto failure = Dispatch();
        Require(failure.find("not statically resolvable") == std::string::npos, "the call through the FS table did not link: " + failure);
        Require(failure.empty(), "the dispatch failed: " + failure);
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            Require(Output[lane] == Marker, "lane " + std::to_string(lane) + " is " + std::to_string(Output[lane]) + " after the call through the FS table, expected " + std::to_string(Marker));
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("compute FS table call tests passed");
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

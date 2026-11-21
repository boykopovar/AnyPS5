#include "prx/libc/include/WindowsMappings.hpp"
#include <atomic>
#include <cstdio>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("Windows mappings lock regression at " + std::to_string(location.line()));
}

static std::set<std::uintptr_t> Collected(GuestArena::WindowsMappings& mappings, std::uintptr_t address, std::size_t bytes) {
    std::vector<void*> pages(bytes / 4096 + 1);
    std::size_t count = pages.size();
    Require(mappings.Collect(address, bytes, pages.data(), &count, true));
    Require(count < pages.size());
    std::set<std::uintptr_t> result;
    for (std::size_t index = 0; index < count; ++index) result.insert(reinterpret_cast<std::uintptr_t>(pages[index]));
    return result;
}

static void CheckConcurrentCollect() {
    constexpr std::size_t bytes = 2 * 1024 * 1024;
    constexpr std::size_t workers = 8;
    constexpr std::size_t share = bytes / workers;
    constexpr std::size_t rounds = 400;
    auto& mappings = GuestArena::WindowsMappings::Get();
    auto* reserved = static_cast<unsigned char*>(mappings.Reserve(nullptr, 2 * bytes));
    Require(reserved != nullptr);
    const auto start = (reinterpret_cast<std::uintptr_t>(reserved) + bytes - 1) & ~static_cast<std::uintptr_t>(bytes - 1);
    auto* base = reinterpret_cast<unsigned char*>(start);
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, bytes, nullptr);
    Require(section != nullptr);
    mappings.Map(base, bytes, section, 0, PAGE_READWRITE, true, true);
    for (std::size_t at = 0; at < bytes; at += 4096) base[at] = 1;
    Collected(mappings, start, bytes);
    Require(Collected(mappings, start, bytes).empty());

    std::atomic<std::size_t> failures{0};
    std::vector<std::thread> threads;
    for (std::size_t worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&, worker] {
            const auto first = start + worker * share;
            std::uint32_t state = 0x9e3779b9u * static_cast<std::uint32_t>(worker + 1);
            for (std::size_t round = 0; round < rounds; ++round) {
                std::set<std::uintptr_t> expected;
                for (std::size_t store = 0; store < 6; ++store) {
                    state = state * 1664525u + 1013904223u;
                    const auto page = first + (state >> 8) % (share / 4096) * 4096;
                    reinterpret_cast<unsigned char*>(page)[state & 4095] = static_cast<unsigned char>(round);
                    expected.insert(page);
                }
                state = state * 1664525u + 1013904223u;
                const auto hosted = first + (state >> 8) % (share / 4096) * 4096;
                if (!mappings.BeginHostWrite(hosted, 64)) ++failures;
                reinterpret_cast<unsigned char*>(hosted)[0] = static_cast<unsigned char>(round);
                mappings.EndHostWrite(hosted, 64);
                expected.insert(hosted);
                mappings.ForgetKnownZero(first, share);
                if (Collected(mappings, first, share) != expected) ++failures;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(failures.load() == 0);
    Require(Collected(mappings, start, bytes).empty());
    mappings.Reset(base, bytes);
    Require(VirtualFree(reserved, 2 * bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS));
    Require(VirtualFree(reserved, 0, MEM_RELEASE));
    mappings.ForgetSection(section);
    CloseHandle(section);
}

int main() {
    try {
        CheckConcurrentCollect();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("Windows mappings lock tests passed");
    return 0;
}

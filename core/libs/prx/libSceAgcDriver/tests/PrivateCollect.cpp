#include "prx/libc/include/WindowsMappings.hpp"

#include <chrono>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using GuestArena::WindowsMappings;

constexpr std::size_t PieceBytes = 16384;
constexpr std::size_t PageBytes = 4096;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::set<std::uintptr_t> WatchedPages(std::uintptr_t base, std::size_t bytes, std::size_t granule) {
    std::set<std::uintptr_t> written;
    std::vector<void*> pages(granule / PageBytes);
    for (auto at = base; at < base + bytes; at += granule) {
        ULONG_PTR count = pages.size();
        DWORD granularity = 0;
        Require(GetWriteWatch(0, reinterpret_cast<void*>(at), granule, pages.data(), &count, &granularity) == 0, "the reference write-watch read failed");
        for (ULONG_PTR i = 0; i < count; ++i) written.insert(reinterpret_cast<std::uintptr_t>(pages[i]));
    }
    return written;
}

std::set<std::uintptr_t> Collected(std::uintptr_t base, std::size_t bytes, bool clear, std::uint64_t* calls, double* microseconds = nullptr) {
    auto& mappings = WindowsMappings::Get();
    std::vector<void*> pages(bytes / PageBytes);
    std::size_t count = pages.size();
    std::uint64_t before = 0;
    std::uint64_t piecesBefore = 0;
    mappings.CollectWork(&before, &piecesBefore);
    const auto start = std::chrono::steady_clock::now();
    Require(mappings.Collect(base, bytes, pages.data(), &count, clear), "collecting committed private memory failed");
    const auto stop = std::chrono::steady_clock::now();
    std::uint64_t after = 0;
    std::uint64_t piecesAfter = 0;
    mappings.CollectWork(&after, &piecesAfter);
    *calls = after - before;
    if (microseconds != nullptr) *microseconds = std::chrono::duration<double, std::micro>(stop - start).count();
    return {reinterpret_cast<std::uintptr_t*>(pages.data()), reinterpret_cast<std::uintptr_t*>(pages.data()) + count};
}

std::uintptr_t Committed(std::size_t bytes, std::size_t granule) {
    auto& mappings = WindowsMappings::Get();
    auto* base = mappings.Reserve(nullptr, bytes);
    Require(base != nullptr, "cannot reserve a placeholder range");
    mappings.Commit(base, bytes, PAGE_READWRITE, granule, true);
    return reinterpret_cast<std::uintptr_t>(base);
}

void Write(std::uintptr_t address) {
    *reinterpret_cast<volatile std::uint32_t*>(address) = 0x5e000001u;
}

void PiecesOfOneCommit() {
    constexpr std::size_t bytes = std::size_t{256} << 20;
    constexpr std::size_t pieces = bytes / PieceBytes;
    const auto base = Committed(bytes, PieceBytes);
    std::uint64_t calls = 0;
    Collected(base, bytes, true, &calls);
    std::set<std::uintptr_t> expected;
    for (std::size_t i = 0; i < 4096; ++i) {
        const auto at = base + ((i * 2654435761u) % (bytes / PageBytes)) * PageBytes + (i % 64) * 8;
        Write(at);
        expected.insert(at & ~std::uintptr_t{PageBytes - 1});
    }
    Require(WatchedPages(base, bytes, PieceBytes) == expected, "the reference read does not see the scattered writes");
    double microseconds = 0;
    const auto written = Collected(base, bytes, true, &calls, &microseconds);
    std::printf("[private-collect] 256 MiB of 16 KiB pieces: %llu system calls for %zu pieces, %zu written pages, %.0f us\n", static_cast<unsigned long long>(calls), pieces, written.size(), microseconds);
    Require(written == expected, "collect did not return exactly the written pages: " + std::to_string(written.size()) + " of " + std::to_string(expected.size()));
    Require(calls <= pieces, "collect makes more than one system call per piece allocation: " + std::to_string(calls) + " for " + std::to_string(pieces) + " pieces");
    Require(Collected(base, bytes, true, &calls).empty(), "a collect after the reset returned pages");
    WindowsMappings::Get().Reset(reinterpret_cast<void*>(base), bytes);
    std::size_t count = 1;
    void* page = nullptr;
    Require(!WindowsMappings::Get().Collect(base, bytes, &page, &count, false), "collect over released memory succeeded");
    VirtualFree(reinterpret_cast<void*>(base), 0, MEM_RELEASE);
}

void OneAllocationWithMixedProtection() {
    constexpr std::size_t bytes = std::size_t{1} << 20;
    const auto base = Committed(bytes, bytes);
    std::uint64_t calls = 0;
    Collected(base, bytes, true, &calls);
    Write(base + 3 * PageBytes);
    Write(base + 200 * PageBytes);
    WindowsMappings::Get().Commit(reinterpret_cast<void*>(base + 64 * PageBytes), 64 * PageBytes, PAGE_READONLY, bytes, true);
    WindowsMappings::Get().Commit(reinterpret_cast<void*>(base + 160 * PageBytes), 16 * PageBytes, PAGE_READONLY, bytes, true);
    const auto written = Collected(base, bytes, true, &calls);
    std::printf("[private-collect] 1 MiB allocation in five protection regions: %llu system calls\n", static_cast<unsigned long long>(calls));
    Require(written == std::set<std::uintptr_t>{base + 3 * PageBytes, base + 200 * PageBytes}, "collect over mixed protection did not return the written pages");
    Require(calls == 1, "collect over one allocation splits at protection changes: " + std::to_string(calls) + " system calls");
    WindowsMappings::Get().Reset(reinterpret_cast<void*>(base), bytes);
    VirtualFree(reinterpret_cast<void*>(base), 0, MEM_RELEASE);
}

}

int main() {
    try {
        PiecesOfOneCommit();
        OneAllocationWithMixedProtection();
        std::puts("private collect tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "private collect test failed: %s\n", error.what());
        return 1;
    }
}

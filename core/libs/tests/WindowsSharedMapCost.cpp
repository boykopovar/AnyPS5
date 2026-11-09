#define NOMINMAX
#include "prx/libc/include/WindowsMappings.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>

namespace {
using Mappings = GuestArena::WindowsMappings;
constexpr std::size_t PageBytes = 16384;
constexpr int Rounds = 5;

[[noreturn]] void Fail(const char* what) {
    std::fprintf(stderr, "windows shared map cost: %s (error %lu)\n", what, GetLastError());
    std::exit(1);
}

std::size_t MappedViews(const unsigned char* begin, std::size_t bytes) {
    std::set<void*> bases;
    for (auto cursor = begin; cursor < begin + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(cursor, &info, sizeof(info)) != sizeof(info)) Fail("view query failed");
        if (info.Type == MEM_MAPPED) bases.insert(info.AllocationBase);
        cursor = static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize;
    }
    return bases.size();
}

struct Sample {
    double mapMs = 0;
    double releaseMs = 0;
    std::size_t views = 0;
};

Sample MapOnce(std::size_t pages) {
    const auto bytes = pages * PageBytes;
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, static_cast<DWORD>(static_cast<std::uint64_t>(bytes) >> 32), static_cast<DWORD>(bytes), nullptr);
    auto* memory = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, bytes));
    if (section == nullptr || memory == nullptr) Fail("cannot allocate shared memory");
    const auto start = std::chrono::steady_clock::now();
    Mappings::Get().Map(memory, bytes, section, 0, PAGE_READWRITE);
    Sample sample;
    sample.mapMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    sample.views = MappedViews(memory, bytes);
    memory[0] = 1;
    memory[bytes - 1] = 1;
    const auto released = std::chrono::steady_clock::now();
    Mappings::Get().Reset(memory, bytes);
    sample.releaseMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - released).count();
    VirtualFree(memory, 0, MEM_RELEASE);
    CloseHandle(section);
    return sample;
}

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

}

int main() {
    for (const std::size_t pages : {std::size_t{256}, std::size_t{16384}, std::size_t{65536}}) {
        std::vector<double> map;
        std::vector<double> release;
        std::size_t views = 0;
        for (int round = 0; round < Rounds; ++round) {
            const auto sample = MapOnce(pages);
            map.push_back(sample.mapMs);
            release.push_back(sample.releaseMs);
            views = sample.views;
        }
        std::printf("shared map of %zu pages (%zu MiB): map median %.2f ms (%.2f us per page), release median %.2f ms, %zu views\n", pages, pages * PageBytes >> 20, Median(map), Median(map) * 1000.0 / static_cast<double>(pages), Median(release), views);
    }
    return 0;
}

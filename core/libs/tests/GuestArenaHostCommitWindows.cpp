#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>

namespace {

using namespace GuestArena;

constexpr std::size_t Granule = 65536;
constexpr std::size_t HostBytes = 3 * Granule;
unsigned privateMappings = 0;
unsigned accesses = 0;

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(location.line()));
}

void ObservePrivate(std::uintptr_t, std::size_t, std::uint64_t) { ++privateMappings; }
void ObserveAccess(std::uintptr_t, std::size_t) { ++accesses; }

class Child {
public:
    Child(std::string_view mode, std::string_view placement) {
        std::array<wchar_t, 32768> executable{};
        const auto length = GetModuleFileNameW(nullptr, executable.data(), executable.size());
        Require(length != 0 && length < executable.size(), "cannot locate the test executable");
        for (unsigned attempt = 0; attempt != 16; ++attempt) {
            address = 0x2000000000ull + std::uintptr_t{attempt} * 0x40000000ull;
            std::wstring command = L"\"" + std::wstring(executable.data()) + L"\" child "
                + std::wstring(mode.begin(), mode.end()) + L" " + std::wstring(placement.begin(), placement.end())
                + L" " + std::to_wstring(address);
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            Require(CreateProcessW(executable.data(), command.data(), nullptr, nullptr, TRUE,
                CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process), "cannot create the child");
            MEMORY_BASIC_INFORMATION memory{};
            const auto begin = address - Granule;
            if (VirtualQueryEx(process.hProcess, reinterpret_cast<void*>(begin), &memory, sizeof(memory)) != 0
                && memory.State == MEM_FREE
                && reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize >= address + HostBytes + Granule
                && VirtualAllocEx(process.hProcess, reinterpret_cast<void*>(address), HostBytes, MEM_RESERVE, PAGE_READWRITE)
                    == reinterpret_cast<void*>(address)) return;
            Close();
        }
        throw std::runtime_error("cannot place host memory before the child loader starts");
    }
    ~Child() { Close(); }
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;

    void Run(std::string_view mode) {
        if (mode != "reserved") {
            const auto start = mode == "stack" ? address + HostBytes - 8192 : address;
            const auto bytes = mode == "stack" ? 8192 : HostBytes;
            Require(VirtualAllocEx(process.hProcess, reinterpret_cast<void*>(start), bytes, MEM_COMMIT, PAGE_READWRITE)
                == reinterpret_cast<void*>(start), "cannot commit the host fixture");
            std::vector<unsigned char> contents(bytes, 0x6d);
            SIZE_T written = 0;
            Require(WriteProcessMemory(process.hProcess, reinterpret_cast<void*>(start), contents.data(), bytes, &written)
                && written == bytes, "cannot initialize the host fixture");
            if (mode == "stack") {
                DWORD previous = 0;
                Require(VirtualProtectEx(process.hProcess, reinterpret_cast<void*>(start), 4096,
                    PAGE_READWRITE | PAGE_GUARD, &previous), "cannot guard the host stack fixture");
            }
        }
        Require(ResumeThread(process.hThread) != static_cast<DWORD>(-1), "cannot resume the child");
        Require(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "child did not finish");
        DWORD result = 1;
        Require(GetExitCodeProcess(process.hProcess, &result) && result == 0, "child rejected the commit checks");
    }

private:
    PROCESS_INFORMATION process{};
    std::uintptr_t address = 0;

    void Close() {
        if (process.hProcess) {
            if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 1000);
            }
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
            process = {};
        }
    }
};

std::vector<MEMORY_BASIC_INFORMATION> Snapshot(std::uintptr_t start, std::size_t bytes) {
    std::vector<MEMORY_BASIC_INFORMATION> result;
    for (auto address = start; address < start + bytes; address += 4096) {
        MEMORY_BASIC_INFORMATION memory{};
        Require(VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) != 0, "cannot query the fixture");
        result.push_back(memory);
    }
    return result;
}

void CheckGuestCommit() {
    auto* memory = static_cast<unsigned char*>(GuestArenaAllocate_nid_postfix(2 * Granule, Granule));
    GuestArenaCommit_nid_postfix(memory, 2 * Granule, PAGE_READWRITE, Granule);
    Require(privateMappings != 0 && accesses != 0, "guest commit did not notify its observers");
    memory[0] = 37;
    memory[2 * Granule - 1] = 91;
    GuestArenaCommit_nid_postfix(memory, 2 * Granule, PAGE_READWRITE, Granule);
    Require(memory[0] == 37 && memory[2 * Granule - 1] == 91, "guest recommit lost data");
    GuestArenaReset_nid_postfix(memory, 2 * Granule);
    GuestArenaRelease_nid_postfix(memory, 2 * Granule);
}

void CheckHostCommit(std::string_view mode, std::string_view placement, std::uintptr_t host) {
    Require(GuestArenaContains_nid_postfix(reinterpret_cast<void*>(host - Granule), HostBytes + 2 * Granule),
        "host fixture is outside the guest arena");
    Require(GuestArenaHostRegionOverlaps_nid_postfix(host, HostBytes), "host fixture was not recorded at startup");
    Require(!GuestArenaHostRegionOverlaps_nid_postfix(host - Granule, Granule)
        && !GuestArenaHostRegionOverlaps_nid_postfix(host + HostBytes, Granule), "fixture neighbours are not guest placeholders");
    const auto before = Snapshot(host - Granule, HostBytes + 2 * Granule);
    const auto generation = GuestArenaCommitGeneration_nid_postfix();
    GuestArenaSetPrivateMappingObserver_nid_postfix(ObservePrivate);
    GuestArenaSetAccessObserver_nid_postfix(ObserveAccess);
    auto start = host + Granule;
    auto bytes = Granule;
    if (placement == "prefix") { start = host - Granule; bytes = 2 * Granule; }
    else if (placement == "suffix") { start = host + 2 * Granule; bytes = 2 * Granule; }
    else if (placement == "enclosing") { start = host - Granule; bytes = HostBytes + 2 * Granule; }
    bool rejected = false;
    try { GuestArenaCommit_nid_postfix(reinterpret_cast<void*>(start), bytes, PAGE_READWRITE, Granule); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "a guest commit accepted host-owned memory");
    Require(GuestArenaCommitGeneration_nid_postfix() == generation, "a rejected commit changed the mapping generation");
    Require(privateMappings == 0 && accesses == 0, "a rejected commit notified mapping observers");
    GuestArenaCommit_nid_postfix(reinterpret_cast<void*>(host + Granule), 0, PAGE_READWRITE, Granule);
    Require(privateMappings == 0, "an empty commit created a mapping");
    const auto after = Snapshot(host - Granule, HostBytes + 2 * Granule);
    Require(before.size() == after.size(), "the fixture query changed size");
    for (std::size_t index = 0; index < before.size(); ++index) {
        const auto& a = before[index];
        const auto& b = after[index];
        Require(a.BaseAddress == b.BaseAddress && a.AllocationBase == b.AllocationBase && a.RegionSize == b.RegionSize
            && a.AllocationProtect == b.AllocationProtect && a.State == b.State && a.Protect == b.Protect && a.Type == b.Type,
            "a rejected commit changed native memory");
    }
    if (mode != "reserved") {
        const auto start = mode == "stack" ? host + HostBytes - 4096 : host;
        const auto bytes = mode == "stack" ? 4096 : HostBytes;
        const auto* contents = reinterpret_cast<const unsigned char*>(start);
        for (std::size_t index = 0; index < bytes; ++index) Require(contents[index] == 0x6d, "host data was overwritten");
    }
    privateMappings = 0;
    accesses = 0;
    CheckGuestCommit();
    GuestArenaSetPrivateMappingObserver_nid_postfix(nullptr);
    GuestArenaSetAccessObserver_nid_postfix(nullptr);
    std::puts("host regions preserved; owned guest commits and recommits succeed");
}

}

int main(int argc, char** argv) {
    try {
        const bool child = argc == 5 && std::string_view(argv[1]) == "child";
        Require(child || argc == 3, "expected fixture mode and overlap placement");
        const std::string_view mode = argv[child ? 2 : 1];
        const std::string_view placement = argv[child ? 3 : 2];
        Require(mode == "reserved" || mode == "committed" || mode == "stack", "unknown fixture mode");
        Require(placement == "inside" || placement == "prefix" || placement == "suffix" || placement == "enclosing",
            "unknown overlap placement");
        if (child) CheckHostCommit(mode, placement, std::stoull(argv[4]));
        else { Child process(mode, placement); process.Run(mode); }
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "%s\n", failure.what());
        return 1;
    }
    return 0;
}

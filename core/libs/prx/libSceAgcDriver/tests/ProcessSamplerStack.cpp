#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <windows.h>

namespace {

std::atomic<bool> ready{false};
std::atomic<bool> done{false};
std::atomic<std::uint64_t> caller{0};

[[gnu::noinline]] void Busy() {
    caller.store(reinterpret_cast<std::uint64_t>(__builtin_return_address(0)));
    ready.store(true);
    while (!done.load(std::memory_order_relaxed)) std::atomic_signal_fence(std::memory_order_seq_cst);
}

[[gnu::noinline]] void Parent() {
    Busy();
    ready.store(false);
}

std::atomic<std::uint64_t> lastWord{0};

bool Accepted(const void* address) {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    const auto at = reinterpret_cast<std::uint64_t>(address);
    if (!AgcDriver::ReadableStackRegion(at, low, high)) return false;
    std::uint64_t last = 0;
    std::memcpy(&last, reinterpret_cast<const void*>(high - sizeof(last)), sizeof(last));
    lastWord.store(last);
    return low <= at && at < high;
}

bool Rejected(const void* address) {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    return !AgcDriver::ReadableStackRegion(reinterpret_cast<std::uint64_t>(address), low, high) && low == 0 && high == 0;
}

bool HasProtection(const void* address, DWORD expected) {
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(address, &memory, sizeof(memory)) != 0 && memory.Protect == expected;
}

bool Fail(const char* what, DWORD protection) {
    std::fprintf(stderr, "%s (protection 0x%lx)\n", what, static_cast<unsigned long>(protection));
    return false;
}

bool CheckAllocated(DWORD allocation, DWORD protection, bool readable) {
    void* page = VirtualAlloc(nullptr, 65536, allocation, protection);
    if (page == nullptr) return Fail("cannot allocate the test region", protection);
    const auto* inside = static_cast<const char*>(page) + 4096;
    const bool committed = (allocation & MEM_COMMIT) != 0;
    const bool passed = (!committed || HasProtection(page, protection)) && (readable ? Accepted(inside) : Rejected(inside));
    VirtualFree(page, 0, MEM_RELEASE);
    return passed || Fail(readable ? "a readable region was rejected" : "an unreadable region was accepted", protection);
}

bool CheckMapped(HANDLE section, DWORD access, DWORD protection) {
    void* view = MapViewOfFile(section, access, 0, 0, 65536);
    if (view == nullptr) return Fail("cannot map the copy-on-write view", protection);
    const bool passed = HasProtection(view, protection) && Accepted(view);
    UnmapViewOfFile(view);
    return passed || Fail("a readable mapped view was rejected", protection);
}

bool CheckRegions() {
    for (const DWORD protection : {PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE}) {
        if (!CheckAllocated(MEM_RESERVE | MEM_COMMIT, protection, true)) return false;
    }
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, 65536, nullptr);
    if (section == nullptr) return Fail("cannot create the copy-on-write section", PAGE_EXECUTE_READWRITE);
    const bool mapped = CheckMapped(section, FILE_MAP_COPY, PAGE_WRITECOPY) && CheckMapped(section, FILE_MAP_COPY | FILE_MAP_EXECUTE, PAGE_EXECUTE_WRITECOPY);
    CloseHandle(section);
    if (!mapped) return false;
    for (const DWORD protection : {static_cast<DWORD>(PAGE_EXECUTE), static_cast<DWORD>(PAGE_NOACCESS), static_cast<DWORD>(PAGE_READWRITE | PAGE_GUARD)}) {
        if (!CheckAllocated(MEM_RESERVE | MEM_COMMIT, protection, false)) return false;
    }
    if (!CheckAllocated(MEM_RESERVE, PAGE_READWRITE, false)) return false;
    return Rejected(nullptr) || Fail("the null page was accepted", 0);
}

}

int main() {
    if (!CheckRegions()) return 1;
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-process-stack-" + std::to_string(GetCurrentProcessId()) + ".txt");
    _putenv_s("APS5_SAMPLE_THREADS", path.string().c_str());
    _putenv_s("APS5_SAMPLE_WORKER", "");
    _putenv_s("APS5_SAMPLE_WORKER_QUEUE", "0x50");
    std::thread target(Parent);
    while (!ready.load()) std::this_thread::yield();
    AgcDriver::StartWorkerSampler(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    AgcDriver::StopWorkerSampler();
    done.store(true);
    target.join();
    const auto pc = caller.load();
    DWORD64 imageBase = 0;
    if (RtlLookupFunctionEntry(pc, &imageBase, nullptr) != nullptr) {
        std::filesystem::remove(path);
        std::fprintf(stderr, "fixture unexpectedly has native Windows unwind metadata\n");
        return 1;
    }
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(pc), &module)) return 1;
    wchar_t imagePath[MAX_PATH]{};
    if (GetModuleFileNameW(module, imagePath, MAX_PATH) == 0) return 1;
    const auto imageName = std::filesystem::path(imagePath).filename().string();
    const auto offset = pc - reinterpret_cast<std::uint64_t>(module);
    std::ifstream file(path);
    std::string line;
    bool found = false;
    std::uint64_t inclusive = 0;
    while (std::getline(file, line)) {
        char name[1024]{};
        unsigned long long address = 0, self = 0, total = 0;
        if (std::sscanf(line.c_str(), " %1023s %llx %llu %llu", name, &address, &self, &total) == 4 && name == imageName && address == offset && total != 0) {
            found = true;
            inclusive += total;
        }
    }
    file.close();
    std::filesystem::remove(path);
    std::printf("process sampler caller: %s (%llu inclusive samples)\n", found ? "passed" : "missing", static_cast<unsigned long long>(inclusive));
    return found ? 0 : 1;
}

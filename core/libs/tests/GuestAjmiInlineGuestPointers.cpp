#include "prx/libSceAjmi/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool ok, const char* description) {
    if (ok) return;
    std::fprintf(stderr, "AJMI inline guest pointer: %s\n", description);
    std::abort();
}

template <typename Operation>
void RequireRejected(Operation call, const char* description) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, description);
}

char* GuardedPageEnd() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* bytes = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(bytes != nullptr, "allocate guard pages");
    DWORD previous = 0;
    Require(VirtualProtect(bytes + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect second page");
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0, "get page size");
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* memory = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(memory != MAP_FAILED, "allocate guard pages");
    auto* bytes = static_cast<char*>(memory);
    Require(mprotect(bytes + pageSize, pageSize, PROT_NONE) == 0, "protect second page");
#endif
    return bytes + pageSize;
}

}

int main() {
    std::array<std::uint8_t, 32> job{};
    job.fill(0xA5);
    const std::array<std::uint8_t, 5> data{4, 6, 8, 10, 12};
    const void* payload = nullptr;
    errno = 71;
    RequireRejected([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(reinterpret_cast<void*>(0x10), nullptr, 0, nullptr); }, "unreadable destination header accepted");
    Require(errno == 71, "invalid destination changed errno");
    RequireRejected([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), reinterpret_cast<const void*>(0x10), 5, nullptr); }, "unreadable data input accepted");
    Require(errno == 71, "invalid source changed errno");
    RequireRejected([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), data.data(), data.size(), reinterpret_cast<const void**>(0x10)); }, "unwritable payload address accepted");
    Require(errno == 71, "invalid address output changed errno");

    job.fill(0xA5);
    void* next = sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), data.data(), data.size(), &payload);
    Require(next == job.data() + 16, "next chunk pointer changed");
    Require(payload == job.data() + 8, "payload pointer changed");
    const std::array<std::uint8_t, 16> expected{0xA7, 0xA5, 0xA5, 0xA5, 8, 0, 0, 0, 4, 6, 8, 10, 12, 0xA5, 0xA5, 0xA5};
    Require(std::memcmp(job.data(), expected.data(), expected.size()) == 0, "measured buffer bytes changed");
    Require(job[16] == 0xA5, "output exceeded the expected chunk");
    Require(errno == 71, "valid inline encoding changed errno");

    char* end = GuardedPageEnd();
    std::memset(end - 16, 0xA5, 16);
    Require(sceAjmiBatchJobInlineBuffer_nid_no_patch(end - 16, data.data(), data.size(), nullptr) == end, "batch at writable page end was refused");
    RequireRejected([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(end - 12, data.data(), data.size(), nullptr); }, "batch writing past page boundary was accepted");
    std::memcpy(end - 4, data.data(), 4);
    RequireRejected([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), end - 4, 5, nullptr); }, "input crossing page boundary was accepted");

    std::puts("AJMI inline guest pointer tests passed");
}

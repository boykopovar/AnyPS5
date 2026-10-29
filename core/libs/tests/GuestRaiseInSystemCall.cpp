#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <string_view>
#include <thread>
#include <windows.h>

static constexpr int SigUsr1 = 30;
static constexpr int Raises = 10;
static constexpr DWORD PipeBytes = 4096;
static constexpr std::uint64_t BlockBytes = 4u * PipeBytes;
static constexpr std::size_t RipOffset = 0xe0;
static constexpr ULONG ThreadSuspendCount = 35;
static void Require(bool value) { if (!value) std::abort(); }

static std::FILE* shared = nullptr;
static std::atomic<int> deliveries{0};
static std::atomic<int> released{0};
static std::atomic<int> written{0};
static std::atomic<int> blocked{0};
static std::atomic<HANDLE> writerThread{nullptr};
static bool delayReturn = false;

static bool InTestImage(std::uint64_t rip) {
    const auto base = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return rip >= base && rip - base < nt->OptionalHeader.SizeOfImage;
}

static bool Suspended(HANDLE thread) {
    using Query = LONG(NTAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    static const auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    ULONG count = 0;
    Require(query != nullptr && query(thread, ThreadSuspendCount, &count, sizeof(count), nullptr) >= 0);
    return count > 0;
}

static void APS5_VABI Handler(int, void* context) {
    const auto delivery = deliveries.fetch_add(1) + 1;
    std::uint64_t rip = 0;
    std::memcpy(&rip, static_cast<const std::byte*>(context) + RipOffset, sizeof(rip));
    Require(InTestImage(rip));
    while (released.load() < delivery) std::this_thread::yield();
}

static void* APS5_VABI Writer(void*) {
    HANDLE self = nullptr;
    Require(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self, THREAD_QUERY_INFORMATION, FALSE, 0) != 0);
    writerThread.store(self);
    const std::string block(BlockBytes, 'x');
    for (int raise = 1; raise <= Raises; ++raise) {
        Require(std::fwrite(block.data(), 1, block.size(), shared) == block.size());
        if (delayReturn) {
            volatile std::uint64_t spin = 0;
            for (std::uint64_t i = 0; i < 100000000; ++i) spin = spin + i;
        }
        written.store(raise);
        while (deliveries.load() < raise) {
        }
    }
    return nullptr;
}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    Require(mode.empty() || mode == "--delay-raise" || mode == "--delay-return");
    delayReturn = mode == "--delay-return";
    std::atomic<bool> done{false};
    std::thread watchdog([&done] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
        while (!done.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!done.load()) std::abort();
    });
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    Require(CreatePipe(&read, &write, nullptr, PipeBytes) != 0);
    shared = _fdopen(_open_osfhandle(reinterpret_cast<intptr_t>(write), _O_WRONLY | _O_BINARY), "wb");
    Require(shared != nullptr && std::setvbuf(shared, nullptr, _IONBF, 0) == 0);
    std::thread reader([&] {
        char chunk[PipeBytes];
        DWORD got = 0;
        std::uint64_t total = 0;
        int block = 0;
        while (ReadFile(read, chunk, sizeof(chunk), &got, nullptr) != 0 && got != 0) {
            total += got;
            if (block == Raises || total <= static_cast<std::uint64_t>(block) * (BlockBytes + 1)) continue;
            blocked.store(++block);
            while (!Suspended(writerThread.load())) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            Require(written.load() < block);
        }
    });
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, nullptr, "pipe writer") == 0);
    for (int raise = 1; raise <= Raises; ++raise) {
        while (blocked.load() < raise) std::this_thread::yield();
        if (mode == "--delay-raise") std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Require(sceKernelRaiseException(writer, SigUsr1) == 0);
        while (deliveries.load() < raise) std::this_thread::yield();
        Require(std::fputc('y', shared) == 'y');
        released.store(raise);
    }
    Require(scePthreadJoin(writer, nullptr) == 0);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    std::fclose(shared);
    reader.join();
    CloseHandle(read);
    CloseHandle(writerThread.load());
    done.store(true);
    watchdog.join();
}

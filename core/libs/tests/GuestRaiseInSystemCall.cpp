#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <thread>
#include <windows.h>

static constexpr int SigUsr1 = 30;
static constexpr int Raises = 10;
static constexpr DWORD PipeBytes = 4096;
static void Require(bool value) { if (!value) std::abort(); }

static std::FILE* shared = nullptr;
static std::atomic<int> deliveries{0};
static std::atomic<int> released{0};
static std::atomic<bool> stop{false};
static std::atomic<int> blocks{0};

static void APS5_VABI Handler(int, void*) {
    const auto delivery = deliveries.fetch_add(1) + 1;
    while (released.load() < delivery) std::this_thread::yield();
}

static void* APS5_VABI Writer(void*) {
    const std::string block(16u * PipeBytes, 'x');
    while (!stop.load()) {
        Require(std::fwrite(block.data(), 1, block.size(), shared) == block.size());
        blocks.fetch_add(1);
        volatile std::uint64_t spin = 0;
        for (std::uint64_t i = 0; i < 200000; ++i) spin = spin + i;
    }
    return nullptr;
}

int main() {
    std::atomic<bool> done{false};
    std::thread watchdog([&done] {
        for (int i = 0; i < 25000 && !done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!done.load()) std::abort();
    });
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    Require(CreatePipe(&read, &write, nullptr, PipeBytes) != 0);
    shared = _fdopen(_open_osfhandle(reinterpret_cast<intptr_t>(write), _O_WRONLY | _O_BINARY), "wb");
    Require(shared != nullptr);
    std::atomic<bool> draining{true};
    std::thread reader([&] {
        char chunk[PipeBytes];
        DWORD got = 0;
        while (ReadFile(read, chunk, sizeof(chunk), &got, nullptr) != 0 && got != 0) {
            if (draining.load()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, nullptr, "pipe writer") == 0);
    while (blocks.load() == 0) std::this_thread::yield();
    for (int raise = 1; raise <= Raises; ++raise) {
        Require(sceKernelRaiseException(writer, SigUsr1) == 0);
        while (deliveries.load() < raise) std::this_thread::yield();
        Require(std::fputc('y', shared) == 'y');
        released.store(raise);
    }
    stop.store(true);
    draining.store(false);
    Require(scePthreadJoin(writer, nullptr) == 0);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    std::fclose(shared);
    reader.join();
    CloseHandle(read);
    done.store(true);
    watchdog.join();
}

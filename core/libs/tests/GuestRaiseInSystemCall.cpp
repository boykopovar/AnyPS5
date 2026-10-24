#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <thread>
#include <windows.h>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread*, const PthreadAttr*, PthreadEntry, void*, const char*);
int APS5_VABI scePthreadJoin(Pthread, void**);
int APS5_VABI sceKernelInstallExceptionHandler(int, void*);
int APS5_VABI sceKernelRemoveExceptionHandler(int);
int APS5_VABI sceKernelRaiseException(Pthread, int);
}

static constexpr int SigUsr1 = 30;
static constexpr int Raises = 10;
static constexpr DWORD PipeBytes = 4096;
static void Require(bool value) { if (!value) std::abort(); }

static std::FILE* shared = nullptr;
static std::atomic<int> deliveries{0};
static std::atomic<int> released{0};
static std::atomic<int> writing{0};

static void APS5_VABI Handler(int, void*) {
    const auto delivery = deliveries.fetch_add(1) + 1;
    while (released.load() < delivery) std::this_thread::yield();
}

static void* APS5_VABI Writer(void*) {
    const std::string block(2u * PipeBytes, 'x');
    for (int raise = 1; raise <= Raises; ++raise) {
        writing.store(raise);
        Require(std::fwrite(block.data(), 1, block.size(), shared) == block.size());
        while (deliveries.load() < raise) {
        }
    }
    return nullptr;
}

int main() {
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
    Require(shared != nullptr);
    std::atomic<bool> draining{true};
    std::thread reader([&] {
        char chunk[PipeBytes];
        DWORD got = 0;
        while (ReadFile(read, chunk, sizeof(chunk), &got, nullptr) != 0 && got != 0) {
            if (draining.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, nullptr, "pipe writer") == 0);
    for (int raise = 1; raise <= Raises; ++raise) {
        while (writing.load() < raise) std::this_thread::yield();
        Require(sceKernelRaiseException(writer, SigUsr1) == 0);
        while (deliveries.load() < raise) std::this_thread::yield();
        Require(std::fputc('y', shared) == 'y');
        released.store(raise);
    }
    draining.store(false);
    Require(scePthreadJoin(writer, nullptr) == 0);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
    std::fclose(shared);
    reader.join();
    CloseHandle(read);
    done.store(true);
    watchdog.join();
}

#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "Testing.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int UserId = 0x10000000;
constexpr int InvalidHandle = static_cast<int>(0x80260101);
constexpr int InvalidPointer = static_cast<int>(0x80260105);
constexpr int InvalidState = static_cast<int>(0x80260109);
constexpr int TestErrno = EBUSY;

void ClobberErrnoHook(std::uintptr_t, std::size_t) {
    errno = EIO;
}

struct ScopedErrnoClobberHooks {
    ScopedErrnoClobberHooks() {
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(&ClobberErrnoHook);
        GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(&ClobberErrnoHook);
    }
    ~ScopedErrnoClobberHooks() {
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
        GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    }
};

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 3 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
        Require(VirtualProtect(memory + 2 * page, page, PAGE_READONLY, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 3 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
        Require(mprotect(memory + 2 * page, page, PROT_READ) == 0);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 3 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const { return memory + page; }
    void* ReadOnly() const { return memory + 2 * page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

template <typename Call>
void ExpectErrno(Call call, int expected) {
    errno = TestErrno;
    Require(call() == expected);
    Require(errno == TestErrno);
}

void TestHandleStatusInfo(const GuardPage& guard) {
    std::array<std::uint8_t, 64> statusBuffer{};
    void* const infos[] = {nullptr, statusBuffer.data(), reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)),
        guard.Unreadable(), guard.ReadOnly()};

    for (const int badHandle : {-1, 0, 1, 8, 0x30000008}) {
        for (void* info : infos) {
            ExpectErrno([&] { return sceAudioInGetHandleStatusInfo(badHandle, info); }, InvalidHandle);
        }
    }

    const int handle = sceAudioInOpen(UserId, 0, 0, 128, 48000, 2);
    Require(handle > 0);
    errno = TestErrno;
    Require(Throws([&] { sceAudioInGetHandleStatusInfo(handle, statusBuffer.data()); }));
    Require(errno == TestErrno);

    Require(sceAudioInClose(handle) == 0);
    for (void* info : infos) {
        ExpectErrno([&] { return sceAudioInGetHandleStatusInfo(handle, info); }, InvalidState);
    }
}

void TestGainErrors(const GuardPage& guard) {
    float* const badPointers[] = {
        nullptr,
        reinterpret_cast<float*>(static_cast<std::uintptr_t>(0x10)),
        static_cast<float*>(guard.Unreadable()),
        static_cast<float*>(guard.ReadOnly()),
    };

    for (const int badHandle : {-1, 0, 1, 0x30000008}) {
        float gain = -2.5f;
        ExpectErrno([&] { return sceAudioInGetGain(badHandle, &gain); }, InvalidHandle);
        Require(gain == 1.0f);
        for (float* pointer : badPointers) {
            ExpectErrno([&] { return sceAudioInGetGain(badHandle, pointer); }, InvalidPointer);
        }
    }

    const int handle = sceAudioInOpen(UserId, 0, 0, 128, 48000, 2);
    Require(handle > 0);
    float gain = 0.0f;
    ExpectErrno([&] { return sceAudioInGetGain(handle, &gain); }, 0);
    Require(gain == 1.0f);
    for (float* pointer : badPointers) {
        ExpectErrno([&] { return sceAudioInGetGain(handle, pointer); }, InvalidPointer);
    }

    Require(sceAudioInClose(handle) == 0);
    gain = 0.25f;
    ExpectErrno([&] { return sceAudioInGetGain(handle, &gain); }, InvalidState);
    Require(gain == 1.0f);
    for (float* pointer : badPointers) {
        ExpectErrno([&] { return sceAudioInGetGain(handle, pointer); }, InvalidPointer);
    }
}

void TestInputErrors(const GuardPage& guard) {
    std::vector<std::uint8_t> buffer(128 * 4 + 1, 0xCC);
    void* const badBuffers[] = {
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)),
        guard.Unreadable(),
        guard.ReadOnly(),
    };

    for (const int badHandle : {-1, 0, 1, 0x30000008}) {
        ExpectErrno([&] { return sceAudioInInput(badHandle, nullptr); }, InvalidHandle);
        ExpectErrno([&] { return sceAudioInInput(badHandle, buffer.data()); }, InvalidHandle);
        for (void* bad : badBuffers) {
            ExpectErrno([&] { return sceAudioInInput(badHandle, bad); }, InvalidHandle);
        }
    }

    const int handle = sceAudioInOpen(UserId, 0, 0, 128, 48000, 2);
    Require(handle > 0);

    ExpectErrno([&] { return sceAudioInInput(handle, nullptr); }, 0);
    for (void* bad : badBuffers) {
        ExpectErrno([&] { return sceAudioInInput(handle, bad); }, InvalidPointer);
    }
    ExpectErrno([&] { return sceAudioInInput(handle, buffer.data()); }, 128);
    for (std::size_t i = 0; i < 128 * 4; ++i) Require(buffer[i] == 0);
    Require(buffer[128 * 4] == 0xCC);

    Require(sceAudioInClose(handle) == 0);
    ExpectErrno([&] { return sceAudioInInput(handle, nullptr); }, InvalidState);
    ExpectErrno([&] { return sceAudioInInput(handle, buffer.data()); }, InvalidState);
}

void TestConcurrentHandleStatusInfo() {
    const int slotHandle = sceAudioInOpen(UserId, 0, 0, 128, 48000, 2);
    Require(slotHandle > 0);
    Require(sceAudioInClose(slotHandle) == 0);

    std::atomic<bool> running{true};
    std::thread churn([&] {
        for (int iteration = 0; iteration < 200; ++iteration) {
            const int handle = sceAudioInOpen(UserId, 0, 0, 128, 48000, 2);
            Require(handle == slotHandle);
            Require(sceAudioInClose(handle) == 0);
        }
        running.store(false, std::memory_order_release);
    });

    while (running.load(std::memory_order_acquire)) {
        errno = TestErrno;
        try {
            const int result = sceAudioInGetHandleStatusInfo(slotHandle, nullptr);
            Require(result == InvalidState);
        } catch (const std::exception&) {
        }
        Require(errno == TestErrno);
    }

    churn.join();
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "none");
    Require(sceAudioInInit() == 0);
    GuardPage guard;
    {
        const ScopedErrnoClobberHooks errnoHooks;
        TestHandleStatusInfo(guard);
        TestGainErrors(guard);
        TestInputErrors(guard);
    }
    TestConcurrentHandleStatusInfo();
    std::puts("AudioIn error and errno tests passed");
    return 0;
}

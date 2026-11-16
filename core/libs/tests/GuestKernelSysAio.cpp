#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
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

}

extern "C" {

void AioHoldWorkerForTest_nid_no_patch(bool hold);
void AioHoldSocketForTest_nid_no_patch(bool hold);
std::size_t AioSocketsHeldForTest_nid_no_patch();
}

namespace {

constexpr int Ebadf = 9;
constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int Einprogress = 36;
constexpr int Eopnotsupp = 45;
constexpr int Ecanceled = 85;
constexpr int ORdonly = 0;
constexpr int ORdwr = 2;
constexpr int OCreat = 0x200;
constexpr int OSync = 0x80;
constexpr int ODsync = 0x1000000;
constexpr int AioCanceled = 1;
constexpr int AioAllDone = 3;
constexpr int Epipe = 32;
constexpr int Eperm = 1;
constexpr int Enoent = 2;
constexpr int SigevSignal = 1;
constexpr int SigevKevent = 3;
constexpr int SigevThreadId = 4;
constexpr std::int16_t EvfiltAio = -3;
constexpr std::uint16_t EvAdd = 0x1;
constexpr std::uint16_t EvDelete = 0x2;
constexpr std::uint16_t EvOneshot = 0x10;
constexpr std::uint16_t EvClear = 0x20;
constexpr std::uint16_t EvEof = 0x8000;
constexpr int Sigpipe = 13;
constexpr int Sigurg = 16;
constexpr int Sigusr1 = 30;
constexpr int Sigusr2 = 31;
constexpr int SigBlock = 1;
constexpr int SigUnblock = 2;
constexpr int SiAsyncio = 0x10004;
constexpr int AfUnix = 1;
constexpr int SockStream = 1;
constexpr int AioNotCanceled = 2;
constexpr int FSetfl = 4;
constexpr int ONonblock = 4;
constexpr int MsgDontWait = 0x80;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

class GuardedMemory {
public:
    GuardedMemory() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardedMemory() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, 2 * page) == 0);
#endif
    }

    void ProtectReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, page, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory, page, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }

    const KernelTimespec* UnreadableTimeout() const {
        return reinterpret_cast<const KernelTimespec*>(memory + page);
    }

    const KernelTimespec* CrossingTimeout() const {
        return reinterpret_cast<const KernelTimespec*>(memory + page - sizeof(std::int64_t));
    }

    const KernelTimespec* UnalignedCrossingTimeout() const {
        return reinterpret_cast<const KernelTimespec*>(memory + page - 1);
    }

    GuestAiocb** UnreadableResult() const {
        return reinterpret_cast<GuestAiocb**>(memory + page);
    }

    GuestAiocb** CrossingResult() const {
        return reinterpret_cast<GuestAiocb**>(memory + page - sizeof(void*) / 2);
    }

    GuestAiocb** UnalignedCrossingResult() const {
        return reinterpret_cast<GuestAiocb**>(memory + page - 1);
    }

    GuestAiocb** WritableResult() const {
        return reinterpret_cast<GuestAiocb**>(memory);
    }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

GuestAiocb Request(int descriptor, void* buffer, std::uint64_t bytes, std::int64_t offset = 0) {
    GuestAiocb request{};
    request.fildes = descriptor;
    request.buf = buffer;
    request.nbytes = bytes;
    request.offset = offset;
    return request;
}

void Wait(GuestAiocb& request) {
    const GuestAiocb* list[] = {&request};
    Require(aio_suspend_nid_postfix(list, 1, nullptr) == 0);
}

std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    return system.dwPageSize;
#else
    return static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
#endif
}

char* Pages(std::size_t bytes) {
#ifdef _WIN32
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr);
#else
    auto* memory = static_cast<char*>(::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED);
#endif
    return memory;
}

void Protect(void* address, std::size_t size, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(address, size, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(::mprotect(address, size, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

void BeforeUse() {
    GuestAiocb request{};
    const GuestAiocb* list[] = {&request};
    Require(aio_error_nid_postfix(&request) == Einval);
    Require(aio_return_nid_postfix(&request) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(list, 1, nullptr) == -1 && Error() == Eagain);
}

void Validation(int file, int readOnly) {
    char buffer[8] = {};
    Require(aio_read_nid_postfix(nullptr) == -1 && Error() == Efault);
    Require(aio_read_nid_postfix(reinterpret_cast<GuestAiocb*>(std::uintptr_t{0x10})) == -1 && Error() == Efault);

    const std::size_t page = PageSize();
    char* memory = Pages(page * 4);
    Protect(memory + page, page, false);
    auto* straddling = reinterpret_cast<GuestAiocb*>(memory + page + 40 - sizeof(GuestAiocb));
    Require(aio_read_nid_postfix(straddling) == -1 && Error() == Efault && straddling->status == -1 && straddling->error == Efault);

    auto* locked = reinterpret_cast<GuestAiocb*>(memory + page * 2);
    *locked = Request(file, buffer, sizeof(buffer));
    Protect(locked, page, true);
    Require(aio_read_nid_postfix(locked) == -1 && Error() == Einval && locked->status == 0 && aio_error_nid_postfix(locked) == Einval);

    auto* finished = reinterpret_cast<GuestAiocb*>(memory + page * 3);
    *finished = Request(file, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(finished) == 0);
    Wait(*finished);
    Protect(finished, page, true);
    Require(aio_return_nid_postfix(finished) == 0 && finished->status == -1 && finished->error == Einprogress);
    Require(aio_return_nid_postfix(finished) == -1 && Error() == Einval);

    auto bad = Request(-1, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&bad) == -1 && Error() == Ebadf);
    Require(bad.status == -1 && bad.error == Ebadf && aio_error_nid_postfix(&bad) == Ebadf);

    auto readOnlyWrite = Request(readOnly, buffer, sizeof(buffer));
    Require(aio_write_nid_postfix(&readOnlyWrite) == -1 && Error() == Ebadf);

    auto negative = Request(file, buffer, sizeof(buffer), -1);
    Require(aio_read_nid_postfix(&negative) == -1 && Error() == Einval && negative.error == Einval);

    auto huge = Request(file, buffer, std::uint64_t{std::numeric_limits<int>::max()} + 1);
    Require(aio_read_nid_postfix(&huge) == -1 && Error() == Einval && huge.error == 0);

    auto notify = Request(file, buffer, sizeof(buffer));
    notify.sigevent.notify = 2;
    Require(aio_read_nid_postfix(&notify) == -1 && Error() == Einval && notify.error == Einval);
    notify.sigevent = {1, 0, 0, {}};
    Require(aio_read_nid_postfix(&notify) == -1 && Error() == Einval);

    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    auto piped = Request(pipe[0], buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&piped) == -1 && Error() == Eopnotsupp && piped.error == Eopnotsupp);
    auto pipeSync = Request(pipe[1], nullptr, 0);
    Require(aio_fsync_nid_postfix(OSync, &pipeSync) == -1 && Error() == Einval);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);

    auto sync = Request(file, nullptr, 0);
    Require(aio_fsync_nid_postfix(0, &sync) == -1 && Error() == Einval);
    Require(aio_fsync_nid_postfix(ODsync, &sync) == -1 && Error() == Einval);
}

void ErrorOfUnreadable() {
    constexpr int Sentinel = 12345;
    *__error_nid_postfix() = Sentinel;
    Require(aio_error_nid_postfix(nullptr) == -1 && Error() == Sentinel);
    Require(aio_error_nid_postfix(reinterpret_cast<const GuestAiocb*>(std::uintptr_t{0x10})) == -1 && Error() == Sentinel);

    const std::size_t page = PageSize();
    char* memory = Pages(page * 3);
    char* hidden = memory + page;
    Protect(hidden, page, false);
    Require(aio_error_nid_postfix(reinterpret_cast<const GuestAiocb*>(hidden)) == -1 && Error() == Sentinel);

    const std::int64_t failed = -1;
    std::memcpy(hidden - sizeof(failed), &failed, sizeof(failed));
    Require(aio_error_nid_postfix(reinterpret_cast<const GuestAiocb*>(hidden - offsetof(GuestAiocb, error))) == -1 && Error() == Sentinel);

    const std::int64_t recorded = Ebadf;
    std::memcpy(hidden + page, &recorded, sizeof(recorded));
    Require(aio_error_nid_postfix(reinterpret_cast<const GuestAiocb*>(hidden + page - offsetof(GuestAiocb, error))) == Ebadf && Error() == Sentinel);
}

void ReadAndWrite(int file) {
    char text[] = "asynchronous";
    auto write = Request(file, text, 12, 4);
    Require(aio_write_nid_postfix(&write) == 0);
    Wait(write);
    Require(aio_error_nid_postfix(&write) == 0);
    Require(aio_return_nid_postfix(&write) == 12 && write.status == 12 && write.error == 0);
    Require(aio_return_nid_postfix(&write) == -1 && Error() == Einval);
    Require(aio_error_nid_postfix(&write) == Einval);

    char read[16] = {};
    auto request = Request(file, read, sizeof(read), 6);
    Require(aio_read_nid_postfix(&request) == 0);
    Wait(request);
    Require(aio_return_nid_postfix(&request) == 10 && std::memcmp(read, "ynchronous", 10) == 0);

    auto empty = Request(file, read, 0, 0);
    Require(aio_read_nid_postfix(&empty) == 0);
    Wait(empty);
    Require(aio_return_nid_postfix(&empty) == 0);

    const GuestAiocb* none[] = {&request};
    Require(aio_suspend_nid_postfix(none, 1, nullptr) == 0);
    Require(aio_suspend_nid_postfix(none, 0, nullptr) == 0);
    Require(aio_suspend_nid_postfix(none, -1, nullptr) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(none, 257, nullptr) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(nullptr, 1, nullptr) == -1 && Error() == Efault);
    const KernelTimespec badTime{0, 1000000000};
    Require(aio_suspend_nid_postfix(none, 1, &badTime) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(none, 1, reinterpret_cast<const KernelTimespec*>(0x10)) == -1 && Error() == Efault);
    const GuestAiocb* second[] = {nullptr, &request};
    const KernelTimespec soon{0, 5000000};
    Require(aio_suspend_nid_postfix(second, 2, &soon) == 0);
}

void Cancel(int file) {
    char buffer[4] = {};
    AioHoldWorkerForTest_nid_no_patch(true);
    auto first = Request(file, buffer, sizeof(buffer));
    auto second = Request(file, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&first) == 0 && aio_read_nid_postfix(&second) == 0);
    Require(first.error == Einprogress && aio_error_nid_postfix(&first) == Einprogress);
    const GuestAiocb* list[] = {&first};
    const KernelTimespec brief{0, 5000000};
    Require(aio_suspend_nid_postfix(list, 1, &brief) == -1 && Error() == Eagain);
    Require(aio_suspend_nid_postfix(reinterpret_cast<const GuestAiocb* const*>(0x10), 1, &brief) == -1 && Error() == Efault);

    Require(aio_cancel_nid_postfix(file, &first) == AioCanceled);
    Require(aio_error_nid_postfix(&first) == Ecanceled && aio_error_nid_postfix(&second) == Einprogress);
    Require(aio_suspend_nid_postfix(list, 1, &brief) == 0);
    Require(aio_return_nid_postfix(&first) == -1 && Error() == Ecanceled);
    Require(first.status == -1 && first.error == Ecanceled);
    Require(aio_return_nid_postfix(&first) == -1 && Error() == Einval);
    Require(aio_cancel_nid_postfix(file, &first) == AioAllDone);

    Require(aio_cancel_nid_postfix(file, nullptr) == AioCanceled);
    Require(aio_return_nid_postfix(&second) == -1 && Error() == Ecanceled);
    Require(aio_cancel_nid_postfix(file, nullptr) == AioAllDone);
    Require(aio_cancel_nid_postfix(-1, nullptr) == -1 && Error() == Ebadf);
    AioHoldWorkerForTest_nid_no_patch(false);
}

void SyncOrder(int file) {
    char text[] = "ordered";
    AioHoldWorkerForTest_nid_no_patch(true);
    auto write = Request(file, text, 7, 0);
    auto sync = Request(file, nullptr, 0);
    Require(aio_write_nid_postfix(&write) == 0 && aio_fsync_nid_postfix(OSync, &sync) == 0);
    Require(aio_error_nid_postfix(&sync) == Einprogress);
    AioHoldWorkerForTest_nid_no_patch(false);

    GuestAiocb* done = nullptr;
    Require(aio_waitcomplete_nid_postfix(&done, nullptr) == 7 && done == &write);
    Require(aio_waitcomplete_nid_postfix(&done, nullptr) == 0 && done == &sync && sync.status == 0);
    const KernelTimespec zero{0, 0};
    Require(aio_waitcomplete_nid_postfix(&done, &zero) == -1 && Error() == Eagain && done == nullptr);
    const KernelTimespec brief{0, 5000000};
    Require(aio_waitcomplete_nid_postfix(&done, &brief) == -1 && Error() == Eagain);
    const KernelTimespec badTime{0, -1};
    done = &sync;
    Require(aio_waitcomplete_nid_postfix(&done, &badTime) == -1 && Error() == Einval && done == nullptr);
    Require(aio_waitcomplete_nid_postfix(reinterpret_cast<GuestAiocb**>(std::uintptr_t{0x10}), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(reinterpret_cast<GuestAiocb**>(std::uintptr_t{0x10}), &zero) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(reinterpret_cast<GuestAiocb**>(std::uintptr_t{0x10}), &badTime) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(reinterpret_cast<GuestAiocb**>(std::uintptr_t{0x11}), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(reinterpret_cast<GuestAiocb**>(std::uintptr_t{0x10}), reinterpret_cast<const KernelTimespec*>(std::uintptr_t{0x10})) == -1 && Error() == Efault);
    GuardedMemory guarded;
    Require(aio_waitcomplete_nid_postfix(guarded.UnreadableResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.UnreadableResult(), &zero) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.UnreadableResult(), &badTime) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.CrossingResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.CrossingResult(), &zero) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.CrossingResult(), &badTime) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.UnalignedCrossingResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.UnalignedCrossingResult(), &zero) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.UnreadableResult(), guarded.UnreadableTimeout()) == -1 && Error() == Efault);
    guarded.ProtectReadOnly(true);
    Require(aio_waitcomplete_nid_postfix(guarded.WritableResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.WritableResult(), &zero) == -1 && Error() == Efault);
    guarded.ProtectReadOnly(false);
    done = &sync;
    Require(aio_waitcomplete_nid_postfix(&done, reinterpret_cast<const KernelTimespec*>(std::uintptr_t{0x10})) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, reinterpret_cast<const KernelTimespec*>(std::uintptr_t{0x11})) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, guarded.UnreadableTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, guarded.CrossingTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, guarded.UnalignedCrossingTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(nullptr, &zero) == -1 && Error() == Eagain);
    Require(aio_waitcomplete_nid_postfix(nullptr, &badTime) == -1 && Error() == Einval);
    Require(aio_waitcomplete_nid_postfix(nullptr, reinterpret_cast<const KernelTimespec*>(std::uintptr_t{0x10})) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(nullptr, guarded.UnreadableTimeout()) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(nullptr, guarded.CrossingTimeout()) == -1 && Error() == Efault);

    AioHoldWorkerForTest_nid_no_patch(true);
    auto extra = Request(file, text, 7, 0);
    Require(aio_write_nid_postfix(&extra) == 0);
    AioHoldWorkerForTest_nid_no_patch(false);
    while (aio_error_nid_postfix(&extra) == Einprogress) std::this_thread::yield();

    done = &sync;
    Require(aio_waitcomplete_nid_postfix(&done, guarded.UnreadableTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, guarded.CrossingTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(&done, guarded.UnalignedCrossingTimeout()) == -1 && Error() == Efault && done == &sync);
    Require(aio_waitcomplete_nid_postfix(guarded.UnreadableResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(guarded.CrossingResult(), nullptr) == -1 && Error() == Efault);
    Require(aio_waitcomplete_nid_postfix(&done, &badTime) == -1 && Error() == Einval && done == nullptr);
    Require(aio_waitcomplete_nid_postfix(nullptr, nullptr) == 7 && extra.status == 7 && extra.error == 0);
    Require(aio_waitcomplete_nid_postfix(&done, &zero) == -1 && Error() == Eagain && done == nullptr);
}

void TimedWaits(int file) {
    using Clock = std::chrono::steady_clock;
    char buffer[4] = {};
    AioHoldWorkerForTest_nid_no_patch(true);
    auto held = Request(file, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&held) == 0);
    const GuestAiocb* list[] = {&held};
    const KernelTimespec millisecond{0, 1000000};
    auto start = Clock::now();
    for (int round = 0; round < 20; ++round) Require(aio_suspend_nid_postfix(list, 1, &millisecond) == -1 && Error() == Eagain);
    auto elapsed = Clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(20) && elapsed < std::chrono::milliseconds(200));

    GuestAiocb* done = nullptr;
    start = Clock::now();
    for (int round = 0; round < 20; ++round) Require(aio_waitcomplete_nid_postfix(&done, &millisecond) == -1 && Error() == Eagain);
    elapsed = Clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(20) && elapsed < std::chrono::milliseconds(200));

    std::atomic<bool> stop{false};
    std::thread waker([&] {
        char other[4] = {};
        const auto end = Clock::now() + std::chrono::milliseconds(600);
        while (!stop.load() && Clock::now() < end) {
            auto request = Request(file, other, sizeof(other));
            Require(aio_read_nid_postfix(&request) == 0);
            Require(aio_cancel_nid_postfix(file, &request) == AioCanceled);
            Require(aio_return_nid_postfix(&request) == -1 && Error() == Ecanceled);
            std::this_thread::yield();
        }
    });
    const KernelTimespec brief{0, 30000000};
    start = Clock::now();
    Require(aio_suspend_nid_postfix(list, 1, &brief) == -1 && Error() == Eagain);
    elapsed = Clock::now() - start;
    stop = true;
    waker.join();
    Require(elapsed >= std::chrono::milliseconds(30) && elapsed < std::chrono::milliseconds(300));

    Require(aio_cancel_nid_postfix(file, &held) == AioCanceled);
    Require(aio_return_nid_postfix(&held) == -1 && Error() == Ecanceled);
    AioHoldWorkerForTest_nid_no_patch(false);
}

int KeventWait(int queue, KernelEvent* events, int count, std::int64_t nanoseconds) {
    const KernelTimespec timeout{nanoseconds / 1000000000, nanoseconds % 1000000000};
    return kevent_nid_postfix(queue, nullptr, 0, events, count, &timeout);
}

GuestAiocb KeventRequest(int descriptor, void* buffer, std::uint64_t bytes, int queue, std::int64_t value, std::uint16_t flags = 0) {
    auto request = Request(descriptor, buffer, bytes);
    request.sigevent.notify = SigevKevent;
    request.sigevent.signo = queue;
    request.sigevent.value = value;
    request.sigevent.extra[0] = flags;
    return request;
}

void KeventCompletion(int file) {
    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    char buffer[8] = {};
    KernelEvent events[2]{};

    auto request = KeventRequest(file, buffer, sizeof(buffer), queue, 0x1234);
    Require(aio_read_nid_postfix(&request) == 0);
    Require(KeventWait(queue, events, 2, 5000000000) == 1);
    Require(events[0].ident == reinterpret_cast<std::uintptr_t>(&request) && events[0].filter == EvfiltAio);
    Require(events[0].flags == EvEof && events[0].fflags == 0 && events[0].data == 0);
    Require(reinterpret_cast<std::uintptr_t>(events[0].udata) == 0x1234);
    Require(KeventWait(queue, events, 2, 0) == 1 && events[0].ident == reinterpret_cast<std::uintptr_t>(&request));
    Require(aio_return_nid_postfix(&request) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(KeventWait(queue, events, 2, 0) == 0);
    KernelEvent gone{reinterpret_cast<std::uintptr_t>(&request), EvfiltAio, EvDelete, 0, 0, nullptr};
    Require(kevent_nid_postfix(queue, &gone, 1, nullptr, 0, nullptr) == -1 && Error() == Enoent);

    auto oneshot = KeventRequest(file, buffer, sizeof(buffer), queue, 7, EvOneshot);
    Require(aio_read_nid_postfix(&oneshot) == 0);
    Require(KeventWait(queue, events, 2, 5000000000) == 1 && events[0].flags == (EvOneshot | EvEof));
    Require(KeventWait(queue, events, 2, 0) == 0);
    Require(aio_return_nid_postfix(&oneshot) == static_cast<std::int64_t>(sizeof(buffer)));

    auto cleared = KeventRequest(file, buffer, sizeof(buffer), queue, 8, EvClear);
    Require(aio_read_nid_postfix(&cleared) == 0);
    Require(KeventWait(queue, events, 2, 5000000000) == 1 && events[0].flags == (EvClear | EvEof));
    Require(KeventWait(queue, events, 2, 0) == 0);
    KernelEvent touched{reinterpret_cast<std::uintptr_t>(&cleared), EvfiltAio, EvAdd, 0, 0, reinterpret_cast<void*>(std::uintptr_t{12})};
    Require(kevent_nid_postfix(queue, &touched, 1, nullptr, 0, nullptr) == 0);
    Require(KeventWait(queue, events, 2, 0) == 1 && reinterpret_cast<std::uintptr_t>(events[0].udata) == 12 && events[0].data == 0);
    Require(KeventWait(queue, events, 2, 0) == 0);
    Require(aio_return_nid_postfix(&cleared) == static_cast<std::int64_t>(sizeof(buffer)));

    AioHoldWorkerForTest_nid_no_patch(true);
    auto deleted = KeventRequest(file, buffer, sizeof(buffer), queue, 9);
    Require(aio_read_nid_postfix(&deleted) == 0);
    KernelEvent existing{reinterpret_cast<std::uintptr_t>(&deleted), EvfiltAio, EvAdd, 0, 0, reinterpret_cast<void*>(std::uintptr_t{10})};
    Require(kevent_nid_postfix(queue, &existing, 1, nullptr, 0, nullptr) == 0);
    auto cancelled = KeventRequest(file, buffer, sizeof(buffer), queue, 11);
    Require(aio_read_nid_postfix(&cancelled) == 0);
    Require(aio_cancel_nid_postfix(file, &cancelled) == AioCanceled);
    Require(KeventWait(queue, events, 2, 0) == 1 && events[0].ident == reinterpret_cast<std::uintptr_t>(&cancelled) && events[0].data == Ecanceled);
    Require(aio_return_nid_postfix(&cancelled) == -1 && Error() == Ecanceled);
    auto clearedCancel = KeventRequest(file, buffer, sizeof(buffer), queue, 13, EvClear);
    Require(aio_read_nid_postfix(&clearedCancel) == 0);
    Require(aio_cancel_nid_postfix(file, &clearedCancel) == AioCanceled);
    Require(KeventWait(queue, events, 2, 0) == 1 && events[0].data == Ecanceled);
    Require(KeventWait(queue, events, 2, 0) == 0);
    KernelEvent retouched{reinterpret_cast<std::uintptr_t>(&clearedCancel), EvfiltAio, EvAdd, 0, 0, reinterpret_cast<void*>(std::uintptr_t{14})};
    Require(kevent_nid_postfix(queue, &retouched, 1, nullptr, 0, nullptr) == 0);
    Require(KeventWait(queue, events, 2, 0) == 1 && reinterpret_cast<std::uintptr_t>(events[0].udata) == 14 && events[0].data == Ecanceled);
    Require(aio_return_nid_postfix(&clearedCancel) == -1 && Error() == Ecanceled);
    KernelEvent remove{reinterpret_cast<std::uintptr_t>(&deleted), EvfiltAio, EvDelete, 0, 0, nullptr};
    Require(kevent_nid_postfix(queue, &remove, 1, nullptr, 0, nullptr) == 0);
    AioHoldWorkerForTest_nid_no_patch(false);
    Wait(deleted);
    Require(KeventWait(queue, events, 2, 0) == 0);
    Require(aio_return_nid_postfix(&deleted) == static_cast<std::int64_t>(sizeof(buffer)));

    KernelEvent forged{0x5000, EvfiltAio, EvAdd, 0, 0, nullptr};
    Require(kevent_nid_postfix(queue, &forged, 1, nullptr, 0, nullptr) == -1 && Error() == Eperm);

    auto notQueue = KeventRequest(file, buffer, sizeof(buffer), file, 1);
    Require(aio_read_nid_postfix(&notQueue) == -1 && Error() == Ebadf && notQueue.error == Ebadf);
    auto badFlags = KeventRequest(file, buffer, sizeof(buffer), queue, 1, EvAdd);
    Require(aio_read_nid_postfix(&badFlags) == -1 && Error() == Einval && badFlags.error == Einval);

    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    auto piped = KeventRequest(pipe[0], buffer, sizeof(buffer), queue, 1);
    Require(aio_read_nid_postfix(&piped) == -1 && Error() == Eopnotsupp);
    KernelEvent leftover{reinterpret_cast<std::uintptr_t>(&piped), EvfiltAio, EvDelete, 0, 0, nullptr};
    Require(kevent_nid_postfix(queue, &leftover, 1, nullptr, 0, nullptr) == -1 && Error() == Enoent);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
    Require(close_nid_postfix(queue) == 0);
}

GuestSignalSet SignalSet(int guest) {
    GuestSignalSet set{};
    set.bits[(guest - 1) / 32] = 1u << ((guest - 1) % 32);
    return set;
}

struct Received {
    int signo;
    int code;
    int pid;
    std::int64_t value;
};

bool SignalPending(int guest) {
    GuestSignalSet pending{};
    Require(sigpending_nid_postfix(&pending) == 0);
    return (pending.bits[(guest - 1) / 32] & SignalSet(guest).bits[(guest - 1) / 32]) != 0;
}

bool TakeSignal(int guest, std::int64_t nanoseconds, Received& received) {
    const auto set = SignalSet(guest);
    unsigned char info[80] = {};
    const KernelTimespec timeout{nanoseconds / 1000000000, nanoseconds % 1000000000};
    if (sigtimedwait_nid_postfix(&set, info, &timeout) != guest) return false;
    std::memcpy(&received.signo, info, sizeof(received.signo));
    std::memcpy(&received.code, info + 8, sizeof(received.code));
    std::memcpy(&received.pid, info + 12, sizeof(received.pid));
    std::memcpy(&received.value, info + 32, sizeof(received.value));
    return true;
}

void SignalCompletion(int file) {
    char buffer[8] = {};
    const auto blocked = SignalSet(Sigusr1);
    const auto blocked2 = SignalSet(Sigusr2);
    Require(pthread_sigmask_nid_postfix(SigBlock, &blocked, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(SigBlock, &blocked2, nullptr) == 0);

    auto request = Request(file, buffer, sizeof(buffer));
    request.sigevent = {SigevSignal, Sigusr1, 0x55, {}};
    Require(aio_read_nid_postfix(&request) == 0);
    Received received{};
    Require(TakeSignal(Sigusr1, 5000000000, received));
    Require(received.signo == Sigusr1 && received.code == SiAsyncio && received.pid == 0 && received.value == 0x55);
    Require(aio_error_nid_postfix(&request) == 0 && aio_return_nid_postfix(&request) == static_cast<std::int64_t>(sizeof(buffer)));

    auto first = Request(file, buffer, sizeof(buffer));
    first.sigevent = {SigevSignal, Sigusr1, 1, {}};
    auto second = Request(file, buffer, sizeof(buffer));
    second.sigevent = {SigevSignal, Sigusr1, 2, {}};
    Require(aio_read_nid_postfix(&first) == 0 && aio_read_nid_postfix(&second) == 0);
    Wait(first);
    Wait(second);
    Require(TakeSignal(Sigusr1, 5000000000, received) && received.value == 1);
    Require(TakeSignal(Sigusr1, 5000000000, received) && received.value == 2);
    Require(aio_return_nid_postfix(&first) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(aio_return_nid_postfix(&second) == static_cast<std::int64_t>(sizeof(buffer)));

    auto directed = Request(file, buffer, sizeof(buffer));
    directed.sigevent = {SigevThreadId, Sigusr2, 0x66, {scePthreadGetthreadid()}};
    Require(aio_read_nid_postfix(&directed) == 0);
    Require(TakeSignal(Sigusr2, 5000000000, received) && received.code == SiAsyncio && received.value == 0x66);
    Require(aio_return_nid_postfix(&directed) == static_cast<std::int64_t>(sizeof(buffer)));

    auto missing = Request(file, buffer, sizeof(buffer));
    missing.sigevent = {SigevThreadId, Sigusr2, 0x77, {0x7ffffff0}};
    Require(aio_read_nid_postfix(&missing) == 0);
    Wait(missing);
    Require(aio_return_nid_postfix(&missing) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(!TakeSignal(Sigusr2, 0, received) && Error() == Eagain);

    auto ignored = Request(file, buffer, sizeof(buffer));
    ignored.sigevent = {SigevSignal, Sigurg, 0x88, {}};
    Require(aio_read_nid_postfix(&ignored) == 0);
    Wait(ignored);
    Require(aio_return_nid_postfix(&ignored) == static_cast<std::int64_t>(sizeof(buffer)));

    AioHoldWorkerForTest_nid_no_patch(true);
    auto cancelled = Request(file, buffer, sizeof(buffer));
    cancelled.sigevent = {SigevSignal, Sigusr1, 0x99, {}};
    Require(aio_read_nid_postfix(&cancelled) == 0);
    Require(aio_cancel_nid_postfix(file, &cancelled) == AioCanceled);
    AioHoldWorkerForTest_nid_no_patch(false);
    Require(TakeSignal(Sigusr1, 0, received) && received.value == 0x99);
    Require(aio_return_nid_postfix(&cancelled) == -1 && Error() == Ecanceled);

    auto returned = Request(file, buffer, sizeof(buffer));
    returned.sigevent = {SigevSignal, Sigusr1, 0xaa, {}};
    Require(aio_read_nid_postfix(&returned) == 0);
    Wait(returned);
    Require(SignalPending(Sigusr1));
    Require(aio_return_nid_postfix(&returned) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(!SignalPending(Sigusr1));
    Require(!TakeSignal(Sigusr1, 0, received) && Error() == Eagain);

    auto waited = Request(file, buffer, sizeof(buffer));
    waited.sigevent = {SigevSignal, Sigusr1, 0xab, {}};
    Require(aio_read_nid_postfix(&waited) == 0);
    Wait(waited);
    Require(SignalPending(Sigusr1));
    GuestAiocb* completed = nullptr;
    Require(aio_waitcomplete_nid_postfix(&completed, nullptr) == static_cast<std::int64_t>(sizeof(buffer)) && completed == &waited);
    Require(!SignalPending(Sigusr1));

    auto sharedFirst = Request(file, buffer, sizeof(buffer));
    sharedFirst.sigevent = {SigevSignal, Sigusr1, 0xbb, {}};
    auto sharedSecond = Request(file, buffer, sizeof(buffer));
    sharedSecond.sigevent = {SigevSignal, Sigusr1, 0xbb, {}};
    Require(aio_read_nid_postfix(&sharedFirst) == 0 && aio_read_nid_postfix(&sharedSecond) == 0);
    Wait(sharedFirst);
    Wait(sharedSecond);
    Require(TakeSignal(Sigusr1, 0, received) && received.value == 0xbb);
    Require(aio_return_nid_postfix(&sharedFirst) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(TakeSignal(Sigusr1, 0, received) && received.value == 0xbb);
    Require(aio_return_nid_postfix(&sharedSecond) == static_cast<std::int64_t>(sizeof(buffer)));
    Require(!SignalPending(Sigusr1));

    Require(pthread_sigmask_nid_postfix(SigUnblock, &blocked, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(SigUnblock, &blocked2, nullptr) == 0);
}

void SocketRequests() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    char buffer[8] = {};

    auto sync = Request(pair[0], nullptr, 0);
    Require(aio_fsync_nid_postfix(OSync, &sync) == -1 && Error() == Einval && sync.error == Einval);
    auto negative = Request(pair[0], buffer, sizeof(buffer), -1);
    Require(aio_read_nid_postfix(&negative) == -1 && Error() == Einval);

    auto waiting = Request(pair[0], buffer, sizeof(buffer), 12345);
    Require(aio_read_nid_postfix(&waiting) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(aio_error_nid_postfix(&waiting) == Einprogress);
    Require(aio_cancel_nid_postfix(pair[0], &waiting) == AioCanceled);
    Require(aio_return_nid_postfix(&waiting) == -1 && Error() == Ecanceled);

    auto first = Request(pair[0], buffer, 3, 99);
    auto second = Request(pair[0], buffer + 3, 5);
    Require(aio_read_nid_postfix(&first) == 0 && aio_read_nid_postfix(&second) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(aio_error_nid_postfix(&first) == Einprogress && aio_error_nid_postfix(&second) == Einprogress);
    Require(_write_nid_postfix(pair[1], "abcdefgh", 8) == 8);
    Wait(first);
    Wait(second);
    Require(aio_return_nid_postfix(&first) == 3 && aio_return_nid_postfix(&second) == 5);
    Require(std::memcmp(buffer, "abcdefgh", 8) == 0);

    auto partial = Request(pair[0], buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&partial) == 0);
    Require(_write_nid_postfix(pair[1], "xy", 2) == 2);
    Wait(partial);
    Require(aio_return_nid_postfix(&partial) == 2 && std::memcmp(buffer, "xy", 2) == 0);

    const int queue = kqueue_nid_postfix();
    Require(queue >= 0);
    auto sent = KeventRequest(pair[0], const_cast<char*>("pqrs"), 4, queue, 0x42);
    Require(aio_write_nid_postfix(&sent) == 0);
    KernelEvent events[1]{};
    Require(KeventWait(queue, events, 1, 5000000000) == 1 && events[0].ident == reinterpret_cast<std::uintptr_t>(&sent) && events[0].data == 0);
    Require(aio_return_nid_postfix(&sent) == 4);
    char got[4] = {};
    Require(_read_nid_postfix(pair[1], got, sizeof(got)) == 4 && std::memcmp(got, "pqrs", 4) == 0);
    Require(close_nid_postfix(queue) == 0);

    auto eof = Request(pair[0], buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&eof) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    Wait(eof);
    Require(aio_return_nid_postfix(&eof) == 0);

    const GuestHandler previous = signal_nid_postfix(Sigpipe, reinterpret_cast<GuestHandler>(std::uintptr_t{1}));
    auto broken = Request(pair[0], buffer, sizeof(buffer));
    Require(aio_write_nid_postfix(&broken) == 0);
    Wait(broken);
    Require(aio_return_nid_postfix(&broken) == -1 && Error() == Epipe);
    signal_nid_postfix(Sigpipe, previous);

    Require(close_nid_postfix(pair[0]) == 0);
}

void SocketCancelDuringAttempt() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    char buffer[4] = {};
    AioHoldSocketForTest_nid_no_patch(true);
    auto raced = Request(pair[0], buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&raced) == 0);
    Require(_write_nid_postfix(pair[1], "abcd", 4) == 4);
    const auto patience = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (AioSocketsHeldForTest_nid_no_patch() == 0) {
        Require(std::chrono::steady_clock::now() < patience);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(aio_cancel_nid_postfix(pair[0], &raced) == AioNotCanceled);
    Require(aio_cancel_nid_postfix(pair[0], &raced) == AioNotCanceled);
    char drained[4] = {};
    Require(recv_nid_postfix(pair[0], drained, sizeof(drained), MsgDontWait) == 4);
    AioHoldSocketForTest_nid_no_patch(false);
    const GuestAiocb* list[] = {&raced};
    const KernelTimespec timeout{5, 0};
    Require(aio_suspend_nid_postfix(list, 1, &timeout) == 0);
    Require(aio_return_nid_postfix(&raced) == -1 && Error() == Ecanceled);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}

void SigpipeToSubmitter() {
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
    const auto pipeSet = SignalSet(Sigpipe);
    const auto usr2Set = SignalSet(Sigusr2);
    Require(pthread_sigmask_nid_postfix(SigBlock, &pipeSet, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(SigBlock, &usr2Set, nullptr) == 0);
    std::atomic<int> target{0};
    std::atomic<bool> finished{false};
    std::thread other([&] {
        Require(pthread_sigmask_nid_postfix(SigBlock, &usr2Set, nullptr) == 0);
        Require(pthread_sigmask_nid_postfix(SigUnblock, &pipeSet, nullptr) == 0);
        target.store(pthread_getthreadid_np_nid_postfix());
        while (!finished.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    while (target.load() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    char buffer[8] = {};
    auto broken = Request(pair[0], buffer, sizeof(buffer));
    broken.sigevent = {SigevThreadId, Sigusr2, 0x31, {target.load()}};
    Require(aio_write_nid_postfix(&broken) == 0);
    Wait(broken);
    Require(aio_return_nid_postfix(&broken) == -1 && Error() == Epipe);
    Received received{};
    Require(TakeSignal(Sigpipe, 0, received) && received.signo == Sigpipe);
    finished.store(true);
    other.join();
    Require(pthread_sigmask_nid_postfix(SigUnblock, &usr2Set, nullptr) == 0);
    Require(pthread_sigmask_nid_postfix(SigUnblock, &pipeSet, nullptr) == 0);
    Require(close_nid_postfix(pair[0]) == 0);
}

void SocketPartialWrites() {
    constexpr std::size_t Large = 1 << 20;
    std::vector<char> data(Large, 'z');
    std::vector<char> sink(Large);
    int pair[2] = {-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    auto blocking = Request(pair[0], data.data(), Large);
    Require(aio_write_nid_postfix(&blocking) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(aio_error_nid_postfix(&blocking) == Einprogress);
    Require(aio_cancel_nid_postfix(pair[0], &blocking) == AioNotCanceled);
    while (aio_error_nid_postfix(&blocking) == Einprogress) {
        if (recv_nid_postfix(pair[1], sink.data(), sink.size(), MsgDontWait) < 0) {
            Require(Error() == Eagain);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    const auto written = aio_return_nid_postfix(&blocking);
    Require(written > 0 && written <= static_cast<std::int64_t>(Large));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);

    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    Require(fcntl_nid_postfix(pair[0], FSetfl, ONonblock) == 0);
    auto nonblocking = Request(pair[0], data.data(), Large);
    Require(aio_write_nid_postfix(&nonblocking) == 0);
    Wait(nonblocking);
    const auto part = aio_return_nid_postfix(&nonblocking);
    Require(part > 0 && part < static_cast<std::int64_t>(Large));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}

}

int main() {
    const auto path = "anyps5-kernel-sys-aio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    BeforeUse();
    const int file = open_nid_postfix(path.c_str(), ORdwr | OCreat, 0600);
    const int readOnly = open_nid_postfix(path.c_str(), ORdonly, 0);
    Require(file >= 0 && readOnly >= 0);
    Validation(file, readOnly);
    ErrorOfUnreadable();
    ReadAndWrite(file);
    Cancel(file);
    SyncOrder(file);
    TimedWaits(file);
    KeventCompletion(file);
    SignalCompletion(file);
    SocketRequests();
    SocketCancelDuringAttempt();
    SigpipeToSubmitter();
    SocketPartialWrites();
    Require(close_nid_postfix(readOnly) == 0 && close_nid_postfix(file) == 0);
    Require(std::filesystem::remove(path));
    std::puts("guest_kernel_sys_aio passed");
    return 0;
}

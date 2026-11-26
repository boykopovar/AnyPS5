#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CreationMask.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Ksem.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Efault = 14;
constexpr int Ebusy = 16;
constexpr int Einval = 22;
constexpr int Enospc = 28;
constexpr int Espipe = 29;
constexpr int Eagain = 35;
constexpr int Eopnotsupp = 45;
constexpr int Etimedout = 60;
constexpr int Eoverflow = 84;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

KernelTimespec FromNow(std::chrono::milliseconds delay) {
    const auto when = std::chrono::system_clock::now() + delay;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(when.time_since_epoch()).count();
    return {nanoseconds / 1000000000, nanoseconds % 1000000000};
}

int Value(std::intptr_t id) {
    int value = -1;
    Require(ksem_getvalue_nid_postfix(id, &value) == 0);
    return value;
}

template <typename TFinished>
void WaitForRegisteredWaiters(std::intptr_t id, unsigned expected, TFinished finished) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        Require(!finished());
        if (KsemWaiterCountForTests_nid_no_patch(id) == expected) {
            Require(!finished());
            return;
        }
        Require(std::chrono::steady_clock::now() < limit);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Counting() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0x80000000u) == -1 && Error() == Einval);
    Require(ksem_init_nid_postfix(nullptr, 1) == -1 && Error() == Efault);
    Require(ksem_init_nid_postfix(&id, 2) == 0 && id >= 0);
    Require(Value(id) == 2);
    Require(ksem_trywait_nid_postfix(id) == 0 && ksem_wait_nid_postfix(id) == 0 && Value(id) == 0);
    Require(ksem_trywait_nid_postfix(id) == -1 && Error() == Eagain);
    Require(ksem_post_nid_postfix(id) == 0 && Value(id) == 1);
    Require(ksem_getvalue_nid_postfix(id, nullptr) == -1 && Error() == Efault);
    Require(ksem_getvalue_nid_postfix(id, reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);
    Require(Value(id) == 1);
    Require(ksem_trywait_nid_postfix(id) == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    Require(ksem_init_nid_postfix(&id, 0x7fffffffu) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Eoverflow);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Timed() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    const KernelTimespec invalid{0, 1000000000};
    Require(ksem_timedwait_nid_postfix(id, &invalid) == -1 && Error() == Einval);
    const auto deadline = FromNow(std::chrono::milliseconds(150));
    const auto start = std::chrono::steady_clock::now();
    Require(ksem_timedwait_nid_postfix(id, &deadline) == -1 && Error() == Etimedout);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(100));
    Require(ksem_post_nid_postfix(id) == 0);
    const auto later = FromNow(std::chrono::seconds(5));
    Require(ksem_timedwait_nid_postfix(id, &later) == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

bool SameTime(const KernelTimespec& left, const KernelTimespec& right) {
    return left.tv_sec == right.tv_sec && left.tv_nsec == right.tv_nsec;
}

bool Later(const KernelTimespec& left, const KernelTimespec& right) {
    return left.tv_sec > right.tv_sec || (left.tv_sec == right.tv_sec && left.tv_nsec > right.tv_nsec);
}

FileStat Status(int descriptor) {
    FileStat status;
    std::memset(&status, 0xff, sizeof(status));
    Require(_fstat_nid_postfix(descriptor, &status) == 0);
    FileStat kernel;
    std::memset(&kernel, 0xee, sizeof(kernel));
    Require(sceKernelFstat(descriptor, &kernel) == 0 && std::memcmp(&kernel, &status, sizeof(status)) == 0);
    return status;
}

void Metadata() {
    const auto start = std::chrono::system_clock::now();
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0 && id >= 0);
    const int d = static_cast<int>(id);
    const FileStat created = Status(d);
    Require(created.st_mode == (0100000 | (0770 & ~File::GetCreationMask_nid_no_patch())));
    Require(created.st_dev == 0 && created.st_ino == 0 && created.st_nlink == 0 && created.st_rdev == 0);
    Require(created.st_uid == static_cast<std::uint32_t>(getuid_nid_postfix()) && created.st_gid == static_cast<std::uint32_t>(getgid_nid_postfix()));
    Require(created.st_size == 0 && created.st_blocks == 0 && created.st_blksize == 0 && created.st_flags == 0 && created.st_gen == 0);
    const auto born = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::seconds(created.st_birthtim.tv_sec) + std::chrono::nanoseconds(created.st_birthtim.tv_nsec)));
    Require(born + std::chrono::microseconds(1) >= start && created.st_birthtim.tv_nsec % 1000 == 0);
    Require(SameTime(created.st_atim, created.st_birthtim) && SameTime(created.st_mtim, created.st_birthtim) && SameTime(created.st_ctim, created.st_birthtim));
    Require(fchmod_nid_postfix(d, 07604) == 0 && Status(d).st_mode == 0100604);
    Require(sceKernelFchmod(d, 0640) == 0);
    FileStat changed = Status(d);
    Require(changed.st_mode == 0100640 && SameTime(changed.st_ctim, created.st_ctim) && SameTime(changed.st_atim, created.st_atim));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    Require(ksem_post_nid_postfix(id) == 0);
    const FileStat posted = Status(d);
    Require(Later(posted.st_ctim, created.st_ctim) && SameTime(posted.st_atim, created.st_atim) && SameTime(posted.st_mtim, created.st_mtim));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    Require(ksem_trywait_nid_postfix(id) == 0 && ksem_trywait_nid_postfix(id) == 0);
    const FileStat waited = Status(d);
    Require(Later(waited.st_atim, posted.st_ctim) && SameTime(waited.st_ctim, posted.st_ctim) && SameTime(waited.st_mtim, created.st_mtim));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    Require(ksem_trywait_nid_postfix(id) == -1 && Error() == Eagain);
    const FileStat refused = Status(d);
    Require(Later(refused.st_atim, waited.st_atim) && SameTime(refused.st_ctim, posted.st_ctim));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    int value = -1;
    Require(ksem_getvalue_nid_postfix(id, &value) == 0 && value == 0);
    const FileStat read = Status(d);
    Require(Later(read.st_atim, refused.st_atim));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    Require(ksem_getvalue_nid_postfix(id, nullptr) == -1 && Error() == Efault);
    Require(Later(Status(d).st_atim, read.st_atim));
    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "ksem") == 0);
    for (const bool write : {false, true}) {
        bool threw = false;
        try {
            if (write) sceKernelAddWriteEvent(queue, d, 1, nullptr);
            else sceKernelAddReadEvent(queue, d, 1, nullptr);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
    }
    Require(sceKernelDeleteEqueue(queue) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Blocking() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    std::atomic<int> result{-2};
    std::thread waiter([&] { result = ksem_wait_nid_postfix(id); });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(result == -2);
    Require(ksem_destroy_nid_postfix(id) == -1 && Error() == Ebusy);
    Require(ksem_post_nid_postfix(id) == 0);
    waiter.join();
    Require(result == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void AbsoluteTimeouts() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    for (const auto seconds : {std::int64_t{0}, std::numeric_limits<std::int64_t>::min()}) {
        const KernelTimespec expired{seconds, 0};
        Require(ksem_timedwait_nid_postfix(id, &expired) == -1 && Error() == Etimedout);
        Require(ksem_post_nid_postfix(id) == 0);
        Require(ksem_timedwait_nid_postfix(id, &expired) == 0 && Value(id) == 0);
    }
    const KernelTimespec future{std::numeric_limits<std::int64_t>::max(), 999999999};
    std::atomic<bool> completed{false};
    int result = -2;
    std::thread waiter([&] {
        result = ksem_timedwait_nid_postfix(id, &future);
        completed.store(true, std::memory_order_release);
    });
    WaitForRegisteredWaiters(id, 1, [&] { return completed.load(std::memory_order_acquire); });
    Require(ksem_post_nid_postfix(id) == 0);
    waiter.join();
    Require(result == 0 && Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void TimedWakeup() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = FromNow(std::chrono::seconds(5));
    std::atomic<unsigned> completed{0};
    std::array<int, 2> results{};
    std::array<int, 2> errors{};
    std::array<std::thread, 2> waiters;
    for (std::size_t index = 0; index < waiters.size(); ++index) {
        waiters[index] = std::thread([&, index] {
            results[index] = ksem_timedwait_nid_postfix(id, &deadline);
            errors[index] = Error();
            completed.fetch_add(1, std::memory_order_release);
        });
    }
    WaitForRegisteredWaiters(id, 2, [&] { return completed.load(std::memory_order_acquire) != 0; });
    Require(ksem_post_nid_postfix(id) == 0);
    for (auto& waiter : waiters) waiter.join();
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(4950));
    Require((results[0] == 0 && results[1] == -1 && errors[1] == Etimedout) ||
            (results[1] == 0 && results[0] == -1 && errors[0] == Etimedout));
    Require(Value(id) == 0);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

void Identifiers() {
    Require(ksem_post_nid_postfix(-1) == -1 && Error() == Einval);
    const auto path = "anyps5-ksem-stale-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    { std::ofstream stream(path); stream << "x"; }
    const int plain = open_nid_postfix(path.c_str(), 0, 0);
    Require(plain >= 0);
    Require(ksem_post_nid_postfix(plain) == -1 && Error() == Einval);
    Require(close_nid_postfix(plain) == 0);
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0);
    Require(close_nid_postfix(static_cast<int>(id)) == 0);
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    const int reused = open_nid_postfix(path.c_str(), 0, 0);
    Require(reused >= 0);
    Require(ksem_post_nid_postfix(reused) == -1 && Error() == Einval);
    Require(ksem_getvalue_nid_postfix(reused, nullptr) == -1 && Error() == Einval);
    Require(close_nid_postfix(reused) == 0);
    Require(ksem_init_nid_postfix(&id, 1) == 0);
    const int other = open_nid_postfix(path.c_str(), 0, 0);
    Require(other >= 0 && other != id);
    Require(dup2_nid_postfix(other, static_cast<int>(id)) == static_cast<int>(id));
    Require(ksem_post_nid_postfix(id) == -1 && Error() == Einval);
    Require(ksem_trywait_nid_postfix(id) == -1 && Error() == Einval);
    Require(close_nid_postfix(static_cast<int>(id)) == 0 && close_nid_postfix(other) == 0);
    std::filesystem::remove(path);
}

void Duplicates() {
    std::intptr_t id = -1;
    std::intptr_t target = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0 && ksem_init_nid_postfix(&target, 5) == 0);
    const int copy = dup_nid_postfix(static_cast<int>(id));
    Require(copy >= 0 && copy != id);
    Require(ksem_post_nid_postfix(copy) == 0 && Value(id) == 2);
    Require(ksem_trywait_nid_postfix(id) == 0 && Value(copy) == 1);
    const int moved = fcntl_nid_postfix(static_cast<int>(id), 0, 0);
    Require(moved >= 0 && moved != id && moved != copy);
    Require(ksem_post_nid_postfix(moved) == 0 && Value(copy) == 2);
    Require(dup2_nid_postfix(static_cast<int>(id), static_cast<int>(target)) == static_cast<int>(target));
    Require(Value(target) == 2);
    Require(ksem_trywait_nid_postfix(target) == 0 && Value(id) == 1);
    Require(dup2_nid_postfix(static_cast<int>(id), static_cast<int>(id)) == static_cast<int>(id));
    Require(Value(id) == 1);
    Require(fchmod_nid_postfix(copy, 0600) == 0 && Status(static_cast<int>(id)).st_mode == 0100600);
    Require(ksem_destroy_nid_postfix(id) == 0);
    Require(fchmod_nid_postfix(moved, 0640) == 0);
    for (const int descriptor : {copy, moved, static_cast<int>(target)}) {
        Require(Status(descriptor).st_mode == 0100640);
        Require(ksem_post_nid_postfix(descriptor) == -1 && Error() == Einval);
        Require(ksem_trywait_nid_postfix(descriptor) == -1 && Error() == Einval);
        int value = -1;
        Require(ksem_getvalue_nid_postfix(descriptor, &value) == -1 && Error() == Einval && value == -1);
        Require(ksem_destroy_nid_postfix(descriptor) == -1 && Error() == Einval);
        Require(close_nid_postfix(descriptor) == 0);
    }
}

std::intptr_t g_destroyAfterFind = -1;

void DestroyAfterFind(std::intptr_t) {
    Require(ksem_destroy_nid_postfix(g_destroyAfterFind) == 0);
}

void DestroyedAfterLookup() {
    for (int operation = 0; operation < 5; ++operation) {
        std::intptr_t id = -1;
        Require(ksem_init_nid_postfix(&id, 0) == 0);
        const int copy = dup_nid_postfix(static_cast<int>(id));
        Require(copy >= 0);
        g_destroyAfterFind = id;
        KsemAfterFindForTests_nid_no_patch(DestroyAfterFind);
        int value = -1;
        KernelTimespec deadline = FromNow(std::chrono::milliseconds(50));
        int result = 0;
        switch (operation) {
        case 0: result = ksem_post_nid_postfix(copy); break;
        case 1: result = ksem_trywait_nid_postfix(copy); break;
        case 2: result = ksem_timedwait_nid_postfix(copy, &deadline); break;
        case 3: result = ksem_getvalue_nid_postfix(copy, &value); break;
        default: result = ksem_destroy_nid_postfix(copy); break;
        }
        Require(result == -1 && Error() == Einval && value == -1);
        Require(close_nid_postfix(copy) == 0);
    }
}

void DuplicatesCountOnce() {
    std::intptr_t ids[30] = {};
    for (auto& id : ids) Require(ksem_init_nid_postfix(&id, 0) == 0);
    Require(ksem_destroy_nid_postfix(ids[29]) == 0);
    const int copy = dup_nid_postfix(static_cast<int>(ids[0]));
    Require(copy >= 0);
    Require(ksem_init_nid_postfix(&ids[29], 0) == 0);
    std::intptr_t extra = -1;
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc);
    Require(ksem_destroy_nid_postfix(ids[0]) == 0);
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc);
    Require(close_nid_postfix(copy) == 0);
    Require(ksem_init_nid_postfix(&ids[0], 0) == 0);
    for (const auto id : ids) Require(ksem_destroy_nid_postfix(id) == 0);
}

void Limit() {
    std::intptr_t ids[30] = {};
    for (auto& id : ids) Require(ksem_init_nid_postfix(&id, 0) == 0);
    std::intptr_t extra = -1;
    Require(ksem_init_nid_postfix(&extra, 0) == -1 && Error() == Enospc && extra >= 0);
    Require(ksem_post_nid_postfix(extra) == -1 && Error() == Einval);
    Require(ksem_destroy_nid_postfix(ids[0]) == 0);
    Require(ksem_init_nid_postfix(&ids[0], 0) == 0);
    for (const auto id : ids) Require(ksem_destroy_nid_postfix(id) == 0);
}

}

class UnreadablePage {
public:
    UnreadablePage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
    }

    ~UnreadablePage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    UnreadablePage(const UnreadablePage&) = delete;
    UnreadablePage& operator=(const UnreadablePage&) = delete;

    void* Address() const { return memory; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

void BadPointers() {
    UnreadablePage page;
    void* const bad[] = {reinterpret_cast<void*>(0x10), page.Address()};
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 0) == 0 && id >= 0);
    for (void* pointer : bad) {
        Require(ksem_init_nid_postfix(static_cast<std::intptr_t*>(pointer), 1) == -1 && Error() == Efault);
        Require(ksem_timedwait_nid_postfix(id, static_cast<const KernelTimespec*>(pointer)) == -1 && Error() == Efault);
        Require(ksem_timedwait_nid_postfix(-1, static_cast<const KernelTimespec*>(pointer)) == -1 && Error() == Efault);
    }
    Require(ksem_destroy_nid_postfix(id) == 0);
    const auto openDescriptors = [] {
        std::intptr_t probe = -1;
        Require(ksem_init_nid_postfix(&probe, 0) == 0);
        const auto first = probe;
        Require(ksem_destroy_nid_postfix(probe) == 0);
        return first;
    };
    const auto before = openDescriptors();
    Require(ksem_init_nid_postfix(reinterpret_cast<std::intptr_t*>(0x10), 1) == -1 && Error() == Efault);
    Require(openDescriptors() == before);
    KernelTimespec valid = FromNow(std::chrono::milliseconds(1));
    Require(ksem_init_nid_postfix(&id, 1) == 0 && ksem_timedwait_nid_postfix(id, &valid) == 0 && ksem_destroy_nid_postfix(id) == 0);
}

void Transfers() {
    std::intptr_t id = -1;
    Require(ksem_init_nid_postfix(&id, 1) == 0 && id >= 0);
    const int d = static_cast<int>(id);
    char byte = 'k';
    KernelIovec one{&byte, 1};
    KernelIovec none{&byte, 0};
    Require(read_nid_postfix(d, &byte, 1) == -1 && Error() == Eopnotsupp);
    Require(read_nid_postfix(d, nullptr, 1) == -1 && Error() == Eopnotsupp);
    Require(write_nid_postfix(d, "x", 1) == -1 && Error() == Eopnotsupp);
    Require(write_nid_postfix(d, nullptr, 1) == -1 && Error() == Eopnotsupp);
    Require(read_nid_postfix(d, &byte, 0) == 0);
    Require(write_nid_postfix(d, "x", 0) == -1 && Error() == Eopnotsupp);
    Require(readv_nid_postfix(d, &one, 1) == -1 && Error() == Eopnotsupp);
    Require(writev_nid_postfix(d, &one, 1) == -1 && Error() == Eopnotsupp);
    Require(readv_nid_postfix(d, &none, 1) == 0);
    Require(writev_nid_postfix(d, &none, 1) == -1 && Error() == Eopnotsupp);
    Require(lseek_nid_postfix(d, 0, 0) == -1 && Error() == Espipe);
    Require(lseek_nid_postfix(d, 0, 7) == -1 && Error() == Espipe);
    Require(lseek_nid_postfix(d, -1, 0) == -1 && Error() == Espipe);
    Require(pread_nid_postfix(d, &byte, 1, 0) == -1 && Error() == Espipe);
    Require(pread_nid_postfix(d, nullptr, 1, -1) == -1 && Error() == Espipe);
    Require(pwrite_nid_disambig1_nid_postfix(d, &byte, 1, 0) == -1 && Error() == Espipe);
    Require(preadv_nid_postfix(d, &one, 1, 0) == -1 && Error() == Espipe);
    Require(pwritev_nid_postfix(d, &one, 1, 0) == -1 && Error() == Espipe);
    Require(pread_nid_postfix(d, &byte, 0, 0) == -1 && Error() == Espipe);
    Require(pwrite_nid_disambig1_nid_postfix(d, &byte, 0, 0) == -1 && Error() == Espipe);
    Require(preadv_nid_postfix(d, &one, 0, 0) == -1 && Error() == Espipe);
    Require(pwritev_nid_postfix(d, &none, 1, 0) == -1 && Error() == Espipe);
    Require(sceKernelRead(d, &byte, 1) == SceKernelError(Eopnotsupp));
    Require(sceKernelWrite(d, &byte, 1) == SceKernelError(Eopnotsupp));
    Require(sceKernelReadv(d, &one, 1) == SceKernelError(Eopnotsupp));
    Require(sceKernelWritev(d, &one, 1) == SceKernelError(Eopnotsupp));
    Require(sceKernelLseek(d, 0, 0) == SceKernelError(Espipe));
    Require(sceKernelLseek(d, 0, 7) == SceKernelError(Espipe));
    Require(sceKernelPread(d, &byte, 1, 0) == SceKernelError(Espipe));
    Require(sceKernelPwrite(d, &byte, 1, 0) == SceKernelError(Espipe));
    Require(sceKernelPreadv(d, &one, 1, 0) == SceKernelError(Espipe));
    Require(sceKernelPwritev(d, &one, 1, 0) == SceKernelError(Espipe));
    Require(byte == 'k' && Value(id) == 1);
    Require(ksem_destroy_nid_postfix(id) == 0);
}

int main() {
    BadPointers();
    Counting();
    Timed();
    AbsoluteTimeouts();
    TimedWakeup();
    Blocking();
    Identifiers();
    Metadata();
    Duplicates();
    DestroyedAfterLookup();
    DuplicatesCountOnce();
    Transfers();
    Limit();
    std::puts("ksem unnamed semaphores passed");
    return 0;
}

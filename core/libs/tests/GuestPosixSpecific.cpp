#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Einval = 22;
constexpr int Efault = 14;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void WaitFor(const std::atomic<bool>& flag, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(flag, message);
}

class UnwritablePage {
public:
    UnwritablePage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
        Require(memory != nullptr, "map a read-only page");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "page size");
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED, "map a read-only page");
#endif
    }

    ~UnwritablePage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, size);
#endif
    }

    UnwritablePage(const UnwritablePage&) = delete;
    UnwritablePage& operator=(const UnwritablePage&) = delete;

    PthreadKey* Address() const { return reinterpret_cast<PthreadKey*>(memory); }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

void CheckKeyCreateFaults() {
    PthreadKey probe = 0;
    Require(pthread_key_create_nid_postfix(&probe, nullptr) == 0 && probe != 0, "key create for the fault checks");
    Require(pthread_key_delete_nid_postfix(probe) == 0, "key delete for the fault checks");

    UnwritablePage page;
    PthreadKey* const bad[] = {reinterpret_cast<PthreadKey*>(0x10), page.Address()};
    for (PthreadKey* pointer : bad) {
        Require(pthread_key_create_nid_postfix(pointer, nullptr) == Efault, "key create into an unusable pointer returns EFAULT");
        PthreadKey after = 0;
        Require(pthread_key_create_nid_postfix(&after, nullptr) == 0 && after == probe, "a failed key create does not keep the key");
        Require(pthread_key_delete_nid_postfix(after) == 0, "key delete after a failed create");
    }
    Require(pthread_key_create_nid_postfix(nullptr, nullptr) == Einval, "key create into NULL returns EINVAL");
    Require(pthread_key_delete_nid_postfix(probe) == Einval, "key delete of a key that is already deleted returns EINVAL");
    Require(pthread_key_delete_nid_postfix(0) == Einval, "key delete of key 0 returns EINVAL");
    Require(pthread_key_delete_nid_postfix(0x7fffffff) == Einval, "key delete of a key past the table returns EINVAL");
}

}

int main() {
    CheckKeyCreateFaults();
    int first = 0;
    int second = 0;
    for (const PthreadKey invalid : {static_cast<PthreadKey>(0), static_cast<PthreadKey>(0xFFFFFFFFu)}) {
        Require(pthread_setspecific_nid_postfix(invalid, &first) == Einval, "setspecific of an out-of-range key returns EINVAL");
        Require(pthread_getspecific_nid_postfix(invalid) == nullptr, "getspecific of an out-of-range key returns NULL");
    }

    PthreadKey key = 0;
    Require(pthread_key_create_nid_postfix(&key, nullptr) == 0 && key != 0, "key create");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific of a key never set returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == 0, "setspecific returns 0");
    Require(pthread_getspecific_nid_postfix(key) == &first, "getspecific returns the stored value");
    Require(pthread_setspecific_nid_postfix(key, nullptr) == 0, "setspecific of NULL returns 0");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific after storing NULL returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == 0, "setspecific again returns 0");

    std::atomic<bool> stored{false};
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
    std::atomic<void*> seenBefore{&first};
    std::atomic<void*> seenAfter{nullptr};
    std::thread other([&] {
        seenBefore = pthread_getspecific_nid_postfix(key);
        Require(pthread_setspecific_nid_postfix(key, &second) == 0, "helper setspecific returns 0");
        seenAfter = pthread_getspecific_nid_postfix(key);
        stored = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!release && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        finished = true;
    });
    WaitFor(stored, "helper never stored its value");
    Require(seenBefore.load() == nullptr, "another thread starts with NULL for a key this thread has set");
    Require(seenAfter.load() == &second, "another thread reads back its own value");
    Require(pthread_getspecific_nid_postfix(key) == &first, "this thread still reads its own value");
    release = true;
    WaitFor(finished, "helper never finished");
    other.join();

    Require(pthread_key_delete_nid_postfix(key) == 0, "key delete returns 0");
    Require(pthread_getspecific_nid_postfix(key) == nullptr, "getspecific of a deleted key returns NULL");
    Require(pthread_setspecific_nid_postfix(key, &first) == Einval, "setspecific of a deleted key returns EINVAL");

    PthreadKey reused = 0;
    Require(pthread_key_create_nid_postfix(&reused, nullptr) == 0 && reused == key, "a new key reuses the freed number");
    Require(pthread_getspecific_nid_postfix(reused) == nullptr, "the value stored under the old key is gone");
    Require(pthread_key_delete_nid_postfix(reused) == 0, "second key delete returns 0");
    return 0;
}

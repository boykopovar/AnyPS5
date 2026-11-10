#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

static void CheckValueLimit() {
    constexpr unsigned int maximum = 0x7fffffffu;
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 7, nullptr) == SCE_OK);
    const PthreadSem original = sem;
    const unsigned int invalidValues[] = {maximum + 1u, std::numeric_limits<unsigned int>::max()};
    int value = -1;
    for (const unsigned int initial : invalidValues) {
        Require(scePthreadSemInit(&sem, 0, initial, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(sem == original);
        Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 7);
    }
    Require(scePthreadSemDestroy(&sem) == SCE_OK);

    Require(scePthreadSemInit(&sem, 0, maximum, nullptr) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum - 1));
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemPost(&sem) == SCE_KERNEL_ERROR_EOVERFLOW);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == static_cast<int>(maximum));
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

struct Context {
    PthreadSem sem = nullptr;
};

static void* APS5_VABI Poster(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    Require(scePthreadSemPost(&context.sem) == SCE_OK);
    return nullptr;
}

class GuardPage {
public:
    GuardPage() {
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

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const { return memory + page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckBadPointers() {
    GuardPage guard;
    void* const bad[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 3, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    for (void* pointer : bad) {
        Require(scePthreadSemInit(static_cast<PthreadSem*>(pointer), 0, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadSemGetvalue(static_cast<PthreadSem*>(pointer), &value) == SCE_KERNEL_ERROR_EFAULT && value == -1);
        Require(scePthreadSemGetvalue(&sem, static_cast<int*>(pointer)) == SCE_KERNEL_ERROR_EFAULT);
    }
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 3);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

int main() {
    CheckBadPointers();
    CheckValueLimit();

    PthreadSem sem = nullptr;
    Require(scePthreadSemInit(&sem, 0, 2, nullptr) == SCE_OK);
    int value = -1;
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 2);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_OK);
    Require(scePthreadSemTrywait(&sem) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 0);
    Require(scePthreadSemTimedwait(&sem, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadSemPost(&sem) == SCE_OK);
    Require(scePthreadSemGetvalue(&sem, &value) == SCE_OK && value == 1);

    Context context;
    Require(scePthreadSemInit(&context.sem, 0, 0, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Poster, &context, nullptr) == SCE_OK);
    Require(scePthreadSemTimedwait(&context.sem, 5000000) == SCE_OK);
    Require(scePthreadSemWait(&context.sem) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadSemDestroy(&context.sem) == SCE_OK);

    bool rejected = false;
    try { scePthreadSemWait(&context.sem); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    Require(scePthreadSemDestroy(&sem) == SCE_OK);
}

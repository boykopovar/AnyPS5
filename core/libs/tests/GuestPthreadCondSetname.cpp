#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int GuestEfault = 14;
static constexpr int GuestEinval = 22;

static void Require(bool value) { if (!value) std::abort(); }

template <class TCall>
static bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

class FaultPages {
public:
    FaultPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        memory = static_cast<char*>(mmap(nullptr, 2 * size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + size, size, PROT_NONE) == 0);
#endif
    }

    ~FaultPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * size);
#endif
    }

    FaultPages(const FaultPages&) = delete;
    FaultPages& operator=(const FaultPages&) = delete;

    void* ReadOnly() const { return memory; }
    void* Unreadable() const { return memory + size; }

private:
    char* memory = nullptr;
    std::size_t size = 0;
};

static void CheckBadPointers() {
    FaultPages pages;
    void* const badSlots[] = {reinterpret_cast<void*>(0x10), pages.ReadOnly(), pages.Unreadable()};
    for (void* pointer : badSlots) {
        auto* slot = static_cast<PthreadCond*>(pointer);
        Require(pthread_cond_init_nid_postfix(slot, nullptr) == GuestEfault);
        Require(scePthreadCondInit(slot, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(pthread_cond_setname_np_nid_postfix(slot, "bad") == GuestEfault);
        if (pointer != pages.ReadOnly()) {
            Require(pthread_cond_destroy_nid_postfix(slot) == GuestEfault);
            Require(scePthreadCondDestroy(slot) == SCE_KERNEL_ERROR_EFAULT);
        }
    }
    Require(pthread_cond_destroy_nid_postfix(static_cast<PthreadCond*>(pages.ReadOnly())) == 0);

    void* const badAttrs[] = {reinterpret_cast<void*>(0x10), pages.Unreadable()};
    for (void* pointer : badAttrs) {
        PthreadCond cond = nullptr;
        Require(pthread_cond_init_nid_postfix(&cond, static_cast<PthreadCondattr*>(pointer)) == GuestEfault);
        Require(scePthreadCondInit(&cond, static_cast<PthreadCondattr*>(pointer), nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(cond == nullptr);
    }

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0 && cond != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
}

int main() {
    CheckBadPointers();
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "first") == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "second") == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(ThrowsInvalidArgument([&] { return pthread_cond_setname_np_nid_postfix(&cond, nullptr); }));
    Require(pthread_cond_setname_np_nid_postfix(nullptr, "cond") == GuestEinval);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "destroyed") == GuestEinval);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "reinitialized") == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);

    PthreadCond lazy = nullptr;
    Require(pthread_cond_setname_np_nid_postfix(&lazy, "lazy") == 0 && lazy != nullptr);
    Require(pthread_cond_signal_nid_postfix(&lazy) == 0);
    Require(pthread_cond_destroy_nid_postfix(&lazy) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&lazy, "lazy") == GuestEinval);
}

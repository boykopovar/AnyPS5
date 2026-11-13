#include "prx/libSceNpPartner001/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
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

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::logic_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<bool*>(VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        *memory = true;
        DWORD previous;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0);
#else
        const long nativePageSize = sysconf(_SC_PAGESIZE);
        Require(nativePageSize > 0);
        pageSize = static_cast<std::size_t>(nativePageSize);
        void* allocation = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED);
        memory = static_cast<bool*>(allocation);
        *memory = true;
        Require(mprotect(memory, pageSize, PROT_READ) == 0);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize);
#endif
    }

    bool* Result() const { return memory; }

private:
    bool* memory = nullptr;
    std::size_t pageSize = 0;
};

}

int main() {
    bool subscribed = true;
    Require(Throws([] { sceNpEAAccessTerminate(); }));
    Require(Throws([&] { sceNpHasEAAccessSubscription(1, &subscribed); }));
    Require(sceNpEAAccessInitialize() == 0);
    Require(Throws([] { sceNpEAAccessInitialize(); }));
    Require(ThrowsMessage([] { sceNpHasEAAccessSubscription(1, nullptr); }, "null result"));
    Require(sceNpHasEAAccessSubscription(1, &subscribed) == 0);
    Require(!subscribed);
    subscribed = true;
    Require(sceNpHasEAAccessSubscription(0, &subscribed) == 0);
    Require(!subscribed);
    const ReadOnlyPage readOnly;
    Require(ThrowsMessage([&] { sceNpHasEAAccessSubscription(1, readOnly.Result()); }, "unwritable result"));
    Require(*readOnly.Result());
    Require(ThrowsMessage([] { sceNpHasEAAccessSubscription(1, reinterpret_cast<bool*>(0x10)); }, "unwritable result"));
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    errno = 71;
    subscribed = true;
    Require(sceNpHasEAAccessSubscription(1, &subscribed) == 0);
    Require(!subscribed && errno == 71);
    Require(ThrowsMessage([&] { sceNpHasEAAccessSubscription(1, readOnly.Result()); }, "unwritable result"));
    Require(*readOnly.Result() && errno == 71);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    Require(sceNpEAAccessTerminate() == 0);
    Require(ThrowsMessage([] { sceNpEAAccessTerminate(); }, "not initialized"));
    Require(ThrowsMessage([&] { sceNpHasEAAccessSubscription(1, &subscribed); }, "not initialized"));
    Require(ThrowsMessage([] { sceNpHasEAAccessSubscription(1, nullptr); }, "not initialized"));
    Require(ThrowsMessage([] { sceNpHasEAAccessSubscription(0, nullptr); }, "not initialized"));
    Require(sceNpEAAccessInitialize() == 0);
    Require(sceNpEAAccessTerminate() == 0);
    Require(ThrowsMessage([] { sceNpEAAccessTerminate(); }, "not initialized"));
}

#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/DescriptorAccess.hpp"
#include "nid/Sha1.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s, guest errno %d\n", message, *__error_nid_postfix());
    std::exit(1);
}
template<class TPredicate>
static bool Observe(TPredicate predicate, int milliseconds = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

class NativeLock {
public:
    explicit NativeLock(int descriptor) {
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION info{};
        Require(GetFileInformationByHandle(reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor)), &info), "identify shared object");
        const auto identity = L"object-" + std::to_wstring(info.dwVolumeSerialNumber) + L"-" +
            std::to_wstring(info.nFileIndexHigh) + L"-" + std::to_wstring(info.nFileIndexLow);
        const auto* begin = reinterpret_cast<const std::uint8_t*>(identity.data());
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(begin, begin + identity.size() * sizeof(wchar_t)));
        std::wstring name = L"Global\\AnyPS5NamedShm-";
        for (const auto byte : digest) { name += L"0123456789abcdef"[byte >> 4]; name += L"0123456789abcdef"[byte & 15]; }
        handle = CreateMutexW(nullptr, FALSE, name.c_str());
        Require(handle && WaitForSingleObject(handle, 2000) == WAIT_OBJECT_0, "hold shared object lock");
#else
        static_cast<void>(descriptor);
        handle = ::open("/dev/shm", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        Require(handle >= 0 && ::flock(handle, LOCK_EX) == 0, "hold shared namespace lock");
#endif
    }
    ~NativeLock() { Release(); }
    void Release() {
#ifdef _WIN32
        if (!handle) return;
        Require(ReleaseMutex(handle), "release shared object lock");
        CloseHandle(handle); handle = nullptr;
#else
        if (handle < 0) return;
        Require(::flock(handle, LOCK_UN) == 0, "release shared namespace lock");
        ::close(handle); handle = -1;
#endif
    }
private:
#ifdef _WIN32
    HANDLE handle = nullptr;
#else
    int handle = -1;
#endif
};

struct State {
    int descriptor;
    std::string api;
    bool disabled;
    PthreadKey key = -1;
    std::atomic<bool> ready{false}, returned{false};
    std::atomic<int> cleanups{0}, destructors{0};
    std::int64_t result = -2;
    int error = 0;
};
static void APS5_VABI Cleanup(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.fetch_add(1) == 0 && state.destructors.load() == 0, "shared write cleanup order");
}
static void APS5_VABI Destroy(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    Require(state.cleanups.load() == 1 && state.destructors.fetch_add(1) == 0, "shared write TSD order");
}
static void* APS5_VABI Writer(void* opaque) {
    auto& state = *static_cast<State*>(opaque);
    if (state.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable shared writer cancellation");
    Require(scePthreadSetspecific(state.key, &state) == 0, "set shared write TSD");
    CleanupHandler cleanup{};
    __pthread_cleanup_push_imp_nid_postfix(Cleanup, &state, &cleanup);
    KernelIovec vectors[2]{{const_cast<char*>("w"), 1}, {const_cast<char*>("w"), 1}};
    state.ready.store(true);
    if (state.api == "write") state.result = write_nid_postfix(state.descriptor, "ww", 2);
    else if (state.api == "alias") state.result = _write_nid_postfix(state.descriptor, "ww", 2);
    else if (state.api == "pwrite") state.result = pwrite_nid_disambig1_nid_postfix(state.descriptor, "ww", 2, 0);
    else if (state.api == "writev") state.result = writev_nid_postfix(state.descriptor, vectors, 2);
    else if (state.api == "writev_alias") state.result = _writev_nid_postfix(state.descriptor, vectors, 2);
    else if (state.api == "pwritev") state.result = pwritev_nid_postfix(state.descriptor, vectors, 2, 0);
    else Require(false, "unknown shared write API");
    state.error = *__error_nid_postfix();
    state.returned.store(true);
    if (state.disabled) {
        Require(scePthreadSetcancelstate(GUEST_CANCEL_ENABLE, nullptr) == 0, "enable shared writer cancellation");
        scePthreadTestcancel();
    }
    __pthread_cleanup_pop_imp_nid_postfix(1);
    return reinterpret_cast<void*>(std::uintptr_t{7});
}

int main(int argc, char** argv) {
    Require(argc == 3, "expected shared write API and scenario");
    const std::string scenario = argv[2];
    const bool close = scenario == "close" || scenario == "disabled_close";
    const bool disabled = scenario.starts_with("disabled");
    const bool cancel = scenario == "cancel" || disabled;
#ifdef _WIN32
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    const auto name = "/anyps5-write-cancel-" + std::to_string(process) + "-" + argv[1] + "-" + scenario;
    const int descriptor = shm_open_nid_postfix(name.c_str(), 2 | 0x200 | 0x800, 0600);
    Require(descriptor >= 0 && ftruncate_nid_postfix(descriptor, 0x4000) == 0, "create shared write object");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "safe", 4, 0) == 4, "seed shared write object");
    const int alias = dup_nid_postfix(descriptor);
    Require(alias >= 0, "retain shared write alias");
    NativeLock held(descriptor);
    State state{descriptor, argv[1], disabled};
    Require(scePthreadKeyCreate(&state.key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "create shared write TSD key");
    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, Writer, &state, nullptr) == 0, "create shared writer");
    Require(Observe([&] { return state.ready.load(); }), "shared writer did not start");
    Require(Observe([&] {
        GuestDescriptorAccess probe(descriptor, GuestDescriptorAccess::Mode::Update, std::defer_lock);
        return !probe.TryLock();
    }), "shared writer did not acquire descriptor admission");
    if (cancel) Require(pthread_cancel_nid_postfix(writer) == 0, "request shared write cancellation");
    std::atomic<bool> closed{false};
    std::thread closer;
    if (close) closer = std::thread([&] { Require(close_nid_postfix(descriptor) == 0, "close contended shared writer"); closed.store(true); });
    bool prompt = true;
    if (scenario == "cancel" || close) prompt = Observe([&] { return writer->_finished.load() && (!close || closed.load()); }, 500);
    else Require(!state.returned.load(), "shared write escaped held metadata lock");
    held.Release();
    Require(Observe([&] { return writer->_finished.load(); }), "shared writer did not finish after release");
    void* result = nullptr;
    Require(scePthreadJoin(writer, &result) == 0, "join shared writer");
    if (closer.joinable()) closer.join();
    Require(prompt, "cancellation/close required releasing shared metadata lock");
    Require(state.cleanups.load() == 1 && state.destructors.load() == 1, "shared write cleanup or TSD repeated");
    if (scenario == "cancel") Require(result == GUEST_CANCELED && !state.returned.load(), "contended shared write escaped cancellation");
    else if (close) Require(result == (disabled ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})) && state.returned.load() && state.result == -1 && state.error == 4, "shared close did not interrupt safely");
    else Require(result == (disabled ? GUEST_CANCELED : reinterpret_cast<void*>(std::uintptr_t{7})) && state.returned.load() && state.result == 2, "shared completion lost bytes or pending cancellation");
    std::array<char, 4> bytes{};
    Require(pread_nid_postfix(alias, bytes.data(), bytes.size(), 0) == 4 && std::memcmp(bytes.data(), scenario == "cancel" || close ? "safe" : "wwfe", 4) == 0, "shared write touched wrong bytes");
    Require(lseek_nid_postfix(alias, 0, 1) == (scenario == "cancel" || close || state.api.starts_with("pwrite") ? 0 : 2), "shared write offset changed unexpectedly");
    Require(close_nid_postfix(alias) == 0 && (close || close_nid_postfix(descriptor) == 0), "close shared write handles");
    Require(shm_unlink_nid_postfix(name.c_str()) == 0 && scePthreadKeyDelete(state.key) == 0, "remove shared write resources");
}

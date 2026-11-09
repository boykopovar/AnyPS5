#include "prx/libc/include/LegacySignals.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <link.h>
#endif

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "sigvec modules: %s\n", message);
    std::abort();
}

class Module {
public:
    explicit Module(const std::filesystem::path& path) {
#ifdef _WIN32
        handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (handle == nullptr) std::fprintf(stderr, "LoadLibraryExW(%ls) failed: %lu\n", path.c_str(), GetLastError());
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) std::fprintf(stderr, "%s\n", dlerror());
#endif
        Require(handle != nullptr, "load actual patched module");
#ifdef _WIN32
        HMODULE pinned = nullptr;
        Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(handle), &pinned) != 0 && pinned == handle, "pin actual module until process exit");
#endif
    }

    ~Module() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    void* Find(const char* name) const {
#ifdef _WIN32
        auto* result = reinterpret_cast<void*>(GetProcAddress(handle, name));
#else
        auto* result = dlsym(handle, name);
#endif
        return result;
    }

    void* Symbol(const char* name) const {
        auto* result = Find(name);
        Require(result != nullptr, "lookup actual native export");
        return result;
    }

    void RequireOwner(void* address) const {
#ifdef _WIN32
        HMODULE owner = nullptr;
        Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address), &owner) != 0 && owner == handle, "NID resolved outside requested DLL");
#else
        link_map* image = nullptr;
        Dl_info info{};
        Require(dlinfo(handle, RTLD_DI_LINKMAP, &image) == 0 && image != nullptr, "read requested module identity");
        Require(dladdr(address, &info) != 0 && info.dli_fbase == reinterpret_cast<void*>(image->l_addr), "NID resolved through an ELF dependency");
#endif
    }

    template<typename T>
    T Function(const char* name) const {
        return reinterpret_cast<T>(Symbol(Nid::ComputeNid(name, "").c_str()));
    }

private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};

using Sigvec = decltype(&sigvec_nid_postfix);
GuestSigaction recorded{};
GuestSigaction answer{0x12345678, 0x13, {{0x87654321, 0x11111111, 0x22222222, 0x33333333}}};
int calls = 0;
int result = 0;
bool sawReplacement = false;
bool sawPrevious = false;
int requestedSignal = 0;

int APS5_VABI RecordAction(int signal, const GuestSigaction* replacement, GuestSigaction* previous) {
    ++calls;
    requestedSignal = signal;
    sawReplacement = replacement != nullptr;
    sawPrevious = previous != nullptr;
    if (replacement != nullptr) recorded = *replacement;
    if (result == 0 && previous != nullptr) *previous = answer;
    return result;
}

bool Equal(const GuestSignalVector& first, const GuestSignalVector& second) {
    return first.handler == second.handler && first.mask == second.mask && first.flags == second.flags;
}

void CheckSharedDisposition(const std::array<Sigvec, 2>& exports, const Module& libc) {
    const auto error = libc.Function<decltype(&__error_nid_postfix)>("__error");
    GuestSignalVector original{};
    Require(exports[0](30, nullptr, &original) == 0, "save actual kernel disposition");
    const GuestSignalVector first{1, std::bit_cast<std::int32_t>(0x80000005u), 3};
    const GuestSignalVector second{1, -123, 2};
    GuestSignalVector queried{};
    Require(exports[1](30, &first, nullptr) == 0 && exports[0](30, nullptr, &queried) == 0 && Equal(queried, first), "Internal install is not shared with libc");
    Require(exports[0](30, &second, nullptr) == 0 && exports[1](30, nullptr, &queried) == 0 && Equal(queried, second), "libc install is not shared with Internal");
    for (const auto function : exports) {
        queried = first;
        Require(function(0, &second, &queried) == -1 && *error() == 22 && Equal(queried, first), "actual backend failure changed module output");
    }
    Require(exports[0](30, &original, nullptr) == 0, "restore actual kernel disposition");
}

void CheckConversions(const std::array<Sigvec, 2>& exports) {
    const GuestSignalVector expected{answer.handler, std::bit_cast<std::int32_t>(answer.mask.bits[0]), answer.flags ^ 2};
    for (const auto function : exports) {
        for (const int flags : {0, 1, 2, 3, -1}) {
            const GuestSignalVector replacement{0x34567890, -1, flags};
            GuestSignalVector previous{};
            result = 0;
            const int before = calls;
            Require(function(30, &replacement, &previous) == 0 && calls == before + 1, "module wrapper did not call shared core once");
            Require(requestedSignal == 30 && sawReplacement && sawPrevious, "forwarded argument identity");
            Require(recorded.handler == replacement.handler && recorded.flags == (flags ^ 2), "firmware handler/flag conversion");
            Require(recorded.mask.bits[0] == 0xffffffffu && recorded.mask.bits[1] == 0 && recorded.mask.bits[2] == 0 && recorded.mask.bits[3] == 0, "firmware low mask conversion");
            Require(Equal(previous, expected), "firmware previous vector conversion");
            GuestSignalVector alias = replacement;
            Require(function(30, &alias, &alias) == 0 && Equal(alias, expected), "module-owned alias conversion");
            Require(recorded.handler == replacement.handler && recorded.flags == (flags ^ 2) && recorded.mask.bits[0] == 0xffffffffu, "alias input changed before forwarding");
        }
        result = 0;
        Require(function(30, nullptr, nullptr) == 0 && !sawReplacement && !sawPrevious, "both null pointers were not forwarded");
        GuestSignalVector previous{};
        Require(function(30, nullptr, &previous) == 0 && !sawReplacement && sawPrevious && Equal(previous, expected), "null replacement query");
        const GuestSignalVector replacement{0x34567890, 7, 0};
        Require(function(30, &replacement, nullptr) == 0 && sawReplacement && !sawPrevious, "null previous output");
        for (const int failure : {-7, -1, 1}) {
            result = failure;
            previous = replacement;
            Require(function(17, &replacement, &previous) == failure && Equal(previous, replacement), "forward failure changed output or result");
        }
    }
}

void CheckGuestOutput(const std::array<Sigvec, 2>& exports, const Module& kernel, const Module& libc) {
    const auto allocate = kernel.Function<decltype(&sceKernelAllocateDirectMemory)>("sceKernelAllocateDirectMemory");
    const auto map = kernel.Function<decltype(&sceKernelMapDirectMemory)>("sceKernelMapDirectMemory");
    const auto unmap = kernel.Function<decltype(&sceKernelMunmap)>("sceKernelMunmap");
    const auto release = kernel.Function<decltype(&sceKernelReleaseDirectMemory)>("sceKernelReleaseDirectMemory");
    constexpr std::size_t page = 0x4000;
    std::int64_t physical = 0;
    Require(allocate(0, 0x7fffffffffll, page * 2, 0, 0, &physical) == 0, "allocate real guest module output");
    void* mapping = nullptr;
    Require(map(&mapping, page * 2, 3, 0, physical, 0) == 0 && mapping != nullptr, "map real guest module output");
    auto* output = reinterpret_cast<GuestSignalVector*>(static_cast<unsigned char*>(mapping) + page - 8);
    const GuestSignalVector expected{answer.handler, std::bit_cast<std::int32_t>(answer.mask.bits[0]), answer.flags ^ 2};
    result = 0;
    for (const auto function : exports) Require(function(30, nullptr, output) == 0 && Equal(*output, expected), "owned NID guest-backed output");
#ifdef _WIN32
    const auto protect = kernel.Function<decltype(&sceKernelMprotect)>("sceKernelMprotect");
    const auto error = libc.Function<decltype(&__error_nid_postfix)>("__error");
    const GuestSignalVector sentinel{0x23456789, 5, 2};
    *output = sentinel;
    Require(protect(static_cast<unsigned char*>(mapping) + page, page, 1) == 0, "protect module output tail");
    for (const auto function : exports) {
        const int before = calls;
        Require(function(30, &sentinel, output) == -1 && *error() == 14 && calls == before, "rejected module output mutated backend");
        Require(Equal(*output, sentinel), "rejected module output partially written");
    }
    Require(protect(static_cast<unsigned char*>(mapping) + page, page, 3) == 0, "restore module output protection");
#else
    static_cast<void>(libc);
#endif
    Require(unmap(reinterpret_cast<std::uint64_t>(mapping), page * 2) == 0, "unmap module output");
    Require(release(physical, page * 2) == 0, "release module output");
}

}

int main(int argc, char** argv) {
    Require(argc == 3 || (argc == 4 && std::string(argv[3]) == "libc-first"), "expected patched module directory, exception probe and optional load order");
    const auto directory = std::filesystem::absolute(argv[1]);
    const Module first(directory / (argc == 4 ? "libc.prx" : "libSceLibcInternal.prx"));
    const Module libc(directory / "libc.prx");
    const Module kernel(directory / "libkernel.prx");
    const Module internal(directory / "libSceLibcInternal.prx");
    auto* libcAddress = libc.Symbol("yhxKO9LYc8A");
    auto* internalAddress = internal.Symbol("yhxKO9LYc8A");
    libc.RequireOwner(libcAddress);
    internal.RequireOwner(internalAddress);
    Require(libcAddress != internalAddress, "both modules resolve to the same wrapper");
    std::array<Sigvec, 2> exports{reinterpret_cast<Sigvec>(libcAddress), reinterpret_cast<Sigvec>(internalAddress)};
    for (const auto function : exports) Require(function(30, nullptr, nullptr) == 0, "cold kernel registration missing");
    CheckSharedDisposition(exports, libc);
    auto* registrationAddress = libc.Symbol("RegisterSigvec_nid_no_patch");
    libc.RequireOwner(registrationAddress);
    auto* coreAddress = libc.Symbol("LegacySigvec_nid_no_patch");
    libc.RequireOwner(coreAddress);
#ifdef _WIN32
    Require(internal.Find("LegacySigvec_nid_no_patch") == nullptr, "Internal duplicated the private core");
#else
    Require(internal.Find("LegacySigvec_nid_no_patch") == coreAddress, "Internal does not use libc private core");
#endif
    const auto registration = reinterpret_cast<decltype(&RegisterSigvec_nid_no_patch)>(registrationAddress);
    const Module exceptionProbe(directory / argv[2]);
    auto* exceptionAddress = exceptionProbe.Symbol("SigvecMissingBackend_nid_no_patch");
    exceptionProbe.RequireOwner(exceptionAddress);
    const auto checkMissingBackend = reinterpret_cast<bool (*)(Sigvec, Sigvec, const void*)>(exceptionAddress);
    registration(nullptr);
    Require(checkMissingBackend(exports[0], exports[1], coreAddress), "module wrapper has duplicate or stale callback state");
    registration(RecordAction);
    CheckConversions(exports);
    CheckGuestOutput(exports, kernel, libc);
    registration(kernel.Function<LegacySigaction_nid_no_patch>("sigaction"));
    for (const auto function : exports) Require(function(30, nullptr, nullptr) == 0, "actual kernel registration restore");
    std::printf("sigvec NID ownership, shared callback/core, firmware conversions and guest output passed (%s first)\n", argc == 4 ? "libc" : "Internal");
}

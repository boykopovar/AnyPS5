#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <nid/NidCompute.hpp>
#include "SceTypes.hpp"
#include "prx/libkernel/File/include/KernelStatfs.hpp"
#include <span>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <source_location>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel sys API check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* LoadLibraryFile(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
#ifdef _WIN32
    std::wstring loaded(32768, L'\0');
    const auto length = GetModuleFileNameW(static_cast<HMODULE>(module), loaded.data(), static_cast<DWORD>(loaded.size()));
    Require(length != 0 && length < loaded.size());
    loaded.resize(length);
    Require(std::filesystem::equivalent(path, std::filesystem::path(loaded)));
#endif
    return module;
}

template <typename Function>
static Function LoadFunction(void* module, const char* name, bool patched, const char* unpatched = nullptr) {
    const auto symbol = patched ? Nid::ComputeNid(name, "") : unpatched ? std::string(unpatched) : std::string(name) + "_nid_postfix";
#ifdef _WIN32
    const auto function = reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), symbol.c_str())));
#else
    const auto function = reinterpret_cast<Function>(dlsym(module, symbol.c_str()));
#endif
    if (!function) std::fprintf(stderr, "Missing symbol %s\n", symbol.c_str());
    Require(function != nullptr);
    return function;
}


int main(int argc, char** argv) {
    Require(argc == 4);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    const std::string test = argv[3];
    auto* sys = LoadLibraryFile(directory / "libkernel_sys.prx");
    auto* kernel = LoadLibraryFile(directory / "libkernel.prx");
    auto* libc = LoadLibraryFile(directory / "libc.prx");
    const auto error = LoadFunction<int* (APS5_VABI*)()>(libc, "__error", patched);
    const auto open = LoadFunction<int (APS5_VABI*)(const char*, int, int)>(kernel, "open", patched);
    const auto close = LoadFunction<int (APS5_VABI*)(int)>(kernel, "close", patched);
    const auto stat = LoadFunction<std::int64_t (APS5_VABI*)(const char*, FileStat*)>(kernel, "stat", patched);
    const auto root = std::filesystem::path("anyps5-kernel-sys-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "data").string();
    { std::ofstream stream(path); stream << "data"; Require(stream.good()); }
    const int base = open(root.string().c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    const int file = open(path.c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(base >= 0 && file >= 0);
    bool skipped = false;
    if (test == "statfs") {
        const auto query = LoadFunction<int (APS5_VABI*)(const char*, KernelStatfs*)>(sys, "statfs", patched);
        const auto fquery = LoadFunction<int (APS5_VABI*)(int, KernelStatfs*)>(kernel, "_fstatfs", patched);
        struct Output { std::uint64_t before; KernelStatfs value; std::uint64_t after; } out{};
        out.before = 0xdeadbeef;
        out.after = 0xbeefdead;
        KernelStatfs byDescriptor{};
        *error() = 71;
        Require(query(path.c_str(), &out.value) == 0 && *error() == 71);
        Require(out.before == 0xdeadbeef && out.after == 0xbeefdead);
        Require(fquery(file, &byDescriptor) == 0);
        Require(out.value.f_version == 0x20030518 && out.value.f_bsize > 0 && out.value.f_namemax > 0);
        Require(out.value.f_bsize == byDescriptor.f_bsize && out.value.f_iosize == byDescriptor.f_iosize);
        Require(out.value.f_flags == byDescriptor.f_flags && out.value.f_blocks == byDescriptor.f_blocks);
        Require(query(root.string().c_str(), &out.value) == 0);
        std::memset(&out.value, 0xa5, sizeof(out.value));
        Require(query((root / "missing").string().c_str(), &out.value) == -1 && *error() == 2);
        for (const auto byte : std::as_bytes(std::span(&out.value, 1))) Require(byte == std::byte{0xa5});
        Require(query(nullptr, &out.value) == -1 && *error() == 14);
        Require(query("", &out.value) == -1 && *error() == 2);
        Require(query((root / "data" / "child").string().c_str(), &out.value) == -1 && *error() == 20);
        Require(query(path.c_str(), nullptr) == -1 && *error() == 14);
        Require(query(path.c_str(), reinterpret_cast<KernelStatfs*>(1)) == -1 && *error() == 14);
#ifdef _WIN32
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const auto page = static_cast<std::size_t>(system.dwPageSize);
        auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != FALSE);
#else
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto* memory = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory, page, PROT_READ) == 0);
#endif
        Require(query(path.c_str(), reinterpret_cast<KernelStatfs*>(memory)) == -1 && *error() == 14);
#ifdef _WIN32
        Require(VirtualProtect(memory, page, PAGE_READWRITE, &previous) != FALSE);
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != FALSE);
#else
        Require(mprotect(memory, page, PROT_READ | PROT_WRITE) == 0);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
        Require(query(path.c_str(), reinterpret_cast<KernelStatfs*>(memory + page - sizeof(KernelStatfs) / 2)) == -1 && *error() == 14);
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != FALSE);
#else
        Require(munmap(memory, page * 2) == 0);
#endif

    } else if (test == "futimesat") {
        const auto update = LoadFunction<int (APS5_VABI*)(int, const char*, const KernelTimeval*)>(sys, "futimesat", patched);
        const KernelTimeval times[2]{{1000000000, 123456}, {1000000001, 654321}};
        *error() = 71;
        Require(update(base, "data", times) == 0 && *error() == 71);
        FileStat info{};
        Require(stat(path.c_str(), &info) == 0);
        Require(info.st_atim.tv_sec == times[0].tv_sec && info.st_mtim.tv_sec == times[1].tv_sec);
        Require(info.st_atim.tv_nsec == times[0].tv_usec * 1000 && info.st_mtim.tv_nsec == times[1].tv_usec * 1000);
        Require(update(-100, path.c_str(), times) == 0);
        Require(update(-1, ("/" + path).c_str(), times) == 0);
        Require(update(-1, "data", times) == -1 && *error() == 9);
        Require(update(file, "data", times) == -1 && *error() == 20);
        Require(update(base, nullptr, times) == -1 && *error() == 14);
        Require(update(base, "", times) == -1 && *error() == 2);
        Require(update(base, "missing", times) == -1 && *error() == 2);
        KernelTimeval invalid[2]{{0, -1}, {0, 0}};
        Require(update(-1, nullptr, invalid) == -1 && *error() == 22);
        invalid[0].tv_usec = 1000000;
        Require(update(base, "data", invalid) == -1 && *error() == 22);
        Require(update(base, "data", nullptr) == 0);
        Require(stat(path.c_str(), &info) == 0 && info.st_mtim.tv_sec > 1000000001);
    } else if (test == "symlinkat") {
        const auto link = LoadFunction<int (APS5_VABI*)(const char*, int, const char*)>(sys, "symlinkat", patched);
        *error() = 71;
        const int result = link("data", base, "link");
#ifdef _WIN32
        if (result == -1 && *error() == 1) {
            std::fputs("SKIP: Windows symbolic-link privileges unavailable\n", stderr);
            skipped = true;
        } else
#endif
        {
            Require(result == 0);
            Require(std::filesystem::read_symlink(root / "link") == "data");
            std::ifstream stream(root / "link");
            std::string contents;
            stream >> contents;
            Require(contents == "data");
            Require(link("data", base, "link") == -1 && *error() == 17);
            Require(link("data", -100, (root / "cwd").string().c_str()) == 0);
            Require(link("data", -1, ("/" + (root / "absolute").string()).c_str()) == 0);
        }
    } else if (test == "symlinkat_errors") {
        const auto link = LoadFunction<int (APS5_VABI*)(const char*, int, const char*)>(sys, "symlinkat", patched);
        Require(link("data", -1, "missing") == -1 && *error() == 9);
        Require(link("data", file, "missing") == -1 && *error() == 20);
        Require(link(nullptr, base, "missing") == -1 && *error() == 14);
        Require(link("data", base, nullptr) == -1 && *error() == 14);
        Require(link("", base, "missing") == -1 && *error() == 2);
        Require(link(std::string(1024, 'x').c_str(), -1, "missing") == -1 && *error() == 63);
        Require(link("data", base, std::string(256, 'x').c_str()) == -1 && *error() == 63);
#ifndef _WIN32
        bool refused = false;
        try { link("../data", base, "refused"); } catch (const std::runtime_error&) { refused = true; }
        Require(refused && !std::filesystem::exists(root / "refused"));
#endif
#ifdef _WIN32
    } else if (test == "symlinkat_refused") {
        const auto link = LoadFunction<int (APS5_VABI*)(const char*, int, const char*)>(sys, "symlinkat", patched);
        link("../data", base, "refused");
#endif
    } else {
        Require(false);
    }
    Require(close(file) == 0 && close(base) == 0);
    std::filesystem::remove_all(root);
    return skipped ? 77 : 0;
}

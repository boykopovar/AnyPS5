#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Import {
    const char* name;
    const char* nid;
};

constexpr std::array<Import, 8> imports{{
    {"Aps5HttpClient_Create", "Xa86mkDT770"},
    {"Aps5HttpClient_Destroy", "FcAd67QjrVw"},
    {"Aps5HttpClient_Open", "T+Un3eukNLs"},
    {"Aps5HttpClient_Read", "mh7PrvIiVtk"},
    {"Aps5HttpClient_Seek", "WN6ypk6qEnU"},
    {"Aps5HttpClient_GetSize", "T8if7aGBDYs"},
    {"Aps5HttpClient_Start", "7EfcZqAehgI"},
    {"Aps5HttpClient_Stop", "M3Qx8MdigHk"}
}};

std::string Utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

#ifdef _WIN32
std::filesystem::path ModulePath(HMODULE module) {
    std::wstring buffer(1024, L'\0');
    for (;;) {
        const auto size = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        Require(size != 0, "GetModuleFileNameW failed");
        if (size < buffer.size()) {
            buffer.resize(size);
            return std::filesystem::canonical(buffer);
        }
        Require(buffer.size() < 32768, "module path is too long");
        buffer.resize(buffer.size() * 2);
    }
}
#endif

}

int main(int argc, char** argv) {
    try {
        Require(argc == 3, "expected module directory and AvPlayer module name");
        const auto directory = std::filesystem::canonical(std::filesystem::u8path(argv[1]));
        const auto player = directory / std::filesystem::u8path(argv[2]);
        const auto expected = std::filesystem::canonical(directory / "libSceHttp.prx");
#ifdef _WIN32
        const auto consumer = LoadLibraryW(player.c_str());
        Require(consumer != nullptr, "loading patched AvPlayer failed");
        const auto provider = GetModuleHandleW(L"libSceHttp.prx");
        Require(provider != nullptr && ModulePath(provider) == expected, "AvPlayer did not load the staged HTTP provider");
        for (const auto& entry : imports) {
            const auto address = GetProcAddress(provider, entry.nid);
            Require(address != nullptr, "patched HTTP helper is missing");
            Require(GetProcAddress(consumer, entry.nid) == nullptr, "HTTP helper was defined in AvPlayer");
            HMODULE owner{};
            Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address), &owner) != 0, "HTTP helper owner lookup failed");
            Require(owner == provider && ModulePath(owner) == expected, "HTTP helper resolved outside libSceHttp");
            std::printf("HTTP provider %s %s %s\n", entry.name, entry.nid, Utf8(expected).c_str());
        }
#else
        const auto consumer = dlopen(player.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!consumer) throw std::runtime_error(dlerror());
        const auto provider = dlopen(expected.c_str(), RTLD_NOW | RTLD_NOLOAD);
        Require(provider != nullptr, "AvPlayer did not load libSceHttp as a dependency");
        for (const auto& entry : imports) {
            const auto address = dlsym(consumer, entry.nid);
            Require(address != nullptr && address == dlsym(provider, entry.nid), "AvPlayer HTTP helper lookup failed");
            Dl_info owner{};
            Require(dladdr(address, &owner) != 0 && owner.dli_fname != nullptr, "HTTP helper owner lookup failed");
            Require(std::filesystem::canonical(owner.dli_fname) == expected, "HTTP helper resolved outside libSceHttp");
            std::printf("HTTP provider %s %s %s\n", entry.name, entry.nid, Utf8(expected).c_str());
        }
#endif
        std::printf("patched %s imports all HTTP helpers from libSceHttp\n", argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "patched HTTP modules: %s\n", error.what());
        return 1;
    }
}

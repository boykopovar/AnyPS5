#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <patched libraries directory>\n", argv[0]);
        return 2;
    }
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
    int loaded = 0;
    int failed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
        if (entry.path().extension() != ".prx") continue;
#ifdef _WIN32
        if (LoadLibraryExW(entry.path().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) != nullptr) {
            ++loaded;
            continue;
        }
        std::fprintf(stderr, "%s: LoadLibraryEx failed with error %lu\n", entry.path().filename().string().c_str(), GetLastError());
#else
        if (dlopen(entry.path().c_str(), RTLD_NOW | RTLD_LOCAL) != nullptr) {
            ++loaded;
            continue;
        }
        std::fprintf(stderr, "%s\n", dlerror());
#endif
        ++failed;
    }
    std::printf("%d patched libraries loaded, %d failed\n", loaded, failed);
    return loaded > 0 && failed == 0 ? 0 : 1;
}

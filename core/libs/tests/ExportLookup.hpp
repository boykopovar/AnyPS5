#pragma once

#include <cstddef>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

inline bool LibraryExportsName(void* library, const char* name) {
#ifdef _WIN32
    return GetProcAddress(static_cast<HMODULE>(library), name) != nullptr;
#else
    return dlsym(library, name) != nullptr;
#endif
}

template <typename Export, std::size_t Count>
bool ExportsResolveByName(const char* path, const Export (&exports)[Count], const char* absentName) {
    if (path == nullptr) {
        std::fprintf(stderr, "no library path was given\n");
        return false;
    }
#ifdef _WIN32
    void* library = LoadLibraryA(path);
#else
    void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (library == nullptr) {
        std::fprintf(stderr, "%s did not load\n", path);
        return false;
    }
    for (const auto& entry : exports) {
        auto* volatile function = entry.function;
        if (function == nullptr || !LibraryExportsName(library, entry.name)) {
            std::fprintf(stderr, "%s is not exported\n", entry.name);
            return false;
        }
    }
    if (LibraryExportsName(library, absentName)) {
        std::fprintf(stderr, "%s resolved although nothing exports it\n", absentName);
        return false;
    }
    return true;
}

#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "environment binding: %s\n", message);
    std::abort();
}

#ifdef _WIN32
using Module = HMODULE;
#else
using Module = void*;
#endif

bool Loaded(const std::filesystem::path& path) {
#ifdef _WIN32
    return GetModuleHandleW(path.c_str()) != nullptr;
#else
    const auto module = ::dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (module == nullptr) return false;
    Require(::dlclose(module) == 0, "cannot release module residency query");
    return true;
#endif
}

Module Open(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto module = LoadLibraryW(path.c_str());
    if (module == nullptr) std::fprintf(stderr, "LoadLibraryW error: %lu\n", GetLastError());
#else
    const auto module = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) std::fprintf(stderr, "dlopen error: %s\n", ::dlerror());
#endif
    Require(module != nullptr, "cannot load libkernel");
    return module;
}

char*** Storage(Module module) {
#ifdef _WIN32
    auto* const storage = reinterpret_cast<char***>(GetProcAddress(module, "environ_nid_postfix"));
#else
    auto* const storage = reinterpret_cast<char***>(::dlsym(module, "environ_nid_postfix"));
#endif
    Require(storage != nullptr, "cannot resolve the environ data object");
    return storage;
}

bool Close(Module module, const std::filesystem::path& path) {
#ifdef _WIN32
    Require(FreeLibrary(module) != 0, "cannot unload libkernel");
#else
    Require(::dlclose(module) == 0, "cannot unload libkernel");
#endif
    const bool unloaded = !Loaded(path);
#ifdef _WIN32
    Require(unloaded, "libkernel remains resident after its last handle closes");
#else
    if (!unloaded)
        std::fprintf(stderr, "environment binding: skipped unload checks because the platform loader retained libkernel; fallback detachment was not verified\n");
#endif
    return unloaded;
}

char* Value(char** environment, std::string_view name) {
    if (environment == nullptr) return nullptr;
    for (; *environment != nullptr; ++environment) {
        const std::string_view entry(*environment);
        const auto separator = entry.find('=');
        if (separator != std::string_view::npos && entry.substr(0, separator) == name)
            return *environment + separator + 1;
    }
    return nullptr;
}

void CheckValue(const char* name, const char* expected) {
    const auto* value = getenv_nid_postfix(name);
    Require(value != nullptr && std::strcmp(value, expected) == 0, "libc returned a different environment value");
}
}

int main() {
    const auto path = std::filesystem::path(GUEST_ENVIRONMENT_KERNEL_PATH);
    Require(!Loaded(path), "libkernel was loaded before the libc-only setup");
    constexpr auto beforeName = "ANYPS5_BINDING_BEFORE_E70B";
    constexpr auto externalName = "ANYPS5_BINDING_EXTERNAL_E70B";
    constexpr auto afterName = "ANYPS5_BINDING_AFTER_E70B";
    constexpr auto reloadedName = "ANYPS5_BINDING_RELOADED_E70B";
    const auto skip = [&] {
        Require(unsetenv_nid_postfix(externalName) == 0, "borrowed entry cleanup before skip failed");
        Require(unsetenv_nid_postfix(afterName) == 0, "fallback entry cleanup before skip failed");
        Require(unsetenv_nid_postfix(reloadedName) == 0, "reloaded entry cleanup before skip failed");
        return 77;
    };
    Require(setenv_nid_postfix(beforeName, "before-load", 1) == 0, "libc-only setenv failed");
    auto* const before = getenv_nid_postfix(beforeName);
    Require(before != nullptr, "libc-only getenv lost its value");

    auto module = Open(path);
    auto* storage = Storage(module);
    Require(Value(*storage, beforeName) == before, "loading libkernel replaced the libc-only environment");
    CheckValue(beforeName, "before-load");
    char external[] = "ANYPS5_BINDING_EXTERNAL_E70B=external";
    char* replacement[] = {external, nullptr};
    *storage = replacement;
    auto* const externalValue = std::strchr(external, '=') + 1;
    Require(getenv_nid_postfix(beforeName) == nullptr, "whole-environ replacement retained the old environment");
    Require(getenv_nid_postfix(externalName) == externalValue, "libc did not adopt the exported environ replacement");

    if (!Close(module, path)) return skip();
    Require(getenv_nid_postfix(externalName) == externalValue, "unloading libkernel lost the borrowed environment");
    Require(setenv_nid_postfix(afterName, "after-unload", 1) == 0, "setenv after unloading libkernel failed");
    CheckValue(afterName, "after-unload");
    CheckValue(externalName, "external");
    Require(!Loaded(path), "libc environment access reloaded libkernel");

    module = Open(path);
    storage = Storage(module);
    Require(Value(*storage, externalName) == externalValue, "reloading libkernel lost the borrowed entry");
    Require(Value(*storage, afterName) == getenv_nid_postfix(afterName), "reloading libkernel lost the fallback mutation");
    Require(Value(*storage, beforeName) == nullptr, "reloading libkernel revived the replaced environment");
    Require(setenv_nid_postfix(reloadedName, "while-reloaded", 1) == 0, "setenv while libkernel was reloaded failed");
    Require(Value(*storage, reloadedName) == getenv_nid_postfix(reloadedName), "reloaded data object did not receive a mutation");
    if (!Close(module, path)) return skip();

    CheckValue(reloadedName, "while-reloaded");
    std::memcpy(externalValue, "modified", 8);
    CheckValue(externalName, "modified");
    Require(unsetenv_nid_postfix(externalName) == 0, "borrowed entry cleanup failed");
    Require(unsetenv_nid_postfix(afterName) == 0, "fallback entry cleanup failed");
    Require(unsetenv_nid_postfix(reloadedName) == 0, "reloaded entry cleanup failed");
    Require(!Loaded(path), "libc cleanup reloaded libkernel");
}

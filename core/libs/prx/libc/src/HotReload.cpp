#include "prx/libc/include/HotReload.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace HotReload {

namespace {

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_initialized{false};
std::mutex g_patchMutex;
std::unordered_map<void*, std::vector<std::uint8_t>> g_originalBytes;

constexpr std::size_t TrampolineSize = 14;

void EnsureInitialized() {
    if (g_initialized.exchange(true)) return;
    const char* env = std::getenv("APS5_HOT_RELOAD");
    if (env != nullptr && *env != '\0' && std::strcmp(env, "0") != 0) {
        g_enabled.store(true);
    }
}

#ifdef _WIN32

bool SetMemoryProtection(void* address, std::size_t size, DWORD protection, DWORD* oldProtection) {
    return VirtualProtect(address, size, protection, oldProtection) != 0;
}

void FlushInstructionCacheFor(void* address, std::size_t size) {
    FlushInstructionCache(GetCurrentProcess(), address, size);
}

#else

bool SetMemoryProtection(void* address, std::size_t size, int protection, int* oldProtection) {
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return false;
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address) & ~(pageSize - 1);
    const std::uintptr_t end = (reinterpret_cast<std::uintptr_t>(address) + size + pageSize - 1) & ~(pageSize - 1);
    if (oldProtection != nullptr) *oldProtection = PROT_READ | PROT_EXEC;
    return mprotect(reinterpret_cast<void*>(start), end - start, protection) == 0;
}

void FlushInstructionCacheFor(void* address, std::size_t size) {
    __builtin___clear_cache(reinterpret_cast<char*>(address), reinterpret_cast<char*>(address) + size);
}

#endif

} // namespace

bool IsEnabled() {
    EnsureInitialized();
    return g_enabled.load();
}

void SetEnabled(const bool enabled) {
    g_initialized.store(true);
    g_enabled.store(enabled);
}

bool PatchFunction(void* target, void* replacement) {
    if (target == nullptr || replacement == nullptr || target == replacement) return false;
    std::lock_guard lock(g_patchMutex);

    const auto existing = g_originalBytes.find(target);
    if (existing != g_originalBytes.end()) {
#ifdef _WIN32
        DWORD oldProtect = 0;
        if (!SetMemoryProtection(reinterpret_cast<std::uint8_t*>(target) + 6, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        std::memcpy(reinterpret_cast<std::uint8_t*>(target) + 6, &replacement, sizeof(replacement));
        SetMemoryProtection(reinterpret_cast<std::uint8_t*>(target) + 6, sizeof(void*), oldProtect, &oldProtect);
        FlushInstructionCacheFor(target, TrampolineSize);
        return true;
#else
        int oldProtect = 0;
        if (!SetMemoryProtection(reinterpret_cast<std::uint8_t*>(target) + 6, sizeof(void*), PROT_READ | PROT_WRITE | PROT_EXEC, &oldProtect)) return false;
        std::memcpy(reinterpret_cast<std::uint8_t*>(target) + 6, &replacement, sizeof(replacement));
        SetMemoryProtection(reinterpret_cast<std::uint8_t*>(target) + 6, sizeof(void*), oldProtect, &oldProtect);
        FlushInstructionCacheFor(target, TrampolineSize);
        return true;
#endif
    }

    std::vector<std::uint8_t> saved(TrampolineSize);
    std::memcpy(saved.data(), target, TrampolineSize);

#ifdef _WIN32
    DWORD oldProtect = 0;
    if (!SetMemoryProtection(target, TrampolineSize, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    std::uint8_t trampoline[TrampolineSize] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(trampoline + 6, &replacement, sizeof(replacement));
    std::memcpy(target, trampoline, TrampolineSize);
    SetMemoryProtection(target, TrampolineSize, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, TrampolineSize);
#else
    int oldProtect = 0;
    if (!SetMemoryProtection(target, TrampolineSize, PROT_READ | PROT_WRITE | PROT_EXEC, &oldProtect)) return false;
    std::uint8_t trampoline[TrampolineSize] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(trampoline + 6, &replacement, sizeof(replacement));
    std::memcpy(target, trampoline, TrampolineSize);
    SetMemoryProtection(target, TrampolineSize, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, TrampolineSize);
#endif

    g_originalBytes.emplace(target, std::move(saved));
    return true;
}

void* GetOriginalFunction(void* target) {
    if (target == nullptr) return nullptr;
    std::lock_guard lock(g_patchMutex);
    const auto it = g_originalBytes.find(target);
    if (it == g_originalBytes.end()) return nullptr;
    return target;
}

bool RestoreFunction(void* target) {
    if (target == nullptr) return false;
    std::lock_guard lock(g_patchMutex);
    const auto it = g_originalBytes.find(target);
    if (it == g_originalBytes.end()) return false;

#ifdef _WIN32
    DWORD oldProtect = 0;
    if (!SetMemoryProtection(target, TrampolineSize, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    std::memcpy(target, it->second.data(), TrampolineSize);
    SetMemoryProtection(target, TrampolineSize, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, TrampolineSize);
#else
    int oldProtect = 0;
    if (!SetMemoryProtection(target, TrampolineSize, PROT_READ | PROT_WRITE | PROT_EXEC, &oldProtect)) return false;
    std::memcpy(target, it->second.data(), TrampolineSize);
    SetMemoryProtection(target, TrampolineSize, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, TrampolineSize);
#endif

    g_originalBytes.erase(it);
    return true;
}

bool ApplyRawPatch(void* target, const void* bytes, const std::size_t size) {
    if (target == nullptr || bytes == nullptr || size == 0) return false;
    std::lock_guard lock(g_patchMutex);
#ifdef _WIN32
    DWORD oldProtect = 0;
    if (!SetMemoryProtection(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    std::memcpy(target, bytes, size);
    SetMemoryProtection(target, size, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, size);
    return true;
#else
    int oldProtect = 0;
    if (!SetMemoryProtection(target, size, PROT_READ | PROT_WRITE | PROT_EXEC, &oldProtect)) return false;
    std::memcpy(target, bytes, size);
    SetMemoryProtection(target, size, oldProtect, &oldProtect);
    FlushInstructionCacheFor(target, size);
    return true;
#endif
}

std::size_t LoadPatchModules(const std::filesystem::path& patchDirectory) {
    std::error_code ec;
    if (!std::filesystem::exists(patchDirectory, ec) || !std::filesystem::is_directory(patchDirectory, ec)) return 0;

    std::size_t patchedTotal = 0;
    const auto shadowDir = patchDirectory / ".shadow";
    std::filesystem::create_directories(shadowDir, ec);

    std::random_device rd;
    std::mt19937_64 rng(rd());

    for (const auto& entry : std::filesystem::directory_iterator(patchDirectory, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const auto ext = entry.path().extension().string();
        if (ext != ".prx" && ext != ".dll" && ext != ".so") continue;

        const auto shadowName = entry.path().stem().string() + "_" + std::to_string(rng()) + ext;
        const auto shadowPath = shadowDir / shadowName;
        std::filesystem::copy_file(entry.path(), shadowPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) continue;

#ifdef _WIN32
        HMODULE hPatch = LoadLibraryExW(shadowPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (hPatch == nullptr) continue;

        const auto filename = entry.path().filename().string();
        HMODULE hBase = GetModuleHandleA(filename.c_str());
        if (hBase == nullptr) {
            hBase = GetModuleHandleW(nullptr);
        }

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hPatch);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const std::uint8_t*>(hPatch) + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                const auto& exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                if (exportDir.VirtualAddress != 0) {
                    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(reinterpret_cast<const std::uint8_t*>(hPatch) + exportDir.VirtualAddress);
                    const auto* names = reinterpret_cast<const DWORD*>(reinterpret_cast<const std::uint8_t*>(hPatch) + exports->AddressOfNames);
                    const auto* ordinals = reinterpret_cast<const WORD*>(reinterpret_cast<const std::uint8_t*>(hPatch) + exports->AddressOfNameOrdinals);
                    const auto* functions = reinterpret_cast<const DWORD*>(reinterpret_cast<const std::uint8_t*>(hPatch) + exports->AddressOfFunctions);

                    for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
                        const char* exportName = reinterpret_cast<const char*>(reinterpret_cast<const std::uint8_t*>(hPatch) + names[i]);
                        const WORD ordinal = ordinals[i];
                        void* patchFunc = const_cast<void*>(reinterpret_cast<const void*>(reinterpret_cast<const std::uint8_t*>(hPatch) + functions[ordinal]));

                        void* baseFunc = nullptr;
                        if (hBase != nullptr) {
                            baseFunc = reinterpret_cast<void*>(GetProcAddress(hBase, exportName));
                        }
                        if (baseFunc == nullptr) {
                            baseFunc = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(nullptr), exportName));
                        }

                        if (baseFunc != nullptr && baseFunc != patchFunc) {
                            if (PatchFunction(baseFunc, patchFunc)) {
                                std::fprintf(stderr, "[HOT-RELOAD] Patched export %s: %p -> %p\n", exportName, baseFunc, patchFunc);
                                ++patchedTotal;
                            }
                        }
                    }
                }
            }
        }
#else
        void* hPatch = dlopen(shadowPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (hPatch == nullptr) continue;
#endif

        std::filesystem::remove(entry.path(), ec);
    }

    return patchedTotal;
}

bool HandleNotImplemented(const char* funcName, void* callerAddress) {
    if (!IsEnabled()) return false;

    std::fprintf(stderr, "\n======================================================================\n");
    std::fprintf(stderr, "[HOT-RELOAD] Unimplemented function called: %s\n", funcName != nullptr ? funcName : "<unknown>");
    std::fprintf(stderr, "[HOT-RELOAD] Caller address: %p\n", callerAddress);
    std::fprintf(stderr, "[HOT-RELOAD] Checking for patches in 'hotreload/' directory...\n");
    std::fprintf(stderr, "======================================================================\n");

    const std::filesystem::path hotreloadDir("hotreload");
    std::size_t patched = LoadPatchModules(hotreloadDir);
    if (patched > 0) {
        std::fprintf(stderr, "[HOT-RELOAD] Applied %zu patches. Resuming...\n", patched);
        return true;
    }

    for (int attempt = 0; attempt < 10; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        patched = LoadPatchModules(hotreloadDir);
        if (patched > 0) {
            std::fprintf(stderr, "[HOT-RELOAD] Patch detected and applied (%zu functions). Resuming...\n", patched);
            return true;
        }
    }

    std::fprintf(stderr, "[HOT-RELOAD] No patch provided after wait period. Terminating as requested.\n");
    return false;
}

std::atomic<bool> g_shaderReloadTriggered{false};

void TriggerShaderReload() {
    g_shaderReloadTriggered.store(true);
}

bool IsShaderReloadTriggered() {
    return g_shaderReloadTriggered.exchange(false);
}

std::mutex g_overrideMutex;
RenderStateOverride g_renderStateOverride;
std::filesystem::file_time_type g_lastOverrideFileWriteTime{};

RenderStateOverride GetRenderStateOverride() {
    std::lock_guard lock(g_overrideMutex);
    return g_renderStateOverride;
}

void SetRenderStateOverride(const RenderStateOverride& overrideState) {
    std::lock_guard lock(g_overrideMutex);
    g_renderStateOverride = overrideState;
}

std::filesystem::file_time_type g_lastPatchesFileWriteTime{};

std::size_t CheckRawPatches() {
    EnsureInitialized();
    if (!g_enabled.load()) return 0;
    const std::filesystem::path patchesPath("hotreload/patches.txt");
    std::error_code ec;
    if (!std::filesystem::exists(patchesPath, ec)) return 0;
    const auto writeTime = std::filesystem::last_write_time(patchesPath, ec);
    if (ec) return 0;
    {
        std::lock_guard lock(g_patchMutex);
        if (writeTime == g_lastPatchesFileWriteTime) return 0;
        g_lastPatchesFileWriteTime = writeTime;
    }
    FILE* file = std::fopen(patchesPath.string().c_str(), "r");
    if (file == nullptr) return 0;
    char line[512];
    std::size_t applied = 0;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char* colon = std::strchr(line, ':');
        if (colon == nullptr) continue;
        *colon = '\0';
        char* endPtr = nullptr;
        const auto addr = std::strtoull(line, &endPtr, 16);
        if (addr == 0 || endPtr == line) continue;
        void* target = reinterpret_cast<void*>(addr);
        char* bytesStr = colon + 1;
        std::vector<std::uint8_t> patchBytes;
        while (*bytesStr != '\0') {
            while (*bytesStr == ' ' || *bytesStr == '\t') ++bytesStr;
            if (*bytesStr == '\0' || *bytesStr == '\r' || *bytesStr == '\n' || *bytesStr == '#') break;
            unsigned int b = 0;
            if (std::sscanf(bytesStr, "%02x", &b) == 1) {
                patchBytes.push_back(static_cast<std::uint8_t>(b));
                bytesStr += 2;
            } else {
                break;
            }
        }
        if (!patchBytes.empty()) {
            if (ApplyRawPatch(target, patchBytes.data(), patchBytes.size())) {
                std::fprintf(stderr, "[HOT-RELOAD] Applied raw patch at %p (%zu bytes)\n", target, patchBytes.size());
                ++applied;
            }
        }
    }
    std::fclose(file);
    return applied;
}

bool CheckRenderStateReload() {
    EnsureInitialized();
    if (!g_enabled.load()) return false;
    const std::filesystem::path overridePath("hotreload/render_state.txt");
    std::error_code ec;
    if (!std::filesystem::exists(overridePath, ec)) return false;
    const auto writeTime = std::filesystem::last_write_time(overridePath, ec);
    if (ec) return false;
    {
        std::lock_guard lock(g_overrideMutex);
        if (writeTime == g_lastOverrideFileWriteTime) return false;
        g_lastOverrideFileWriteTime = writeTime;
    }
    FILE* file = std::fopen(overridePath.string().c_str(), "r");
    if (file == nullptr) return false;
    RenderStateOverride newState;
    char line[256];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char key[128];
        int val = 0;
        if (std::sscanf(line, "%127[^=]=%d", key, &val) == 2) {
            char* trimKey = key;
            while (*trimKey == ' ' || *trimKey == '\t') ++trimKey;
            std::size_t len = std::strlen(trimKey);
            while (len > 0 && (trimKey[len - 1] == ' ' || trimKey[len - 1] == '\t' || trimKey[len - 1] == '\r' || trimKey[len - 1] == '\n')) {
                trimKey[--len] = '\0';
            }
            if (std::strcmp(trimKey, "force_wireframe") == 0) newState.forceWireframe = (val != 0);
            else if (std::strcmp(trimKey, "disable_cull") == 0) newState.disableCull = (val != 0);
            else if (std::strcmp(trimKey, "disable_depth_test") == 0) newState.disableDepthTest = (val != 0);
            else if (std::strcmp(trimKey, "force_depth_write") == 0) newState.forceDepthWrite = (val != 0);
            else if (std::strcmp(trimKey, "force_opaque_alpha") == 0) newState.forceOpaqueAlpha = (val != 0);
            else if (std::strcmp(trimKey, "fallback_magenta") == 0) newState.fallbackMagenta = (val != 0);
            else if (std::strcmp(trimKey, "disable_dcc") == 0) newState.disableDcc = (val != 0);
        }
    }
    std::fclose(file);
    SetRenderStateOverride(newState);
    std::fprintf(stderr, "[HOT-RELOAD] Render state updated from %s:\n", overridePath.string().c_str());
    std::fprintf(stderr, "  force_wireframe=%d disable_cull=%d disable_depth_test=%d force_depth_write=%d force_opaque_alpha=%d fallback_magenta=%d disable_dcc=%d\n",
                 newState.forceWireframe, newState.disableCull, newState.disableDepthTest, newState.forceDepthWrite, newState.forceOpaqueAlpha, newState.fallbackMagenta, newState.disableDcc);
    return true;
}

} // namespace HotReload

extern "C" {
bool HotReloadIsShaderReloadTriggered_nid_no_patch() {
    return HotReload::IsShaderReloadTriggered();
}

HotReload::RenderStateOverride HotReloadGetRenderStateOverride_nid_no_patch() {
    return HotReload::GetRenderStateOverride();
}

bool HotReloadCheckRenderStateReload_nid_no_patch() {
    HotReload::CheckRawPatches();
    return HotReload::CheckRenderStateReload();
}
}



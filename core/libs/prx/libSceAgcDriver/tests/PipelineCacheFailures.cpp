#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "AllocationFailure.hpp"
#include "ShaderCacheDirectory.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <random>
#include <string_view>

using namespace AgcDriver::Graphics;
using namespace std::chrono_literals;

namespace {

void Check(bool value, const char* reason) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", reason);
    std::fflush(stderr);
    std::_Exit(1);
}

std::mutex mutex;
std::condition_variable changed;
std::map<VkPipelineCache, std::vector<std::byte>> caches;
std::uintptr_t next = 1;
std::atomic<bool> armed{false};
std::atomic<bool> failStartup{false};
std::atomic<unsigned> queries{0};
std::atomic<unsigned> faults{0};
std::size_t initialBytes = 64;

std::vector<std::byte> Payload() {
    std::vector<std::byte> bytes(initialBytes);
    const std::array<std::uint32_t, 4> header{32, VK_PIPELINE_CACHE_HEADER_VERSION_ONE, 13, 42};
    std::memcpy(bytes.data(), header.data(), sizeof(header));
    return bytes;
}

VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice, const VkPipelineCacheCreateInfo* info, const VkAllocationCallbacks*, VkPipelineCache* output) {
    std::lock_guard lock(mutex);
    *output = reinterpret_cast<VkPipelineCache>(next++);
    if (info->initialDataSize != 0) {
        const auto* bytes = static_cast<const std::byte*>(info->pInitialData);
        caches[*output] = std::vector<std::byte>(bytes, bytes + info->initialDataSize);
    } else caches[*output] = Payload();
    if (failStartup.exchange(false)) {
        ++faults;
        AgcDriver::Tests::SetAllocationFailureCountdown(0);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice, VkPipelineCache cache, const VkAllocationCallbacks*) {
    std::lock_guard lock(mutex);
    Check(caches.erase(cache) == 1, "destroyed an unknown cache");
    changed.notify_all();
}

VKAPI_ATTR VkResult VKAPI_CALL Merge(VkDevice, VkPipelineCache destination, std::uint32_t count, const VkPipelineCache* sources) {
    std::lock_guard lock(mutex);
    Check(count == 1, "expected one merge source");
    caches.at(destination) = caches.at(sources[0]);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Data(VkDevice, VkPipelineCache cache, std::size_t* size, void* output) {
    std::lock_guard lock(mutex);
    const auto& bytes = caches.at(cache);
    if (output != nullptr) {
        Check(*size >= bytes.size(), "cache snapshot buffer is too small");
        std::memcpy(output, bytes.data(), bytes.size());
    }
    *size = bytes.size();
    if (output == nullptr && armed && ++queries == 2) {
        armed = false;
        ++faults;
        changed.notify_all();
        AgcDriver::Tests::SetAllocationFailureCountdown(0);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreatePipelineCache") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Create);
    if (std::strcmp(name, "vkDestroyPipelineCache") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Destroy);
    if (std::strcmp(name, "vkGetPipelineCacheData") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Data);
    if (std::strcmp(name, "vkMergePipelineCaches") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Merge);
    return nullptr;
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Check(static_cast<bool>(file), "saved cache file is missing");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void Grow(VkPipelineCache cache, std::size_t bytes = 128) {
    std::lock_guard lock(mutex);
    caches.at(cache).resize(bytes, std::byte{9});
}

}

int main(int argc, char** argv) {
    Check(argc == 2, "expected startup, now, busy, destructor, worker or read-retry");
    const std::string_view mode(argv[1]);
    Check(mode == "startup" || mode == "now" || mode == "busy" || mode == "destructor" || mode == "worker" || mode == "read-retry", "unknown mode");
#ifndef _WIN32
    if (mode == "read-retry") return 77;
#endif
    const auto root = std::filesystem::temp_directory_path() / ("aps5-cache-allocation-" + std::to_string(std::random_device{}()));
    Check(std::filesystem::create_directory(root), "test cache directory already exists");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{root};
#ifdef _WIN32
    _putenv_s("ANYPS5_NO_SHADER_CACHE", "0");
    _wputenv_s(L"ANYPS5_SHADER_CACHE_DIR", root.wstring().c_str());
#else
    setenv("ANYPS5_NO_SHADER_CACHE", "0", 1);
    setenv("ANYPS5_SHADER_CACHE_DIR", root.string().c_str(), 1);
#endif
    Context context{};
    context.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    context.deviceProc = Resolve;
    VkPhysicalDeviceProperties properties{};
    properties.vendorID = 13;
    properties.deviceID = 42;
    if (mode == "startup") {
        failStartup = true;
        bool failed = false;
        try {
            PipelineCache attempt(context, properties);
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        Check(failed && faults == 1, "saver startup allocation failure was not propagated");
        Check(caches.empty(), "failed saver startup leaked the Vulkan pipeline cache");
        {
            PipelineCache retry(context, properties);
            Check(retry.Handle() != VK_NULL_HANDLE, "cache creation could not retry after failed saver startup");
        }
        Check(caches.empty(), "successful retry leaked a Vulkan pipeline cache");
        std::puts("startup: propagated the allocation failure, released the cache and allowed recreation");
        return 0;
    }
    if (mode == "read-retry") initialBytes = 32768;
    auto cache = std::make_unique<PipelineCache>(context, properties);
    cache->SaveNow();
    std::filesystem::path file;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) if (entry.path().extension() == ".bin") file = entry.path();
    Check(!file.empty(), "initial save created no cache file");
#ifdef _WIN32
    if (mode == "read-retry") {
        {
            PipelineCache writer(context, properties);
            Grow(writer.Handle(), 40960);
            writer.SaveNow();
        }
        const auto before = Read(file);
        Check(before.size() == 24 + 40960, "the second writer did not update the cache file");
        Grow(cache->Handle(), 65536);
        std::error_code error;
        const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(file, error);
        Check(!error, "could not resolve the test cache path");
        struct TailLock {
            HANDLE handle = INVALID_HANDLE_VALUE;
            OVERLAPPED region{};
            void Release() {
                if (handle == INVALID_HANDLE_VALUE) return;
                Check(UnlockFileEx(handle, 0, 1, 0, &region) != 0, "could not unlock the cache body");
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            }
            ~TailLock() { Release(); }
        } locked;
        locked.handle = CreateFileW(native.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(locked.handle != INVALID_HANDLE_VALUE, "could not open the cache body lock handle");
        locked.region.Offset = 16384;
        Check(LockFileEx(locked.handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &locked.region) != 0,
            "could not lock the cache body");
        {
            std::ifstream header(file, std::ios::binary);
            std::array<char, 24> bytes{};
            Check(static_cast<bool>(header.read(bytes.data(), bytes.size())), "the body lock unexpectedly blocked the header");
        }
        std::vector<std::byte> unreadable;
        Check(!ShaderRecompiler::ReadWholeFile(file, unreadable), "the body lock did not block a complete cache read");
        cache->SaveNow();
        locked.Release();
        Check(Read(file) == before, "a failed cache read overwrote the other writer's file");
        cache->SaveNow();
        Check(Read(file).size() == 24 + 65536, "a transient cache read failure prevented the later save");
        cache.reset();
        Check(caches.empty(), "cache read retry leaked Vulkan objects");
        std::puts("read-retry: kept the unreadable cache file and saved new pipelines after it became readable");
        return 0;
    }
#endif
    const auto original = Read(file);
    Grow(cache->Handle());
    queries = 0;
    armed = true;
    if (mode == "destructor") cache.reset();
    else if (mode == "worker") {
        std::unique_lock lock(mutex);
        Check(changed.wait_for(lock, 14s, [] { return faults == 1; }), "background save did not reach the allocation failure");
    } else if (mode == "busy") cache->SaveUnlessBusy();
    else cache->SaveNow();
    Check(faults == 1 && !armed, "the snapshot allocation was not exercised");
    Check(Read(file) == original, "a failed snapshot replaced the previous cache file");
    {
        std::unique_lock lock(mutex);
        if (mode == "worker") Check(changed.wait_for(lock, 2s, [] { return caches.size() == 1; }), "background snapshot did not release its temporary cache");
        Check(caches.size() == (cache ? 1u : 0u), "failed snapshot leaked its temporary merged cache");
    }
    if (!cache) {
        cache = std::make_unique<PipelineCache>(context, properties);
        Grow(cache->Handle());
    }
    cache->SaveNow();
    Check(Read(file).size() == 24 + 128 && Read(file) != original, "a failed snapshot prevented a later successful save");
    cache.reset();
    Check(caches.empty(), "pipeline cache allocation test leaked Vulkan objects");
    std::printf("%.*s: allocation failure kept the old file, released temporary caches and allowed a retry\n", static_cast<int>(mode.size()), mode.data());
}

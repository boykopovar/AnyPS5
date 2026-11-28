#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/src/MappedCacheFile.hpp"
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
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <psapi.h>
#endif

using namespace AgcDriver::Graphics;
using namespace std::chrono_literals;

namespace AgcDriver::Graphics {

std::uint64_t& DeviceProcLookups() {
    thread_local std::uint64_t count = 0;
    return count;
}

}

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
std::atomic<unsigned> creations{0};
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
    ++creations;
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

#ifdef _WIN32
std::byte Filler(std::size_t index) {
    return static_cast<std::byte>((index * 7u + (index >> 12u)) & 0xffu);
}

bool HasTemporary(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().filename().string().find(".tmp") != std::string::npos) return true;
    }
    return false;
}

void FillLarge(VkPipelineCache cache, std::size_t large) {
    std::lock_guard lock(mutex);
    auto& bytes = caches.at(cache);
    bytes.resize(large);
    for (std::size_t index = initialBytes; index < large; ++index) bytes[index] = Filler(index);
}

constexpr std::size_t PageBytes = 4096;

std::size_t ResidentPages(std::byte* start, std::size_t bytes) {
    std::vector<PSAPI_WORKING_SET_EX_INFORMATION> pages(bytes / PageBytes);
    for (std::size_t index = 0; index < pages.size(); ++index) pages[index].VirtualAddress = start + index * PageBytes;
    Check(K32QueryWorkingSetEx(GetCurrentProcess(), pages.data(), static_cast<DWORD>(pages.size() * sizeof(pages[0]))) != 0, "could not query the working set");
    std::size_t resident = 0;
    for (const auto& page : pages) resident += page.VirtualAttributes.Valid;
    return resident;
}

bool DropsBelowTenth(std::byte* start, std::size_t bytes) {
    for (int attempt = 0; attempt < 250; ++attempt) {
        if (ResidentPages(start, bytes) * 10 <= bytes / PageBytes) return true;
        Sleep(20);
    }
    return false;
}

int RunWorkingSet(const std::filesystem::path& root) {
    const std::size_t window = std::size_t{1} << 20u;
    const std::size_t bytes = 16 * window;
    auto windowed = MappedCacheFile::Create(root / "windowed", bytes, window, {});
    Check(windowed != nullptr, "a windowed mapped file could not be created");
    for (std::size_t at = 0; at < bytes; at += PageBytes) std::memset(windowed->Data() + at, 0x5a, PageBytes);
    const auto behind = ResidentPages(windowed->Data(), bytes - window);
    const auto last = ResidentPages(windowed->Data() + bytes - window, window);
    Check(windowed->WindowsOpened() == 15, "a sequential writer did not open one window after another");
    Check(windowed->ReleaseFailures() == 0, "a window could not be written out or dropped from the working set");
    Check(last * 2 >= window / PageBytes, "the window being written is not in the working set, so the measurement shows nothing");
    Check(behind * 10 <= (bytes - window) / PageBytes, "the windows behind the writer stayed in the working set");
    Check(windowed->Commit(bytes) && std::filesystem::file_size(root / "windowed") == bytes, "the windowed file was not committed");
    auto whole = MappedCacheFile::Create(root / "whole", MappedCacheFile::TrimmedFromBytes, 0, [](std::function<void()> body) { return std::thread(std::move(body)); });
    Check(whole != nullptr, "a mapped file with a helper thread could not be created");
    std::memset(whole->Data(), 0x3c, MappedCacheFile::TrimmedFromBytes);
    Check(DropsBelowTenth(whole->Data(), MappedCacheFile::TrimmedFromBytes), "the helper thread did not drop the written pages from the working set");
    unsigned sum = 0;
    for (std::size_t at = 0; at < MappedCacheFile::TrimmedFromBytes; at += PageBytes) sum += static_cast<unsigned>(whole->Data()[at]);
    Check(sum == 0x3cu * (MappedCacheFile::TrimmedFromBytes / PageBytes), "the pages read back hold other bytes than were written");
    Check(DropsBelowTenth(whole->Data(), MappedCacheFile::TrimmedFromBytes), "the helper thread did not drop the pages read back from the working set");
    const auto failures = whole->ReleaseFailures();
    Check(whole->Commit(MappedCacheFile::TrimmedFromBytes), "the file with a helper thread was not committed");
    std::printf("mapped-working-set: %zu of %zu pages behind the writer and %zu of %zu in its window were resident; the helper thread dropped written and re-read pages (%llu failed calls)\n", behind, (bytes - window) / PageBytes, last, window / PageBytes, static_cast<unsigned long long>(failures));
    return 0;
}

int RunStress(const std::filesystem::path& root) {
    const std::size_t bytes = std::size_t{8} << 20u;
    const std::size_t pages = bytes / PageBytes;
    std::uint64_t opened = 0;
    std::uint64_t churned = 0;
    for (int round = 0; round < 4; ++round) {
        auto shared = MappedCacheFile::Create(root / "shared", bytes, std::size_t{64} << 10u, {});
        Check(shared != nullptr, "the shared mapped file could not be created");
        std::atomic<bool> done{false};
        std::thread churn([&] {
            while (!done.load()) {
                auto small = MappedCacheFile::Create(root / "churn", 2 * PageBytes, PageBytes, {});
                Check(small != nullptr, "a mapped file could not be created while others were being written");
                std::memset(small->Data(), 0x11, 2 * PageBytes);
                Check(small->WindowsOpened() == 1 && small->Commit(2 * PageBytes), "a mapped file could not be committed while others were being written");
                ++churned;
            }
        });
        std::vector<std::thread> writers;
        for (std::size_t writer = 0; writer < 4; ++writer) {
            writers.emplace_back([&, writer] {
                for (std::size_t page = writer; page < pages; page += 4) std::memset(shared->Data() + page * PageBytes, static_cast<int>((page * 7u + 1u) & 0xffu), PageBytes);
            });
        }
        for (auto& writer : writers) writer.join();
        done = true;
        churn.join();
        Check(shared->WindowsOpened() >= 1 && shared->WindowsOpened() <= 127, "the shared file was not opened a window at a time");
        opened += shared->WindowsOpened();
        Check(shared->Commit(bytes), "the shared file was not committed");
        std::vector<std::byte> saved;
        Check(ShaderRecompiler::ReadWholeFile(root / "shared", saved) && saved.size() == bytes, "the shared file has another size than was written");
        bool same = true;
        for (std::size_t page = 0; same && page < pages; ++page) same = saved[page * PageBytes] == static_cast<std::byte>((page * 7u + 1u) & 0xffu) && saved[page * PageBytes + PageBytes - 1] == saved[page * PageBytes];
        Check(same, "the shared file does not hold what its four writers wrote");
    }
    std::printf("mapped-stress: four writers opened %llu windows of one file while %llu other mapped files were created and committed\n", static_cast<unsigned long long>(opened), static_cast<unsigned long long>(churned));
    return 0;
}

bool HoldsLarge(const std::filesystem::path& file, std::size_t large) {
    std::vector<std::byte> saved;
    if (!ShaderRecompiler::ReadWholeFile(file, saved) || saved.size() != 24 + large) return false;
    const auto start = Payload();
    bool same = std::memcmp(saved.data() + 24, start.data(), start.size()) == 0;
    for (std::size_t index = initialBytes; same && index < large; ++index) same = saved[24 + index] == Filler(index);
    return same;
}

std::uint64_t FreeMebibytes() {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    Check(GlobalMemoryStatusEx(&status) != 0, "could not read the host's free memory");
    return status.ullAvailPhys >> 20u;
}

int RunUnthrottled(bool enrollment, std::unique_ptr<PipelineCache> cache, const std::filesystem::path& file, const std::filesystem::path& root, const Context& context, const VkPhysicalDeviceProperties& properties) {
    std::size_t large = (std::size_t{70} << 20u) + 12345u;
    FillLarge(cache->Handle(), large);
    _putenv_s("APS5_PIPELINE_CACHE_MAPPED_SAVE_MB", "1");
    _putenv_s("APS5_PIPELINE_CACHE_SAVE_WINDOW_KIB", "1024");
    std::vector<std::unique_ptr<MappedCacheFile>> blockers;
    if (enrollment) {
        for (int index = 0; index < 4; ++index) {
            blockers.push_back(MappedCacheFile::Create(root / ("blocker-" + std::to_string(index)), 8192, 4096, {}));
            Check(blockers.back() != nullptr, "a mapped file that takes a window slot could not be created");
        }
    } else MappedCacheFileTestAccess::RefuseWindowProtection(true);
    const auto mappedBefore = PipelineCache::MappedSaves();
    bool putOff = false;
    for (int attempt = 0; attempt < 5 && !putOff; ++attempt) {
        const auto free = FreeMebibytes();
        Check(free > 64, "the host has too little free memory for this test");
        _putenv_s("APS5_PIPELINE_CACHE_SAVE_FREE_MB", std::to_string(free - 8).c_str());
        const auto before = Read(file);
        const auto postponedBefore = PipelineCache::PostponedSaves();
        cache->SaveNow();
        Check(PipelineCache::MappedSaves() == mappedBefore, "a save whose view could not be opened a window at a time was written through the mapping");
        Check(!HasTemporary(file.parent_path()), "a rejected mapping left a temporary file");
        putOff = PipelineCache::PostponedSaves() == postponedBefore + 1 && Read(file) == before;
        if (!putOff) {
            Check(HoldsLarge(file, large), "a save that fell back to a buffer wrote another file than the cache");
            large += 4096;
            FillLarge(cache->Handle(), large);
        }
    }
    Check(putOff, "a save that fell back to a buffer was not checked against the reserve for the whole cache");
    _putenv_s("APS5_PIPELINE_CACHE_SAVE_FREE_MB", "0");
    cache->SaveNow();
    Check(PipelineCache::MappedSaves() == mappedBefore, "a save whose view could not be opened a window at a time was written through the mapping");
    Check(HoldsLarge(file, large), "the save that fell back to a buffer does not hold the cache's bytes");
    Check(!HasTemporary(file.parent_path()), "the save that fell back to a buffer left a temporary file");
    blockers.clear();
    MappedCacheFileTestAccess::RefuseWindowProtection(false);
    large += 4096;
    FillLarge(cache->Handle(), large);
    cache->SaveNow();
    Check(PipelineCache::MappedSaves() == mappedBefore + 1, "a save was not mapped again once its view could be opened a window at a time");
    Check(HoldsLarge(file, large), "the mapped save after the fallback does not hold the cache's bytes");
    cache.reset();
    {
        PipelineCache reader(context, properties);
        std::lock_guard lock(mutex);
        Check(caches.at(reader.Handle()).size() == large, "a second cache did not load the save");
    }
    Check(caches.empty(), "the unthrottled mapping test leaked Vulkan objects");
    std::printf("%s: the save was put off for the whole cache's size, then written from a buffer, and mapped again afterwards\n", enrollment ? "mapped-no-slot" : "mapped-no-protection");
    return 0;
}

int RunMapped(bool lowMemory, std::unique_ptr<PipelineCache> cache, const std::filesystem::path& file, const Context& context, const VkPhysicalDeviceProperties& properties) {
    const std::size_t large = (std::size_t{70} << 20u) + 12345u;
    const auto small = Read(file);
    FillLarge(cache->Handle(), large);
    _putenv_s("APS5_PIPELINE_CACHE_MAPPED_SAVE_MB", "1");
    _putenv_s("APS5_PIPELINE_CACHE_SAVE_WINDOW_KIB", "16384");
    _putenv_s("APS5_PIPELINE_CACHE_SAVE_FREE_MB", lowMemory ? "100000000" : "0");
    const auto mappedBefore = PipelineCache::MappedSaves();
    const auto windowsBefore = PipelineCache::MappedSaveWindows();
    const auto postponedBefore = PipelineCache::PostponedSaves();
    cache->SaveNow();
    if (lowMemory) {
        Check(Read(file) == small, "a save the host has no memory for replaced the file");
        Check(PipelineCache::PostponedSaves() == postponedBefore + 1, "a save the host has no memory for was not counted as put off");
        Check(!HasTemporary(file.parent_path()), "a save that was put off left a temporary file");
        cache.reset();
    }
    Check(PipelineCache::MappedSaves() == mappedBefore + 1, lowMemory ? "the teardown save was put off or was not written through a mapped file" : "the large save was not written through a mapped file");
    const auto windows = PipelineCache::MappedSaveWindows() - windowsBefore;
    Check(windows >= 1 && windows <= 4, "the mapped file was not opened to the driver a window at a time");
    Check(HoldsLarge(file, large), "the large save does not hold the cache's bytes under its header");
    Check(!HasTemporary(file.parent_path()), "the large save left a temporary file");
    if (cache) {
        const auto saved = Read(file);
        cache->SaveNow();
        Check(Read(file) == saved, "an idle save after the large one changed the file");
        cache.reset();
    }
    {
        PipelineCache reader(context, properties);
        std::lock_guard lock(mutex);
        Check(caches.at(reader.Handle()).size() == large, "a second cache did not load the large save");
    }
    Check(caches.empty(), "the mapped save test leaked Vulkan objects");
    std::printf("%s: the 70 MiB cache was saved through a mapped file in 16 MiB windows, loaded back, and no temporary file stayed\n", lowMemory ? "mapped-low-memory" : "mapped");
    return 0;
}
#endif

}

int main(int argc, char** argv) {
    Check(argc == 2, "expected startup, now, busy, destructor, worker, read-retry, first-save, external-merge, mapped, mapped-low-memory, mapped-default, mapped-no-slot, mapped-no-protection, mapped-no-helper, mapped-working-set, mapped-stress or setting");
    const std::string_view mode(argv[1]);
    Check(mode == "startup" || mode == "now" || mode == "busy" || mode == "destructor" || mode == "worker" || mode == "read-retry" || mode == "first-save" || mode == "external-merge" || mode == "mapped" || mode == "mapped-low-memory" || mode == "mapped-default" || mode == "mapped-no-slot" || mode == "mapped-no-protection" || mode == "mapped-no-helper" || mode == "mapped-working-set" || mode == "mapped-stress" || mode == "setting", "unknown mode");
    if (mode == "setting") {
        using Value = std::optional<std::uint64_t>;
        Check(PipelineCache::ParseSetting("256", 20) == Value(std::uint64_t{256} << 20u) && PipelineCache::ParseSetting("0", 10) == Value(0), "a whole number was not read as a setting");
        for (const std::string_view text : {"", " 12", "12 ", "12x", "x12", "-1", "+1", "0x10", "1.5", "off", "18446744073709551616"}) Check(!PipelineCache::ParseSetting(text, 0).has_value(), "a setting that is not a whole number in range was accepted");
        Check(PipelineCache::ParseSetting("18446744073709551615", 0) == Value(std::numeric_limits<std::uint64_t>::max()), "the largest value was not read");
        Check(PipelineCache::ParseSetting("17592186044415", 20) == Value(std::uint64_t{17592186044415u} << 20u), "the largest size in MiB was not read");
        Check(!PipelineCache::ParseSetting("17592186044416", 20).has_value() && !PipelineCache::ParseSetting("18014398509481984", 10).has_value(), "a size that overflows when scaled was accepted");
        std::puts("setting: whole numbers in range are read; text, signs, spaces and sizes that overflow when scaled are refused");
        return 0;
    }
#ifndef _WIN32
    if (mode == "read-retry" || mode.starts_with("mapped")) return 77;
#endif
    const auto root = std::filesystem::temp_directory_path() / ("aps5-cache-allocation-" + std::to_string(std::random_device{}()));
    Check(std::filesystem::create_directory(root), "test cache directory already exists");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{root};
#ifdef _WIN32
    if (mode == "mapped-working-set") return RunWorkingSet(root);
    if (mode == "mapped-stress") return RunStress(root);
    if (mode == "mapped-no-helper") {
        const auto refused = MappedCacheFile::Create(root / "refused", MappedCacheFile::TrimmedFromBytes, std::size_t{16} << 20u, [](std::function<void()>) -> std::thread {
            throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
        });
        Check(refused == nullptr, "a mapped file whose helper thread could not start was handed out");
        Check(std::filesystem::is_empty(root), "a mapped file whose helper thread could not start left its temporary file");
        std::vector<std::unique_ptr<MappedCacheFile>> windowed;
        for (int index = 0; index < 4; ++index) {
            windowed.push_back(MappedCacheFile::Create(root / ("windowed-" + std::to_string(index)), 8192, 4096, {}));
            Check(windowed.back() != nullptr, "a mapped file whose helper thread could not start kept its window slot");
        }
        std::puts("mapped-no-helper: a helper thread that cannot start rejects the mapping, removes its file and frees its slot");
        return 0;
    }
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
    if (mode == "first-save") {
        Grow(cache->Handle());
        const auto before = creations.load();
        cache->SaveNow();
        Check(Read(file).size() == 24 + 128, "the save after the first one did not write the grown cache");
        Check(creations == before, "a cache read back and merged the file its own first save wrote");
        cache.reset();
        Check(caches.empty(), "the first-save test leaked Vulkan objects");
        std::puts("first-save: the save after a cache's first one in a new folder wrote the file without reading it back");
        return 0;
    }
    if (mode == "external-merge") {
        {
            PipelineCache writer(context, properties);
            Grow(writer.Handle(), 96);
            writer.SaveNow();
        }
        const auto external = Read(file);
        Check(external.size() == 24 + 96, "the second writer did not update the cache file");
        auto before = creations.load();
        cache->SaveNow();
        Check(creations == before + 1 && Read(file) == external, "a smaller cache did not merge and keep the other writer's file");
        for (const std::size_t bytes : {std::size_t{128}, std::size_t{160}}) {
            Grow(cache->Handle(), bytes);
            before = creations.load();
            cache->SaveNow();
            Check(creations == before + 1, "a cache that lacks another writer's entries saved over the file without merging it");
            Check(Read(file).size() == 24 + bytes, "the save after an external merge did not write the grown cache");
        }
        cache.reset();
        Check(caches.empty(), "the external-merge test leaked Vulkan objects");
        std::puts("external-merge: a cache kept merging the file after another writer changed it, also after settling without a write");
        return 0;
    }
#ifdef _WIN32
    if (mode == "mapped-no-slot" || mode == "mapped-no-protection") return RunUnthrottled(mode == "mapped-no-slot", std::move(cache), file, root, context, properties);
    if (mode == "mapped-default") {
        const std::size_t large = (std::size_t{257} << 20u) + 12345u;
        Grow(cache->Handle(), large);
        _putenv_s("APS5_PIPELINE_CACHE_MAPPED_SAVE_MB", "");
        _putenv_s("APS5_PIPELINE_CACHE_SAVE_FREE_MB", "0");
        const auto mappedBefore = PipelineCache::MappedSaves();
        cache->SaveNow();
        Check(std::filesystem::file_size(file) == 24 + large, "the default save did not write the large cache");
        Check(PipelineCache::MappedSaves() == mappedBefore, "a save was written through a mapped file without APS5_PIPELINE_CACHE_MAPPED_SAVE_MB");
        Check(!HasTemporary(file.parent_path()), "the default save left a temporary file");
        cache.reset();
        Check(caches.empty(), "the default save test leaked Vulkan objects");
        std::puts("mapped-default: without the switch a 257 MiB cache was saved from a buffer");
        return 0;
    }
    if (mode == "mapped" || mode == "mapped-low-memory") return RunMapped(mode == "mapped-low-memory", std::move(cache), file, context, properties);
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
    {
        PipelineCache writer(context, properties);
        Grow(writer.Handle(), 96);
        writer.SaveNow();
    }
    const auto original = Read(file);
    Check(original.size() == 24 + 96, "the second writer did not update the cache file");
    Grow(cache->Handle());
    const auto created = creations.load();
    queries = 0;
    armed = true;
    if (mode == "destructor") cache.reset();
    else if (mode == "worker") {
        std::unique_lock lock(mutex);
        Check(changed.wait_for(lock, 14s, [] { return faults == 1; }), "background save did not reach the allocation failure");
    } else if (mode == "busy") cache->SaveUnlessBusy();
    else cache->SaveNow();
    Check(faults == 1 && !armed, "the snapshot allocation was not exercised");
    Check(creations == created + 1, "the failed snapshot did not merge the other writer's file");
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

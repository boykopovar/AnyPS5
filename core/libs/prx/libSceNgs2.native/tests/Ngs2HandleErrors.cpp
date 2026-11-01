#include "Ngs2Test.hpp"
#include "prx/libSceNgs2.native/include/Api.hpp"

#include <cstdint>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Status(int actual, int expected, const char* message) {
    if (actual != expected) throw std::runtime_error(message);
}

static void CheckExportLinked() {
    auto* volatile linked = &sceNgs2RackQueryInfo;
    Require(linked != nullptr);
}

static void CheckResetOption() {
    Status(sceNgs2SystemResetOption(nullptr), static_cast<int>(0x804A8010u), "null system option did not return the invalid out address error");
    constexpr std::uint64_t size = 0x90;
    constexpr std::uint32_t maxGrainSamples = 512;
    constexpr std::uint32_t numGrainSamples = 256;
    constexpr std::uint32_t sampleRate = 48000;
    constexpr std::uint32_t maxVoiceChannels = 8;
    unsigned char expected[0x90] = {};
    std::memcpy(expected + 0x00, &size, sizeof(size));
    std::memcpy(expected + 0x6c, &maxGrainSamples, sizeof(maxGrainSamples));
    std::memcpy(expected + 0x70, &numGrainSamples, sizeof(numGrainSamples));
    std::memcpy(expected + 0x74, &sampleRate, sizeof(sampleRate));
    std::memcpy(expected + 0x78, &maxVoiceChannels, sizeof(maxVoiceChannels));
    static_assert(sizeof(Ngs2SystemOption) == sizeof(expected));
    Ngs2SystemOption option;
    std::memset(&option, 0xAB, sizeof(option));
    Status(sceNgs2SystemResetOption(&option), SCE_NGS2_OK, "system option reset failed");
    Require(std::memcmp(&option, expected, sizeof(expected)) == 0);
    Ngs2ContextBufferInfo query{};
    Status(sceNgs2SystemQueryBufferSize(&option, &query), SCE_NGS2_OK, "reset system option was rejected");
}

static int allocations = 0;

static std::int32_t APS5_VABI CountedAllocate(Ngs2ContextBufferInfo* info) {
    info->host_buffer = std::calloc(1, info->host_buffer_size);
    allocations++;
    return SCE_NGS2_OK;
}

static std::int32_t APS5_VABI CountedRelease(Ngs2ContextBufferInfo* info) {
    std::free(info->host_buffer);
    allocations--;
    return SCE_NGS2_OK;
}

static std::string Hex(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", static_cast<unsigned>(value));
    return text;
}

static void Returned(int actual, int expected, const char* entry, const std::string& label) {
    if (actual != expected) throw std::runtime_error(std::string(entry) + " with " + label + " returned " + Hex(actual) + " instead of " + Hex(expected));
}

static Ngs2SystemOption Option(std::uint32_t maxGrainSamples = 512, std::uint32_t numGrainSamples = 256, std::uint32_t sampleRate = 48000, std::uint32_t maxVoiceChannels = 8) {
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = maxGrainSamples;
    option.num_grain_samples = numGrainSamples;
    option.sample_rate = sampleRate;
    option.max_voice_channels = maxVoiceChannels;
    return option;
}

static void CheckOption(const Ngs2SystemOption& option, int expected, const std::string& label) {
    constexpr uintptr_t sentinel = 0x5a5a;
    Ngs2ContextBufferInfo query{};
    Returned(sceNgs2SystemQueryBufferSize(&option, &query), expected, "sceNgs2SystemQueryBufferSize", label);
    Require((query.host_buffer_size != 0) == (expected == SCE_NGS2_OK));
    std::vector<std::uint64_t> storage(query.host_buffer_size / sizeof(std::uint64_t) + 1);
    Ngs2ContextBufferInfo buffer{};
    buffer.host_buffer = storage.data();
    buffer.host_buffer_size = query.host_buffer_size;
    const Ngs2BufferAllocator allocator{CountedAllocate, CountedRelease, 0};
    uintptr_t created = sentinel;
    uintptr_t allocated = sentinel;
    Returned(sceNgs2SystemCreate(&option, &buffer, &created), expected, "sceNgs2SystemCreate", label);
    Returned(sceNgs2SystemCreateWithAllocator(&option, &allocator, &allocated), expected, "sceNgs2SystemCreateWithAllocator", label);
    if (expected != SCE_NGS2_OK) {
        Require(created == sentinel && allocated == sentinel && allocations == 0);
        return;
    }
    Require(created != sentinel && allocated != sentinel && allocations == 1);
    for (const auto system : {created, allocated}) {
        Ngs2SystemInfo info{};
        Status(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "system info of a valid option failed");
        Require(info.max_grain_samples == option.max_grain_samples && info.num_grain_samples == option.num_grain_samples && info.sample_rate == option.sample_rate);
        Status(sceNgs2SystemDestroy(system, nullptr), SCE_NGS2_OK, "system destroy of a valid option failed");
    }
    Require(allocations == 0);
}

static void CheckSchedulerOptions() {
    for (std::size_t slot = 0; slot < 4; ++slot) {
        Ngs2JobSchedulerOption scheduler{};
        auto option = Option();
        option.job_scheduler_options[slot] = reinterpret_cast<std::uintptr_t>(&scheduler);
        for (auto capacity : {0u, 1u, 16385u, UINT32_MAX}) {
            scheduler.max_jobs = capacity;
            CheckOption(option, static_cast<int>(0x804A8421u), "invalid scheduler capacity");
        }
        for (auto capacity : {2u, 16u, 16384u}) {
            scheduler.max_jobs = capacity;
            CheckOption(option, SCE_NGS2_OK, "inactive scheduler");
        }
    }
}

struct SchedulerLife {
    struct Handle { SchedulerLife* life; std::uint32_t slot; };
    std::array<Handle, 4> handles{};
    std::vector<int> events;
    int failInit = -1;
    int failQuery = -1;
};

static int APS5_VABI QueryScheduler(const Ngs2JobSchedulerOption* option, std::uint32_t slot, Ngs2ContextBufferInfo* buffer) {
    auto& life = *reinterpret_cast<SchedulerLife*>(option->user_data);
    life.events.push_back(10 + slot);
    if (life.failQuery == static_cast<int>(slot)) return -71;
    *buffer = {};
    buffer->host_buffer_size = 64;
    return SCE_NGS2_OK;
}

static int APS5_VABI InitScheduler(const Ngs2JobSchedulerOption* option, std::uint32_t slot, Ngs2ContextBufferInfo* buffer, std::uintptr_t* handle) {
    auto& life = *reinterpret_cast<SchedulerLife*>(option->user_data);
    Require(buffer->host_buffer != nullptr && buffer->host_buffer_size == 64);
    life.events.push_back(20 + slot);
    if (life.failInit == static_cast<int>(slot)) return -72;
    life.handles[slot] = {&life, slot};
    *handle = reinterpret_cast<std::uintptr_t>(&life.handles[slot]);
    return SCE_NGS2_OK;
}

static int APS5_VABI DestroyScheduler(std::uintptr_t handle) {
    const auto& manager = *reinterpret_cast<SchedulerLife::Handle*>(handle);
    manager.life->events.push_back(30 + manager.slot);
    return SCE_NGS2_OK;
}

static void CheckResetScheduler() {
    Require(sceNgs2JobSchedulerResetOption(nullptr) == static_cast<int>(0x804A8012u));
    Ngs2JobSchedulerOption option{};
    Require(sceNgs2JobSchedulerResetOption(&option) == SCE_NGS2_OK);
    Require(option.size == 112 && option.user_data == 0 && option.num_workers == 4 && option.max_jobs == 1024);
    Require(option.reserved[0] == 32 && option.reserved[1] == 0x20000 && option.reserved[2] == 700 && option.reserved[3] == 0 && option.reserved_tail == 0);
    Require(option.query_handler != nullptr && option.init_handler != nullptr && option.destroy_handler != nullptr && option.submit_handler != nullptr && option.flush_handler != nullptr && option.join_handler != nullptr);
    for (auto count : {0u, 1u, 29u, UINT32_MAX}) {
        option.num_workers = count;
        Ngs2ContextBufferInfo query{};
        Require(option.query_handler(&option, 0, &query) == static_cast<int>(0x804A8422u));
    }
    option.max_jobs = 1;
    Ngs2ContextBufferInfo query{};
    Require(option.query_handler(&option, 0, &query) == static_cast<int>(0x804A8421u));
}

static void CheckJobLifecycle() {
    for (bool allocated : {false, true}) {
        for (int failure = -1; failure < 4; ++failure) {
            SchedulerLife life;
            life.failInit = failure;
            std::array<Ngs2JobSchedulerOption, 4> schedulers{};
            auto option = Option();
            for (std::size_t i = 0; i < schedulers.size(); ++i) {
                auto& scheduler = schedulers[i];
                scheduler.user_data = reinterpret_cast<std::uintptr_t>(&life);
                scheduler.query_handler = QueryScheduler;
                scheduler.init_handler = InitScheduler;
                scheduler.destroy_handler = DestroyScheduler;
                scheduler.max_jobs = 16;
                option.job_scheduler_options[i] = reinterpret_cast<std::uintptr_t>(&scheduler);
            }
            Ngs2ContextBufferInfo query{};
            Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
            Require((life.events == std::vector<int>{10, 11, 12, 13}));
            life.events.clear();
            std::vector<std::uint64_t> storage(query.host_buffer_size / sizeof(std::uint64_t) + 1);
            Ngs2ContextBufferInfo buffer{};
            buffer.host_buffer = storage.data();
            buffer.host_buffer_size = query.host_buffer_size;
            const Ngs2BufferAllocator allocator{CountedAllocate, CountedRelease, 0};
            std::uintptr_t system = 0x5a5a;
            const int result = allocated ? sceNgs2SystemCreateWithAllocator(&option, &allocator, &system)
                                         : sceNgs2SystemCreate(&option, &buffer, &system);
            Require(result == (failure < 0 ? SCE_NGS2_OK : -72));
            std::vector<int> expected;
            if (allocated) expected = {10, 11, 12, 13};
            for (int i = 0; i < (failure < 0 ? 4 : failure + 1); ++i) { expected.push_back(10 + i); expected.push_back(20 + i); }
            if (failure >= 0) {
                for (int i = failure; i-- > 0;) expected.push_back(30 + i);
                Require(system == 0x5a5a && allocations == 0);
            } else {
                Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
                expected.insert(expected.end(), {30, 31, 32, 33});
                Require(allocations == 0);
            }
            Require(life.events == expected);
            life.events.clear();
            life.failQuery = 2;
            query = {};
            Require(sceNgs2SystemQueryBufferSize(&option, &query) == -71 && query.host_buffer_size == 0);
            Require((life.events == std::vector<int>{10, 11, 12}));
        }
    }
}

static void CheckQueryBuffer(const Ngs2SystemOption* option, int expected, const char* label) {
    constexpr uintptr_t userData = 0x1234;
    Ngs2ContextBufferInfo query;
    std::memset(&query, 0xAB, sizeof(query));
    query.user_data = userData;
    Returned(sceNgs2SystemQueryBufferSize(option, &query), expected, "sceNgs2SystemQueryBufferSize", label);
    Require(query.host_buffer == nullptr && query.user_data == userData);
    for (const auto value : query.reserved) Require(value == 0);
    Require((query.host_buffer_size != 0) == (expected == SCE_NGS2_OK));
}

static void CheckCreateArguments() {
    constexpr int outputError = static_cast<int>(0x804A8010u);
    constexpr int bufferError = static_cast<int>(0x804A8020u);
    constexpr int allocatorError = static_cast<int>(0x804A8023u);
    constexpr int rateError = static_cast<int>(0x804A8051u);
    constexpr uintptr_t sentinel = 0x5a5a;
    const auto valid = Option();
    const auto badRate = Option(512, 256, 7000);
    Ngs2ContextBufferInfo query{};
    Status(sceNgs2SystemQueryBufferSize(&valid, &query), SCE_NGS2_OK, "system buffer query failed");
    std::vector<std::uint64_t> storage(query.host_buffer_size / sizeof(std::uint64_t) + 1);
    Ngs2ContextBufferInfo buffer{};
    buffer.host_buffer = storage.data();
    buffer.host_buffer_size = query.host_buffer_size;
    Ngs2ContextBufferInfo noHost = buffer;
    noHost.host_buffer = nullptr;
    const Ngs2BufferAllocator allocator{CountedAllocate, CountedRelease, 0};
    const Ngs2BufferAllocator noAlloc{nullptr, CountedRelease, 0};

    struct CreateCase {
        const Ngs2SystemOption* option;
        const Ngs2ContextBufferInfo* buffer;
        bool handle;
        int expected;
        const char* label;
    };
    for (const auto& c : {CreateCase{&valid, nullptr, true, bufferError, "a null buffer info"},
                          CreateCase{&valid, &noHost, true, bufferError, "a null host buffer"},
                          CreateCase{&badRate, nullptr, false, bufferError, "a null buffer info, handle and bad option"},
                          CreateCase{&badRate, &noHost, false, bufferError, "a null host buffer, handle and bad option"},
                          CreateCase{&badRate, &buffer, false, outputError, "a null handle and bad option"},
                          CreateCase{&badRate, &buffer, true, rateError, "a bad option"}}) {
        uintptr_t created = sentinel;
        Returned(sceNgs2SystemCreate(c.option, c.buffer, c.handle ? &created : nullptr), c.expected, "sceNgs2SystemCreate", c.label);
        Require(created == sentinel);
    }
    uintptr_t created = sentinel;
    Returned(sceNgs2SystemCreate(nullptr, &buffer, &created), SCE_NGS2_OK, "sceNgs2SystemCreate", "a null option");
    Status(sceNgs2SystemDestroy(created, nullptr), SCE_NGS2_OK, "system destroy failed");

    struct AllocatorCase {
        const Ngs2SystemOption* option;
        const Ngs2BufferAllocator* allocator;
        bool handle;
        int expected;
        const char* label;
    };
    for (const auto& c : {AllocatorCase{&valid, nullptr, true, allocatorError, "a null allocator"},
                          AllocatorCase{&valid, &noAlloc, true, allocatorError, "a null alloc handler"},
                          AllocatorCase{&badRate, nullptr, false, allocatorError, "a null allocator, handle and bad option"},
                          AllocatorCase{&badRate, &noAlloc, false, allocatorError, "a null alloc handler, handle and bad option"},
                          AllocatorCase{&badRate, &allocator, false, outputError, "a null handle and bad option"},
                          AllocatorCase{&badRate, &allocator, true, rateError, "a bad option"}}) {
        uintptr_t allocated = sentinel;
        Returned(sceNgs2SystemCreateWithAllocator(c.option, c.allocator, c.handle ? &allocated : nullptr), c.expected, "sceNgs2SystemCreateWithAllocator", c.label);
        Require(allocated == sentinel && allocations == 0);
    }
    uintptr_t allocated = sentinel;
    Returned(sceNgs2SystemCreateWithAllocator(nullptr, &allocator, &allocated), SCE_NGS2_OK, "sceNgs2SystemCreateWithAllocator", "a null option");
    Require(allocations == 1);
    Status(sceNgs2SystemDestroy(allocated, nullptr), SCE_NGS2_OK, "system destroy failed");
    Require(allocations == 0);
}

static void CheckSystemOption() {
    constexpr int outputError = static_cast<int>(0x804A8010u);
    constexpr int sizeError = static_cast<int>(0x804A8013u);
    constexpr int rateError = static_cast<int>(0x804A8051u);
    constexpr int channelsError = static_cast<int>(0x804A8052u);
    constexpr int grainError = static_cast<int>(0x804A8310u);
    constexpr int maxGrainError = static_cast<int>(0x804A8311u);
    constexpr std::uint32_t invalid = 0xffffffffu;

    for (const std::uint32_t size : {0u, 8u, 0x88u, 0x98u, invalid}) {
        auto option = Option();
        option.size = size;
        CheckOption(option, sizeError, "size " + std::to_string(size));
    }
    for (const std::uint32_t maxGrain : {0u, 32u, 63u, 65u, 96u, 2047u, 2049u, 2112u, 4096u, invalid}) {
        CheckOption(Option(maxGrain, 64), maxGrainError, "max grain samples " + std::to_string(maxGrain));
    }
    for (const std::uint32_t maxGrain : {64u, 128u, 576u, 2048u}) {
        CheckOption(Option(maxGrain, 64), SCE_NGS2_OK, "max grain samples " + std::to_string(maxGrain));
        CheckOption(Option(maxGrain, maxGrain), SCE_NGS2_OK, "grain samples " + std::to_string(maxGrain));
    }
    for (const std::uint32_t grain : {0u, 32u, 63u, 65u, 100u, 255u, 257u, 513u, 576u, 2048u, invalid}) {
        CheckOption(Option(512, grain), grainError, "grain samples " + std::to_string(grain));
    }
    for (const std::uint32_t grain : {64u, 192u, 448u, 512u}) {
        CheckOption(Option(512, grain), SCE_NGS2_OK, "grain samples " + std::to_string(grain));
    }
    for (const std::uint32_t rate : {11025u, 12000u, 22050u, 24000u, 44100u, 48000u, 88200u, 96000u, 176400u, 192000u}) {
        CheckOption(Option(512, 256, rate), SCE_NGS2_OK, "sample rate " + std::to_string(rate));
    }
    for (const std::uint32_t rate : {0u, 8000u, 11024u, 11026u, 16000u, 32000u, 47999u, 48001u, 192001u, invalid}) {
        CheckOption(Option(512, 256, rate), rateError, "sample rate " + std::to_string(rate));
    }
    for (const std::uint32_t channels : {1u, 2u, 8u, 36u, 37u}) {
        CheckOption(Option(512, 256, 48000, channels), SCE_NGS2_OK, "voice channels " + std::to_string(channels));
    }
    for (const std::uint32_t channels : {0u, 38u, 39u, invalid}) {
        CheckOption(Option(512, 256, 48000, channels), channelsError, "voice channels " + std::to_string(channels));
    }

    auto badSize = Option(100, 100, 7000, 0);
    badSize.size = 0x88;
    CheckOption(badSize, sizeError, "a bad size and every other field bad");
    CheckOption(Option(100, 100, 7000, 0), maxGrainError, "a bad max grain, grain, rate and channels");
    CheckOption(Option(100, 0, 48000, 8), maxGrainError, "a bad max grain and grain");
    CheckOption(Option(100, 256, 7000, 8), maxGrainError, "a bad max grain and rate");
    CheckOption(Option(100, 256, 48000, 0), maxGrainError, "a bad max grain and channels");
    CheckOption(Option(512, 100, 7000, 0), grainError, "a bad grain, rate and channels");
    CheckOption(Option(512, 576, 48000, 8), grainError, "a grain above the max grain");
    CheckOption(Option(512, 100, 48000, 0), grainError, "a bad grain and channels");
    CheckOption(Option(512, 256, 7000, 0), rateError, "a bad rate and channels");

    auto scheduled = Option();
    for (auto& character : scheduled.name) character = 'n';
    CheckOption(scheduled, SCE_NGS2_OK, "a non-terminated name");

    const auto invalidRate = Option(512, 256, 7000);
    Status(sceNgs2SystemQueryBufferSize(&invalidRate, nullptr), outputError, "invalid option took precedence over the null output");
    Status(sceNgs2SystemQueryBufferSize(nullptr, nullptr), outputError, "null option took precedence over the null output");
    CheckQueryBuffer(nullptr, SCE_NGS2_OK, "a null option");
    CheckQueryBuffer(&invalidRate, rateError, "a refused rate");
    auto invalidSize = Option();
    invalidSize.size = 0x88;
    CheckQueryBuffer(&invalidSize, sizeError, "a refused size");
    const auto valid = Option();
    CheckQueryBuffer(&valid, SCE_NGS2_OK, "a valid option");
}

static void CheckSetGrainSamples() {
    constexpr int systemError = static_cast<int>(0x804A8201u);
    constexpr int grainError = static_cast<int>(0x804A8310u);
    const auto system = CreateSystem();
    Ngs2SystemInfo info{};
    Status(sceNgs2SystemSetGrainSamples(0x1234, 100), systemError, "an invalid system handle lost to an invalid grain");
    for (const std::uint32_t grain : {64u, 192u, 512u}) {
        Status(sceNgs2SystemSetGrainSamples(system, grain), SCE_NGS2_OK, "a valid grain was refused");
        Status(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "system info failed");
        Require(info.num_grain_samples == grain);
    }
    for (const std::uint32_t grain : {0u, 1u, 8u, 63u, 65u, 511u, 513u, 576u, 2048u, 0xffffffffu}) {
        Status(sceNgs2SystemSetGrainSamples(system, grain), grainError, "an invalid grain was accepted");
        Status(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "system info failed");
        Require(info.num_grain_samples == 512);
    }
    Status(sceNgs2SystemDestroy(system, nullptr), SCE_NGS2_OK, "system destroy failed");
}

static void CheckSetSampleRate() {
    constexpr int systemError = static_cast<int>(0x804A8201u);
    constexpr int rateError = static_cast<int>(0x804A8051u);
    const auto system = CreateSystem();
    Ngs2SystemInfo info{};
    Status(sceNgs2SystemSetSampleRate(0x1234, 7000), systemError, "an invalid system handle lost to an invalid rate");
    for (const std::uint32_t rate : {11025u, 12000u, 22050u, 24000u, 44100u, 48000u, 88200u, 96000u, 176400u, 192000u}) {
        Status(sceNgs2SystemSetSampleRate(system, rate), SCE_NGS2_OK, "a valid rate was refused");
        Status(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "system info failed");
        Require(info.sample_rate == rate);
    }
    for (const std::uint32_t rate : {0u, 7000u, 8000u, 16000u, 32000u, 47999u, 48001u, 192001u, 0xffffffffu}) {
        Status(sceNgs2SystemSetSampleRate(system, rate), rateError, "an invalid rate was accepted");
        Status(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "system info failed");
        Require(info.sample_rate == 192000);
    }
    Status(sceNgs2SystemDestroy(system, nullptr), SCE_NGS2_OK, "system destroy failed");
}

static void CheckFftQuerySize() {
    std::uint64_t size = 0;
    Require(sceNgs2FftQuerySize(8, 0, &size) == 0 && size == 2112);
    Require(sceNgs2FftQuerySize(8, 2, &size) == 0 && size == 1088);
    Require(sceNgs2FftQuerySize(9, 0, &size) == 0 && size == 4160);
    Require(sceNgs2FftQuerySize(12, 2, &size) == 0 && size == 16448);
    Require(sceNgs2FftQuerySize(7, 0, &size) == -1);
    Require(sceNgs2FftQuerySize(13, 0, &size) == -1);
    Require(sceNgs2FftQuerySize(8, 0, nullptr) == -1);
}

static void CheckEnumeration() {
    constexpr int invalidArgument = static_cast<int>(0x804A0053u);
    constexpr int invalidSystem = static_cast<int>(0x804A0230u);
    uintptr_t handles[8] = {};
    Require(sceNgs2SystemEnumHandles(nullptr, 0) >= 0);
    Require(sceNgs2SystemEnumHandles(nullptr, 1) == invalidArgument);
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_REVERB);
    const int systems = sceNgs2SystemEnumHandles(handles, 8);
    Require(systems >= 1);
    bool found = false;
    for (int i = 0; i < systems && i < 8; ++i) found = found || handles[i] == system;
    Require(found);
    Require(sceNgs2SystemEnumHandles(nullptr, 0) == systems);
    Require(sceNgs2SystemEnumRackHandles(system, nullptr, 1) == invalidArgument);
    Require(sceNgs2SystemEnumRackHandles(0x1234, handles, 8) == invalidSystem);
    const int racks = sceNgs2SystemEnumRackHandles(system, handles, 8);
    Require(racks == 1 && handles[0] == rack);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    try {
        constexpr int systemError = static_cast<int>(0x804A8201u);
        constexpr int rackError = static_cast<int>(0x804A8202u);
        constexpr int outputError = static_cast<int>(0x804A8010u);
        constexpr uintptr_t sentinel = 0x5a5a;
        uintptr_t value = sentinel;
        Status(sceNgs2SystemGetUserData(0x1234, &value), systemError, "invalid system handle returned a non-PS5 error");
        Require(value == sentinel);
        Status(sceNgs2SystemDestroy(0x1234, nullptr), systemError, "system destroy returned a non-PS5 handle error");
        Status(sceNgs2RackGetVoiceHandle(0x1234, 0, &value), rackError, "invalid rack handle returned a non-PS5 error");
        Require(value == sentinel);
        Status(sceNgs2RackGetVoiceHandle(0x1234, 0, nullptr), rackError, "voice lookup checked output before the rack handle");
        Status(sceNgs2RackDestroy(0x1234, nullptr), rackError, "rack destroy returned a non-PS5 handle error");
        const auto system = CreateSystem();
        const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_REVERB);
        Status(sceNgs2SystemGetUserData(rack, &value), systemError, "rack handle was accepted as a system");
        Require(value == sentinel);
        Status(sceNgs2RackGetVoiceHandle(system, 0, &value), rackError, "system handle was accepted as a rack");
        Require(value == sentinel);
        Status(sceNgs2RackGetVoiceHandle(system, 0, nullptr), rackError, "wrong-kind rack did not take precedence over null output");
        Status(sceNgs2RackGetVoiceHandle(rack, 0, nullptr), outputError, "valid rack lost null-output validation");
        Status(sceNgs2RackGetVoiceHandle(rack, 0, &value), SCE_NGS2_OK, "valid voice lookup failed");
        Require(value != 0 && value != sentinel);
        Status(sceNgs2RackDestroy(rack, nullptr), SCE_NGS2_OK, "valid rack destroy failed");
        value = sentinel;
        Status(sceNgs2RackGetVoiceHandle(rack, 0, &value), rackError, "retired rack returned the wrong error");
        Require(value == sentinel);
        Status(sceNgs2RackGetVoiceHandle(rack, 0, nullptr), rackError, "retired rack did not take precedence over null output");
        Status(sceNgs2SystemDestroy(system, nullptr), SCE_NGS2_OK, "valid system destroy failed");
        Status(sceNgs2SystemGetUserData(system, &value), systemError, "retired system returned the wrong error");
        Require(value == sentinel);
        CheckCreateArguments();
        CheckResetOption();
        CheckSystemOption();
        CheckSchedulerOptions();
        CheckJobLifecycle();
        CheckResetScheduler();
        CheckSetGrainSamples();
        CheckSetSampleRate();
        CheckFftQuerySize();
        CheckEnumeration();
        CheckExportLinked();
        std::cout << "NGS2 PS5 handle errors and validation precedence checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

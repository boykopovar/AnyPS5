#pragma once

#include "Ngs2Test.hpp"

#include <thread>
#include <vector>

struct Manager {
    struct Batch { Ngs2JobHandler handler; void* context; std::uint32_t count; };
    std::vector<Batch> batches;
    unsigned order;
};

static int APS5_VABI Query(const Ngs2JobSchedulerOption*, std::uint32_t slot, Ngs2ContextBufferInfo* buffer) {
    Require(slot == 0);
    *buffer = {};
    buffer->host_buffer_size = 64;
    return SCE_NGS2_OK;
}
static int APS5_VABI Initialize(const Ngs2JobSchedulerOption* option, std::uint32_t slot, Ngs2ContextBufferInfo*, std::uintptr_t* handle) {
    Require(slot == 0);
    *handle = option->user_data;
    return SCE_NGS2_OK;
}
static int APS5_VABI Submit(std::uintptr_t handle, Ngs2JobHandler handler, void* context, std::uint32_t count) {
    reinterpret_cast<Manager*>(handle)->batches.push_back({handler, context, count});
    return SCE_NGS2_OK;
}
static int APS5_VABI Complete(std::uintptr_t handle) {
    auto& manager = *reinterpret_cast<Manager*>(handle);
    for (const auto& batch : manager.batches) {
        std::vector<std::thread> workers;
        if (manager.order == 2) {
            for (std::uint32_t i = 0; i < batch.count; ++i) workers.emplace_back([&, i] { batch.handler(batch.context, i); });
        } else workers.emplace_back([&] {
            for (std::uint32_t i = 0; i < batch.count; ++i) batch.handler(batch.context, manager.order == 0 ? i : batch.count - i - 1);
        });
        for (auto& worker : workers) worker.join();
    }
    manager.batches.clear();
    return SCE_NGS2_OK;
}
static int APS5_VABI Destroy(std::uintptr_t handle) {
    Require(reinterpret_cast<Manager*>(handle)->batches.empty());
    return SCE_NGS2_OK;
}

static std::uintptr_t Sampler(std::uintptr_t system, bool custom) {
    if (!custom) return Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Ngs2CustomSamplerRackOption option{};
    auto& rack = option.custom_rack_option;
    auto& common = rack.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    rack.num_modules = 1;
    rack.num_buffers = 1;
    rack.state_size = 48;
    rack.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT;
    option.max_channel_works = 1;
    option.max_waveform_blocks = 4;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &common, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t handle = 0;
    Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &common, &buffer, &handle) == SCE_NGS2_OK);
    return Voice(handle);
}

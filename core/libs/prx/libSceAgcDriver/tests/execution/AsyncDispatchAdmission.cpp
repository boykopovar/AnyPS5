#include "prx/libSceAgcDriver/Graphics/include/AsyncCompute.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cstdio>
#include <iostream>

namespace {

using AgcDriver::Graphics::AsyncDispatchEligible;
using AgcDriver::Graphics::Require;
using Role = ShaderRecompiler::DescriptorRole;

ShaderRecompiler::DescriptorBinding binding(Role role) {
    ShaderRecompiler::DescriptorBinding result{};
    result.role = role;
    return result;
}

void roles() {
    ShaderRecompiler::RecompileResult compiled{};
    Require(AsyncDispatchEligible(compiled), "dispatch without resources was refused");
    for (const auto role : std::array{Role::GuestBuffers, Role::Gds, Role::FaultBuffer, Role::FlattenedSrt, Role::ShaderData}) {
        compiled.bindings = {binding(role)};
        Require(AsyncDispatchEligible(compiled), "buffer-only dispatch was refused");
    }
    compiled.bindings = {binding(Role::GuestBuffers), binding(Role::Gds), binding(Role::ShaderData)};
    Require(AsyncDispatchEligible(compiled), "mixed buffer-only dispatch was refused");
    const auto bufferOnly = compiled;
    for (const auto role : std::array{Role::GuestImages, Role::GuestSamplers, Role::ImageTable, Role::SamplerTable, Role::ImageTableMap, Role::ImageStorageTable, Role::BdaPagetable, static_cast<Role>(0x7fffffff)}) {
        compiled = bufferOnly;
        compiled.bindings.push_back(binding(role));
        Require(!AsyncDispatchEligible(compiled), "image, sampler, address-table or unknown resource admitted after buffers");
        compiled = bufferOnly;
        compiled.bindings.insert(compiled.bindings.begin(), binding(role));
        Require(!AsyncDispatchEligible(compiled), "unsupported first resource was ignored");
    }
}

void internalScratchAndAddressing() {
    ShaderRecompiler::RecompileResult clean{};
    clean.bindings = {binding(Role::GuestBuffers)};
    auto compiled = clean;
    compiled.workgroupMemoryDwords = 1;
    Require(!AsyncDispatchEligible(compiled), "per-device workgroup scratch admitted across lanes");
    compiled = clean;
    compiled.functionLdsBuffer = true;
    Require(!AsyncDispatchEligible(compiled), "function LDS scratch admitted across lanes");
    compiled = clean;
    compiled.meshWindowBuffer = true;
    Require(!AsyncDispatchEligible(compiled), "mesh window scratch admitted across lanes");
    compiled = clean;
    compiled.bdaAbiVersion = 1;
    Require(!AsyncDispatchEligible(compiled), "address-based dispatch admitted without bounded resource proof");
    Require(!AsyncDispatchEligible(clean, true, false), "shared thread-dimension state admitted across lanes");
    Require(!AsyncDispatchEligible(clean, false, true), "shared diagnostic marker admitted across lanes");
    Require(AsyncDispatchEligible(clean), "refusal changed a separate buffer-only program");
}

}

int main() {
    try {
        roles();
        internalScratchAndAddressing();
        std::puts("async dispatch admission tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

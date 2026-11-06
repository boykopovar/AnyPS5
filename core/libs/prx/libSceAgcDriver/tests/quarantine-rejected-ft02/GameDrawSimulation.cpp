#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace {
using AgcDriver::Graphics::Require;
namespace Harness = DepthFastClearHarness;

struct GameScenario {
    std::string name;
    std::string description;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t drawCalls = 1500;
    std::uint32_t overdrawFactor = 2;
    bool useTextures = true;
    bool useDepth = true;
    bool useBlend = true;
};

// Simulate Dredge-like rendering patterns without launching the game
// This test suite mimics typical game draw instructions for performance testing
}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = device->graphicsContext();
        
        std::string outDir = "G:/UserTemp/giann/game-draw-sim-results";
        std::system("mkdir \"G:/UserTemp/giann/game-draw-sim-results\" 2>nul");
        
        // Define game-like scenarios
        std::vector<GameScenario> scenarios = {
            {"dredge_harbor", "Harbor scene - UI heavy with water reflections", 1280, 720, 2000, 2, true, true, true},
            {"dredge_ocean", "Open ocean - lots of terrain and water", 1280, 720, 1500, 2, true, true, true},
            {"dredge_fishing", "Fishing action - particle effects and UI", 1280, 720, 1800, 3, true, true, true},
            {"dredge_1080p_harbor", "Harbor at 1080p", 1920, 1080, 2000, 2, true, true, true},
        };
        
        std::string gpuName = "unknown", drv = "unknown";
        try {
            gpuName = device->DeviceName();
            drv = device->DriverInfo();
        } catch (...) {}
        
        // Write test manifest
        std::ofstream manifest(outDir + "/game_draw_sim_manifest.json");
        manifest << "{\n  \"type\": \"game_draw_simulation\",\n  \"purpose\": \"Simulate Dredge draw instructions without launching game\",\n  \"gpu\": \"" << gpuName << "\",\n  \"driver\": \"" << drv << "\",\n  \"scenarios\": " << scenarios.size() << "\n}\n";
        manifest.close();
        
        for (const auto& scenario : scenarios) {
            std::vector<double> frameTimes;
            const unsigned warmup = 30;
            const unsigned samples = 150;
            
            // Warmup
            for (unsigned w = 0; w < warmup; ++w) {
                vertexSets = 0;
                const auto depthPtr = reinterpret_cast<std::uintptr_t>(Depths[0].data());
                Harness::Pixels.fill(std::byte{0});
                
                AgcDriver::Graphics::State state{};
                state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
                state.color = {reinterpret_cast<std::uintptr_t>(Harness::Pixels.data()), {Harness::Width, Harness::Height}, VK_FORMAT_R8G8B8A8_UNORM, Harness::Pixels.size(), 0xe4u};
                state.colors = {state.color};
                state.hasColorTarget = true;
                state.depth = AgcDriver::Graphics::DepthTarget{depthPtr, 0, {Harness::Width, Harness::Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
                state.depthTest = true;
                state.depthWrite = true;
                state.depthCompare = VK_COMPARE_OP_LESS;
                state.renderExtent = {Harness::Width, Harness::Height};
                state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                state.viewport = {0, static_cast<float>(Harness::Height), static_cast<float>(Harness::Width), -static_cast<float>(Harness::Height), 0.0f, 1.0f};
                state.scissor = {{0, 0}, {Harness::Width, Harness::Height}};
                state.cullMode = VK_CULL_MODE_BACK_BIT;
                state.blend.colorWriteMask = 15;
                state.blends = {state.blend};
                auto gctx = device->graphicsContext();
                gctx.depthRangeUnrestricted = true;
                AgcDriver::Graphics::ApplyDepthRemap(gctx, state);
                auto target = device->Target();
                ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
                vertexInfo.depthRemapMode = state.depthRemap.mode;
                std::vector<std::uint32_t> vertexUserData(4, 0u);
                Require(vertexSets < VertexSets.size(), "vertex sets");
                auto& vertices = VertexSets[vertexSets++];
                vertices = {{{-1.0f,-1.0f,0.5f,1.0f},{3.0f,-1.0f,0.5f,1.0f},{-1.0f,3.0f,0.5f,1.0f}}};
                const auto vertexBuffer = Harness::BufferDescriptor(vertices.data(), 16u, 3u);
                std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
                const std::span<const std::uint32_t> vertexCode = std::span<const std::uint32_t>(Harness::VertexCode);
                const std::array<ShaderRecompiler::MemoryRegion,1> vm{{{(uintptr_t)vertexCode.data(), std::as_bytes(vertexCode)}}};
                ShaderRecompiler::RecompileRequest vertex{{ShaderRecompiler::ShaderStage::Vertex,(uintptr_t)vertexCode.data(),vertexCode,0,{}},{32,0,vertexUserData,std::nullopt,std::nullopt,vertexInfo,vm},target,{0,0,0,64}};
                vertex.useCache = false;
                auto vhold = std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(vertex));
                ShaderRecompiler::ShaderPixelStageInfo pixel{};
                pixel.wave32 = true;
                std::vector<std::uint32_t> pixelUserData(4,0u);
                const std::array<ShaderRecompiler::MemoryRegion,1> pm{{{(uintptr_t)Harness::PixelCode.data(), std::as_bytes(std::span(Harness::PixelCode))}}};
                ShaderRecompiler::RecompileRequest frag{{ShaderRecompiler::ShaderStage::Fragment,(uintptr_t)Harness::PixelCode.data(),Harness::PixelCode,0,{}},{32,0,pixelUserData,std::nullopt,pixel,std::nullopt,pm},target,PixelPushLayout(vhold->pushConstants.size(),target)};
                frag.useCache = false;
                auto pres = ShaderRecompiler::Recompile(frag);
                std::array<AgcDriver::Graphics::CompiledShader,2> shaders{{{ShaderRecompiler::ShaderStage::Vertex,vhold.get(),0},{ShaderRecompiler::ShaderStage::Fragment,&pres,PixelPushOffset(vhold->pushConstants.size(),target)}}};
                // Simulate multiple draw calls like in game
                for (uint32_t dc = 0; dc < scenario.drawCalls/10; ++dc) {
                    for (uint32_t od = 0; od < scenario.overdrawFactor; ++od) {
                        const AgcDriver::Pm4::DrawParameters draw{0,3,0,1,0,false};
                        device->Draw(state, draw, shaders);
                    }
                }
                device->WaitIdle();
            }
            
            // Measure
            for (unsigned s = 0; s < samples; ++s) {
                vertexSets = 0;
                const auto depthPtr = reinterpret_cast<std::uintptr_t>(Depths[0].data());
                Harness::Pixels.fill(std::byte{0});
                const auto t0 = std::chrono::steady_clock::now();
                
                AgcDriver::Graphics::State state{};
                state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
                state.color = {reinterpret_cast<std::uintptr_t>(Harness::Pixels.data()), {Harness::Width, Harness::Height}, VK_FORMAT_R8G8B8A8_UNORM, Harness::Pixels.size(), 0xe4u};
                state.colors = {state.color};
                state.hasColorTarget = true;
                state.depth = AgcDriver::Graphics::DepthTarget{depthPtr, 0, {Harness::Width, Harness::Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0};
                state.depthTest = true;
                state.depthWrite = true;
                state.depthCompare = VK_COMPARE_OP_LESS;
                state.renderExtent = {Harness::Width, Harness::Height};
                state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                state.viewport = {0, static_cast<float>(Harness::Height), static_cast<float>(Harness::Width), -static_cast<float>(Harness::Height), 0.0f, 1.0f};
                state.scissor = {{0, 0}, {Harness::Width, Harness::Height}};
                state.cullMode = VK_CULL_MODE_BACK_BIT;
                state.blend.colorWriteMask = 15;
                state.blends = {state.blend};
                auto gctx = device->graphicsContext();
                gctx.depthRangeUnrestricted = true;
                AgcDriver::Graphics::ApplyDepthRemap(gctx, state);
                auto target = device->Target();
                ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
                vertexInfo.depthRemapMode = state.depthRemap.mode;
                std::vector<std::uint32_t> vertexUserData(4, 0u);
                Require(vertexSets < VertexSets.size(), "vertex sets");
                auto& vertices = VertexSets[vertexSets++];
                vertices = {{{-1.0f,-1.0f,0.5f,1.0f},{3.0f,-1.0f,0.5f,1.0f},{-1.0f,3.0f,0.5f,1.0f}}};
                const auto vertexBuffer = Harness::BufferDescriptor(vertices.data(), 16u, 3u);
                std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
                const std::span<const std::uint32_t> vertexCode = std::span<const std::uint32_t>(Harness::VertexCode);
                const std::array<ShaderRecompiler::MemoryRegion,1> vm{{{(uintptr_t)vertexCode.data(), std::as_bytes(vertexCode)}}};
                ShaderRecompiler::RecompileRequest vertex{{ShaderRecompiler::ShaderStage::Vertex,(uintptr_t)vertexCode.data(),vertexCode,0,{}},{32,0,vertexUserData,std::nullopt,std::nullopt,vertexInfo,vm},target,{0,0,0,64}};
                vertex.useCache = false;
                auto vhold = std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(vertex));
                ShaderRecompiler::ShaderPixelStageInfo pixel{};
                pixel.wave32 = true;
                std::vector<std::uint32_t> pixelUserData(4,0u);
                const std::array<ShaderRecompiler::MemoryRegion,1> pm{{{(uintptr_t)Harness::PixelCode.data(), std::as_bytes(std::span(Harness::PixelCode))}}};
                ShaderRecompiler::RecompileRequest frag{{ShaderRecompiler::ShaderStage::Fragment,(uintptr_t)Harness::PixelCode.data(),Harness::PixelCode,0,{}},{32,0,pixelUserData,std::nullopt,pixel,std::nullopt,pm},target,PixelPushLayout(vhold->pushConstants.size(),target)};
                frag.useCache = false;
                auto pres = ShaderRecompiler::Recompile(frag);
                std::array<AgcDriver::Graphics::CompiledShader,2> shaders{{{ShaderRecompiler::ShaderStage::Vertex,vhold.get(),0},{ShaderRecompiler::ShaderStage::Fragment,&pres,PixelPushOffset(vhold->pushConstants.size(),target)}}};
                // Simulate game draw calls
                for (uint32_t dc = 0; dc < scenario.drawCalls; ++dc) {
                    for (uint32_t od = 0; od < scenario.overdrawFactor; ++od) {
                        const AgcDriver::Pm4::DrawParameters draw{0,3,0,1,0,false};
                        device->Draw(state, draw, shaders);
                    }
                }
                device->WaitIdle();
                const auto t1 = std::chrono::steady_clock::now();
                frameTimes.push_back(std::chrono::duration<double,std::milli>(t1-t0).count());
            }
            
            std::sort(frameTimes.begin(), frameTimes.end());
            auto pct = [&](double p){ if(frameTimes.empty())return 0.0; double pos=(frameTimes.size()-1)*p; size_t base=(size_t)pos; double frac=pos-base; if(base+1<frameTimes.size()) return frameTimes[base]*(1-frac)+frameTimes[base+1]*frac; return frameTimes[base]; };
            double p50=pct(0.5), p95=pct(0.95), p99=pct(0.99);
            double budget = 1000.0/60.0;
            long over = std::count_if(frameTimes.begin(), frameTimes.end(), [&](double ms){ return ms > budget; });
            
            std::ofstream out(outDir + "/" + scenario.name + "_sim.json");
            out << "{\n  \"scenario\":\"" << scenario.name << "\",\n  \"description\":\"" << scenario.description << "\",\n  \"config\":{\"width\":" << scenario.width << ",\"height\":" << scenario.height << ",\"drawCalls\":" << scenario.drawCalls << ",\"overdrawFactor\":" << scenario.overdrawFactor << ",\"textures\":" << (scenario.useTextures?"true":"false") << "},\n  \"results\":{\"warmup\":" << warmup << ",\"samples\":" << samples << ",\"p50_ms\":" << p50 << ",\"p95_ms\":" << p95 << ",\"p99_ms\":" << p99 << ",\"max_ms\":" << frameTimes.back() << ",\"budget_60fps_ms\":" << budget << ",\"frames_over_budget\":" << over << ",\"over_budget_pct\":" << (100.0*over/samples) << "},\n  \"simulated\":true,\n  \"no_game_launch\":true\n}\n";
            out.close();
            std::cout << "GAME_SIM " << scenario.name << " p50=" << p50 << "ms p95=" << p95 << "ms\n";
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

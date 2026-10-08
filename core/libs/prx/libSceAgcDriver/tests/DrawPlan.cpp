#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <type_traits>

using namespace AgcDriver;
using namespace AgcDriver::DriverDetail;

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void fails(TAction action) {
    bool failed = false;
    try { action(); }
    catch (const std::runtime_error&) { failed = true; }
    check(failed, "invalid runtime inputs were accepted");
}

DrawProgramPlan vertexPlan() {
    DrawProgramPlan plan{};
    plan.userDataBase = 0x8c;
    plan.resourceRegister = 0x8b;
    plan.firstUserSgpr = 8;
    return plan;
}

void testInputs() {
    static_assert(!std::is_copy_constructible_v<DrawPlan>);
    auto plan = vertexPlan();
    QueueState queue{};
    queue.shader[0x8b] = 4;
    queue.shader[0x8c] = 17;
    queue.shader[0x8d] = 23;
    DrawProgram first;
    first.Read(queue, plan);
    queue.shader[0x8c] = 71;
    DrawProgram second;
    second.Read(queue, plan);
    check(first.plan == second.plan && first.UserData()[0] == 17 && second.UserData()[0] == 71, "live inputs changed a previous draw or its plan");
    auto copied = second;
    copied.UserData()[0] = 99;
    check(second.UserData()[0] == 71, "copied inputs alias their original storage");
    queue.shader.erase(0x8d);
    second.Read(queue, plan);
    check(second.UserData().size() == 2 && second.UserData()[1] == 0, "an unset user word did not retain the zero default");
    queue.shader[0x8b] = 0;
    second.Read(queue, plan);
    check(second.UserData().empty(), "shrinking inputs kept stale user words");
    queue.shader[0x8b] = 1u << 27u;
    for (std::uint32_t i = 0; i < 32; ++i) queue.shader[0x8c + i] = 100 + i;
    second.Read(queue, plan);
    check(second.UserData().size() == 32 && second.UserData()[31] == 131, "32-word input bank was truncated");
    queue.shader[0x8b] |= 2;
    fails([&] { second.Read(queue, plan); });
    plan.nullPixel = true;
    second.Read(QueueState{}, plan);
    check(second.UserData().empty(), "null pixel plan read user registers");
}

void testMergedInputs() {
    auto plan = vertexPlan();
    plan.merged = true;
    plan.firstUserSgpr = 0;
    plan.mergedPointer = 0x82;
    QueueState queue{};
    queue.shader[0x8b] = 1u << 27u;
    for (std::uint32_t i = 0; i < 32; ++i) queue.shader[0x8c + i] = i + 1;
    DrawProgram inputs;
    inputs.Read(queue, plan);
    check(inputs.UserData().size() == 40 && inputs.UserData()[39] == 32, "merged input bank was truncated");
    check(std::all_of(inputs.UserData().begin(), inputs.UserData().begin() + 8, [](auto value) { return value == 0; }), "optional merged prefix is not zero");
    alignas(8) std::array<std::uint32_t, 2> data{11, 12};
    const auto address = reinterpret_cast<std::uintptr_t>(data.data());
    queue.shader[0x82] = static_cast<std::uint32_t>(address);
    queue.shader[0x83] = static_cast<std::uint32_t>(address >> 32u);
    plan.mergedPointerRequired = true;
    inputs.Read(queue, plan);
    check(inputs.UserData()[0] == queue.shader.at(0x82) && inputs.UserData()[1] == queue.shader.at(0x83), "merged pointer words were not refreshed");
    queue.shader[0x82] = queue.shader[0x83] = 0;
    fails([&] { inputs.Read(queue, plan); });
    plan.mergedPointerRequired = false;
    inputs.Read(queue, plan);
    check(inputs.UserData()[0] == 0 && inputs.UserData()[1] == 0, "cleared optional pointer kept an old address");
}

std::shared_ptr<DrawEntry> entry(const std::shared_ptr<const DrawPlan>& plan) {
    auto value = std::make_shared<DrawEntry>();
    value->plan = plan;
    return value;
}

void testPlanLifetime() {
    DrawPlanCache cache(2);
    auto firstPlan = std::make_shared<DrawPlan>();
    auto secondPlan = std::make_shared<DrawPlan>();
    firstPlan->programs.push_back(vertexPlan());
    QueueState queue{};
    queue.shader[0x8b] = 2;
    queue.shader[0x8c] = 55;
    auto first = entry(firstPlan);
    first->programs.resize(1);
    first->programs[0].Read(queue, firstPlan->programs[0]);
    std::weak_ptr<DrawEntry> retained = first;
    cache.Keep(1, first);
    first.reset();
    check(!retained.expired() && cache.Find(1)->plan == firstPlan, "shape lost ownership with no exact-cache entry");
    cache.Keep(2, entry(secondPlan));
    auto inUse = cache.Find(1);
    cache.Keep(3, entry(std::make_shared<DrawPlan>()));
    check(cache.Find(2) == nullptr && cache.Find(1) != nullptr && cache.Size() == 2, "plan cache did not evict the least recently used shape");
    auto replacement = entry(firstPlan);
    cache.Keep(1, replacement);
    check(cache.Find(1) == replacement && inUse->plan == firstPlan, "replacing inputs invalidated an in-flight plan");
    cache.Keep(4, entry(secondPlan));
    cache.Keep(5, entry(secondPlan));
    check(cache.Find(1) == nullptr && inUse->plan == firstPlan, "eviction destroyed a plan still in use");
    std::weak_ptr<const DrawPlan> retainedPlan = firstPlan;
    replacement.reset();
    firstPlan.reset();
    check(!retainedPlan.expired() && inUse->programs[0].plan == &inUse->plan->programs[0] && inUse->programs[0].UserData()[0] == 55, "retained inputs lost their owning plan after eviction");
    inUse.reset();
    check(retainedPlan.expired(), "evicted draw plan remained owned after its last consumer retired");
    check(!cache.Admit(0) && cache.Admit(0), "zero exact key was not admitted on its second occurrence");
    check(!cache.Admit(2) && cache.Admit(2) && !cache.Admit(0), "admission hash collision was mistaken for a repeated key");
    DrawPlanCache workload(4096);
    auto stablePlan = std::make_shared<DrawPlan>();
    auto latest = entry(stablePlan);
    for (std::uint64_t i = 0; i < 20000; ++i) {
        check(!workload.Admit(i + 1), "one-shot draw was admitted to the exact cache");
        const auto shape = i % 352 + 1;
        workload.Keep(shape, latest);
        check(workload.Find(shape)->plan == stablePlan, "changing exact keys evicted an active shape");
    }
    check(workload.Size() == 352, "shape storage grew with unique runtime inputs");
}

void testStageLifetime() {
    DrawRecipeRecord recipe;
    std::array<std::shared_ptr<DispatchVariant>, MaxDrawPrograms> retained;
    {
        std::array<DrawStage, MaxDrawPrograms> outer;
        for (std::size_t i = 0; i < outer.size(); ++i) {
            outer[i].fresh = std::make_shared<DispatchVariant>();
            outer[i].fresh->pushOffset = i * 4;
            outer[i].vertexInfo.emplace().resourcesNum = i + 1;
            retained[i] = outer[i].fresh;
            recipe.stages.push_back(outer[i].fresh);
        }
        {
            std::array<DrawStage, MaxDrawPrograms> retry;
            for (auto& stage : retry) {
                stage.fresh = std::make_shared<DispatchVariant>();
                stage.vertexInfo.emplace().resourcesNum = 31;
            }
            for (std::size_t i = 0; i < outer.size(); ++i) {
                check(outer[i].fresh == retained[i] && outer[i].vertexInfo->resourcesNum == i + 1, "nested draw replaced an outer draw's working set");
            }
        }
    }
    check(recipe.Matches(retained) && !recipe.Expired(), "retained stage variants died with the draw working set");
    check(!recipe.Matches(std::span(retained).first(MaxDrawPrograms - 1)), "recipe accepted a truncated stage list");
    std::swap(retained.front(), retained.back());
    check(!recipe.Matches(retained), "recipe accepted reordered stage ownership");
    std::swap(retained.front(), retained.back());
    retained[1].reset();
    check(recipe.Expired() && !recipe.Matches(retained), "released variant remained valid in a recipe");
}

struct LegacyProgram : DrawProgramPlan {
    std::vector<std::uint32_t> userData;
};

struct LegacyDecode {
    Graphics::State state;
    ShaderRecompiler::ShaderPixelStageInfo pixel;
    std::vector<LegacyProgram> programs;
    std::vector<ShaderRecompiler::ProgramRole> roles;
};

void benchmark() {
    DrawPlan plan;
    plan.state.colors.resize(4);
    plan.state.blends.resize(4);
    plan.state.color.extent.width = 1920;
    plan.programs = {vertexPlan(), vertexPlan()};
    plan.programs[1].userDataBase = 0xc;
    plan.programs[1].resourceRegister = 0xb;
    plan.roles = {ShaderRecompiler::ProgramRole::Main, ShaderRecompiler::ProgramRole::Fragment};
    LegacyDecode legacy{plan.state, plan.pixel, {}, plan.roles};
    QueueState queue{};
    for (const auto& source : plan.programs) {
        queue.shader[source.resourceRegister] = 1u << 27u;
        for (std::uint32_t i = 0; i < 32; ++i) queue.shader[source.userDataBase + i] = i;
        legacy.programs.push_back({source, std::vector<std::uint32_t>(32)});
    }
    constexpr std::uint32_t count = 100000;
    std::array<double, 7> previous{}, current{};
    std::uint64_t expected = 0;
    const auto run = [&](bool reuse, double& output) {
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint32_t i = 0; i < count; ++i) {
            queue.shader[0x8c] = i;
            if (reuse) {
                std::array<DrawProgram, 2> inputs;
                for (std::size_t stage = 0; stage < inputs.size(); ++stage) inputs[stage].Read(queue, plan.programs[stage]);
                checksum += inputs[0].UserData()[0] + inputs[1].UserData()[31] + plan.state.color.extent.width;
            } else {
                auto refreshed = std::make_shared<LegacyDecode>(legacy);
                for (auto& program : refreshed->programs) {
                    program.userData.clear();
                    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, program.resourceRegister);
                    const auto resources = readRegister(queue.shader, program.resourceRegister);
                    const auto users = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
                    for (std::uint32_t word = 0; word < users; ++word) {
                        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, program.userDataBase + word);
                        program.userData.push_back(readRegister(queue.shader, program.userDataBase + word));
                    }
                }
                auto programs = refreshed->programs;
                checksum += programs[0].userData[0] + programs[1].userData[31] + refreshed->state.color.extent.width;
            }
        }
        output = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / count;
        if (expected == 0) expected = checksum;
        check(checksum == expected, "plan benchmark changed runtime values");
    };
    for (std::size_t pass = 0; pass < current.size(); ++pass) {
        if ((pass & 1u) == 0) { run(false, previous[pass]); run(true, current[pass]); }
        else { run(true, current[pass]); run(false, previous[pass]); }
    }
    std::sort(previous.begin(), previous.end());
    std::sort(current.begin(), current.end());
    std::printf("Plan/input preparation, 2 stages x32 live words, median of 7 x100000: copied decode %.3f us, immutable plan %.3f us; checksum %llu\n", previous[3], current[3], static_cast<unsigned long long>(expected));
}

}

int main(int argc, char** argv) {
    try {
        testInputs();
        testMergedInputs();
        testPlanLifetime();
        testStageLifetime();
        if (argc == 2 && std::string_view(argv[1]) == "--benchmark") benchmark();
        std::puts("Draw plan inputs, ownership, eviction and admission passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

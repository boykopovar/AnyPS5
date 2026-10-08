#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAWPLAN_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAWPLAN_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace AgcDriver::Graphics { class VertexStagePlan; }

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t MaxDrawPrograms = 5;

struct DrawProgramPlan {
    ShaderRecompiler::ShaderBinary binary;
    std::uint32_t userDataBase;
    std::uint32_t firstUserSgpr = 8;
    std::array<ShaderRecompiler::MemoryRegion, 2> memory;

    std::shared_ptr<const ShaderSnapshot> snapshot;
    std::size_t codeOffset = 0;
    std::uint32_t resourceRegister = 0;
    bool nullPixel = false;
    bool merged = false;
    bool mergedPointerRequired = false;
    std::uint32_t mergedPointer = 0;
    std::shared_ptr<const Graphics::VertexStagePlan> vertexInputs;
};

struct DrawProgram {
    const DrawProgramPlan* plan = nullptr;
    std::span<std::uint32_t> UserData() { return std::span(words).first(wordCount); }
    std::span<const std::uint32_t> UserData() const { return std::span(words).first(wordCount); }
    void Read(const QueueState& queue, const DrawProgramPlan& source);

private:
    std::array<std::uint32_t, 40> words{};
    std::uint32_t wordCount = 0;
};

struct DrawPlan {
    DrawPlan() = default;
    DrawPlan(const DrawPlan&) = delete;
    DrawPlan& operator=(const DrawPlan&) = delete;
    Graphics::State state;
    ShaderRecompiler::ShaderPixelStageInfo pixel;
    std::vector<DrawProgramPlan> programs;
    std::vector<ShaderRecompiler::ProgramRole> roles;
};

}

#endif

#include "VulkanTestDevice.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

int main() {
    try {
        std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        AgcDriver::Graphics::State state{};
        state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
        AgcDriver::DriverDetail::DrawProgram program{};
        ShaderRecompiler::RecompileResult result{};
        result.vertexOffsetSgpr = -1;
        result.instanceOffsetSgpr = -1;
        ShaderRecompiler::VertexAttribute attribute{};
        attribute.components = 4;
        attribute.formatComponents = 4;
        result.vertexAttributes.push_back(attribute);
        std::uint32_t checked = 0;
        for (const auto instance : {0u, 1u}) {
            for (const auto stride : {0u, 16u}) {
                for (const auto select : {0u, 1u, 2u, 3u}) {
                    for (const auto records : {0u, 1u, 15u, 16u, 303u}) {
                        auto& input = result.vertexAttributes.front();
                        input.fetchIndex = instance;
                        input.resource.fields = {0x10000u, stride << 16u, records, 0x0004dfacu | (select << 28u)};
                        AgcDriver::Pm4::DrawParameters draw{};
                        draw.indexed = false;
                        draw.instanceCount = 1;
                        draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{0x10000u, 0x24u, 16u, 16u, 1u, false, 0u, 0x280u, 0x280u, 0x280u, false, 0u};
                        const auto route = AgcDriver::DriverDetail::Driver::ClassifyIndirectDraw(result, state, program, device, draw, false);
                        const bool resolve = stride != 0 && ((select == 2u && records != 0) || (select == 3u && records >= 16));
                        if (resolve) {
                            if (!route || *route != AgcDriver::Graphics::IndirectDrawPath::VertexFetchRange) throw std::runtime_error("an unbounded strided vertex fetch stayed on the GPU route");
                        } else if (route) {
                            throw std::runtime_error("unexpected route " + std::to_string(static_cast<unsigned>(*route)) + " for stride " + std::to_string(stride) + " select " + std::to_string(select) + " records " + std::to_string(records));
                        }
                        ++checked;
                    }
                }
            }
        }
        std::cout << "Indirect vertex range classifications checked: " << checked << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

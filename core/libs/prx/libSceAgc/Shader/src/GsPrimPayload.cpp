#include "prx/libSceAgc/Shader/include/GsPrimPayload.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

namespace {

constexpr std::uint32_t IDX0_EXPORT_FORMAT_MASK = 0xFu;
constexpr std::uint32_t SPI_SHADER_2COMP = 2u;
constexpr std::uint32_t TwoComponentPayloadBytes = 8u;

}

extern "C" {

int APS5_VABI sceAgcGetGsPrimPayload(std::uint32_t* payload, const Shader* gs) {
    if (payload == nullptr || gs == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": payload or gs is null");
    }
    if (gs->num_cx_registers != 0 && gs->cx_registers == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": gs->cx_registers is null but num_cx_registers != 0");
    }

    std::uint32_t bytes = 0;
    for (std::uint32_t i = 0; i < gs->num_cx_registers; ++i) {
        const ShaderRegister& reg = gs->cx_registers[i];
        if (reg.offset == ShaderRegs::SPI_SHADER_IDX_FORMAT && (reg.value & IDX0_EXPORT_FORMAT_MASK) == SPI_SHADER_2COMP) {
            bytes = TwoComponentPayloadBytes;
            break;
        }
    }
    *payload = bytes;
    return 0;
}

}

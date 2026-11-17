#include "prx/libSceHmd2/include/Api.hpp"
#include "prx/libSceHmd2/Hmd2.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::int32_t InvalidHandle = static_cast<std::int32_t>(0x8a720003);
constexpr std::int32_t ParameterNull = static_cast<std::int32_t>(0x8a720008);
constexpr std::int32_t ParameterInvalid = static_cast<std::int32_t>(0x8a720009);

constexpr std::uint8_t Sentinel = 0x5a;
constexpr std::size_t ProbeSize = 16 * 1024;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

struct Probe {
    unsigned char* bytes;
    Probe() : bytes(new unsigned char[ProbeSize]) {
        for (std::size_t index = 0; index < ProbeSize; ++index) bytes[index] = Sentinel;
    }
    ~Probe() { delete[] bytes; }
    bool Untouched() const {
        for (std::size_t index = 0; index < ProbeSize; ++index) {
            if (bytes[index] != Sentinel) return false;
        }
        return true;
    }
};

}

int main() {
    Probe first;
    Probe second;
    Require(sceHmd2GazeObjectSelectionQueryMemory(nullptr, nullptr) == ParameterNull, "query memory with null arguments");
    Require(sceHmd2GazeObjectSelectionQueryMemory(nullptr, reinterpret_cast<Hmd2::GazeObjectSelectionMemoryInfo*>(second.bytes)) == ParameterNull,
            "query memory with a null parameter");
    Require(second.Untouched(), "query memory left the output buffer untouched with a null parameter");
    Require(sceHmd2GazeObjectSelectionQueryMemory(reinterpret_cast<const Hmd2::GazeObjectSelectionMemoryParameter*>(first.bytes), nullptr) == ParameterNull,
            "query memory with a null info");
    Require(first.Untouched(), "query memory left the parameter buffer untouched with a null info");
    Require(sceHmd2GazeObjectSelectionQueryMemory(reinterpret_cast<const Hmd2::GazeObjectSelectionMemoryParameter*>(first.bytes),
                                                  reinterpret_cast<Hmd2::GazeObjectSelectionMemoryInfo*>(second.bytes)) == ParameterInvalid,
            "query memory before initialization with present arguments");
    Require(first.Untouched() && second.Untouched(), "query memory wrote nothing before initialization");

    Require(sceHmd2GazeObjectSelectionGetSelectedObject(nullptr, nullptr) == ParameterNull, "get selected object with null arguments");
    Require(sceHmd2GazeObjectSelectionGetSelectedObject(nullptr, reinterpret_cast<Hmd2::GazeObjectSelectionResult*>(second.bytes)) == ParameterNull,
            "get selected object with a null parameter");
    Require(sceHmd2GazeObjectSelectionGetSelectedObject(reinterpret_cast<const Hmd2::GazeObjectSelectionParameter*>(first.bytes), nullptr) == ParameterNull,
            "get selected object with a null result");
    Require(sceHmd2GazeObjectSelectionGetSelectedObject(reinterpret_cast<const Hmd2::GazeObjectSelectionParameter*>(first.bytes),
                                                        reinterpret_cast<Hmd2::GazeObjectSelectionResult*>(second.bytes)) == ParameterInvalid,
            "get selected object before initialization with present arguments");
    Require(first.Untouched() && second.Untouched(), "get selected object wrote nothing before initialization");

    Require(sceHmd2GazeGetCalibrationDataMaxSize(0, nullptr) == ParameterNull, "calibration data max size with a null output");
    Require(sceHmd2GazeGetCalibrationDataMaxSize(0, reinterpret_cast<std::uint32_t*>(first.bytes)) == ParameterInvalid,
            "calibration data max size before initialization with a present output");
    Require(first.Untouched(), "calibration data max size wrote nothing before initialization");

    Require(sceHmd2GazeInsertRawDataWithHandle(0, nullptr) == ParameterNull, "insert raw data with a null buffer");
    Require(sceHmd2GazeInsertRawDataWithHandle(0, reinterpret_cast<Hmd2::GazeRawData*>(first.bytes)) == InvalidHandle,
            "insert raw data before initialization with a present buffer");
    Require(first.Untouched(), "insert raw data wrote nothing before initialization");
    return 0;
}

#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <vector>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestDrawIndexBounds() {
    AgcDriver::Pm4::DrawParameters draw{};
    draw.indexCount = 0u;
    draw.instanceCount = 1u;
    draw.indexed = false;
    
    std::string rejected;
    bool traceIndirect = false;
    AgcDriver::QueueState queue{};
    std::vector<std::uint32_t> packet{0xc0000000u};
    
    AgcDriver::DriverDetail::Driver driver;
    const auto verdict = driver.precheckDraw(queue, {}, 0, packet, draw, rejected, traceIndirect);
    Check(verdict == AgcDriver::DriverDetail::DrawVerdict::Nothing, "zero-count draw should return Nothing");

    draw.indexCount = 0xffffffffu;
    draw.instanceCount = 0u;
    const auto verdictZeroInstance = driver.precheckDraw(queue, {}, 0, packet, draw, rejected, traceIndirect);
    Check(verdictZeroInstance == AgcDriver::DriverDetail::DrawVerdict::Nothing, "zero-instance draw should return Nothing");
}

void TestBufferCountValidation() {
    AgcDriver::Pm4::DrawParameters draw{};
    draw.indexCount = 3u;
    draw.instanceCount = 1u;
    draw.indexed = true;

    std::string rejected;
    bool traceIndirect = false;
    AgcDriver::QueueState queue{};
    std::vector<std::uint32_t> packet{0xc0000000u};

    AgcDriver::DriverDetail::Driver driver;
    const auto verdict = driver.precheckDraw(queue, {}, 0, packet, draw, rejected, traceIndirect);
    Check(verdict.has_value(), "valid indexed draw should pass precheck");
}

}

int main() {
    try {
        TestDrawIndexBounds();
        TestBufferCountValidation();
        std::printf("agc draw index bounds and buffer count tests passed\n");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "test failed: %s\n", error.what());
        return 1;
    }
    return 0;
}

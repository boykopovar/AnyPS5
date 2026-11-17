#include "prx/libSceAgc/Misc/include/Debug.hpp"
#include "prx/libSceAgc/DcbDraw/include/Stats.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

void testShaderInstrumentation() {
    Require(sceAgcGetShaderInstrumentation() == 0, "shader instrumentation starts cleared");
    Require(sceAgcSetShaderInstrumentation(0x80000005u) == 0, "setting shader instrumentation failed");
    Require(sceAgcGetShaderInstrumentation() == 0x80000005u, "shader instrumentation flags were not kept");
    Require(sceAgcSetShaderInstrumentation(0) == 0 && sceAgcGetShaderInstrumentation() == 0, "shader instrumentation was not cleared");
}

void testOcclusionQuerySizes() {
    Require(sceAgcDcbBeginOcclusionQueryGetSize(0) == 0x10u, "begin occlusion query size for type 0");
    Require(sceAgcDcbBeginOcclusionQueryGetSize(1) == 0x120u, "begin occlusion query size for type 1");
    Require(sceAgcDcbBeginOcclusionQueryGetSize(2) == 0, "begin occlusion query size for an unknown type");
    Require(sceAgcDcbEndOcclusionQueryGetSize() == 0x10u, "end occlusion query size");
}

}

int main() {
    try {
        testShaderInstrumentation();
        testOcclusionQuerySizes();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC shader instrumentation and occlusion query size tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

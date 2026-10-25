#include "prx/libSceAgc/Misc/include/Debug.hpp"
#include "prx/libSceAgc/DcbDraw/include/Stats.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testShaderInstrumentation() {
    check(sceAgcGetShaderInstrumentation() == 0, "shader instrumentation starts cleared");
    check(sceAgcSetShaderInstrumentation(0x80000005u) == 0, "setting shader instrumentation failed");
    check(sceAgcGetShaderInstrumentation() == 0x80000005u, "shader instrumentation flags were not kept");
    check(sceAgcSetShaderInstrumentation(0) == 0 && sceAgcGetShaderInstrumentation() == 0, "shader instrumentation was not cleared");
}

void testOcclusionQuerySizes() {
    check(sceAgcDcbBeginOcclusionQueryGetSize(0) == 0x10u, "begin occlusion query size for type 0");
    check(sceAgcDcbBeginOcclusionQueryGetSize(1) == 0x120u, "begin occlusion query size for type 1");
    check(sceAgcDcbBeginOcclusionQueryGetSize(2) == 0, "begin occlusion query size for an unknown type");
    check(sceAgcDcbEndOcclusionQueryGetSize() == 0x10u, "end occlusion query size");
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

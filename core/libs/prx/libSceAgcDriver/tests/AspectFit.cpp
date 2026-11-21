#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void expectDoubleEq(double actual, double expected, const char* message) {
    if (std::abs(actual - expected) > 1e-9) {
        std::printf("FAIL: %s (expected %f, got %f)\n", message, expected, actual);
        std::exit(1);
    }
}

static void expectSizeEq(AgcDriver::AspectFitSize actual, AgcDriver::AspectFitSize expected, const char* message) {
    if (actual.width != expected.width || actual.height != expected.height) {
        std::printf("FAIL: %s (expected %ux%u, got %ux%u)\n", message, expected.width, expected.height, actual.width, actual.height);
        std::exit(1);
    }
}

static void expectUintEq(std::uint32_t actual, std::uint32_t expected, const char* message) {
    if (actual != expected) {
        std::printf("FAIL: %s (expected %u, got %u)\n", message, expected, actual);
        std::exit(1);
    }
}

static void expectThrow(auto&& fn, const char* message) {
    try {
        fn();
        std::printf("FAIL: %s (expected exception)\n", message);
        std::exit(1);
    } catch (const std::runtime_error&) {
    } catch (...) {
        std::printf("FAIL: %s (wrong exception type)\n", message);
        std::exit(1);
    }
}

int main() {
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(0, 100, 100, 100, false); }, "ComputeContainScale: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 0, 100, 100, false); }, "ComputeContainScale: sourceHeight==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 100, 0, 100, false); }, "ComputeContainScale: boundsWidth==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 100, 100, 0, false); }, "ComputeContainScale: boundsHeight==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(0, 100, 100, 100, true); }, "ComputeContainScale allowUpscale: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 0, 100, 100, true); }, "ComputeContainScale allowUpscale: sourceHeight==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 100, 0, 100, true); }, "ComputeContainScale allowUpscale: boundsWidth==0");
    expectThrow([] { AgcDriver::ComputeContainScale_nid_postfix(100, 100, 100, 0, true); }, "ComputeContainScale allowUpscale: boundsHeight==0");

    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(0, 100, 100, 100, false); }, "ComputeContainSize: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 0, 100, 100, false); }, "ComputeContainSize: sourceHeight==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 100, 0, 100, false); }, "ComputeContainSize: boundsWidth==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 100, 100, 0, false); }, "ComputeContainSize: boundsHeight==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(0, 100, 100, 100, true); }, "ComputeContainSize allowUpscale: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 0, 100, 100, true); }, "ComputeContainSize allowUpscale: sourceHeight==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 100, 0, 100, true); }, "ComputeContainSize allowUpscale: boundsWidth==0");
    expectThrow([] { AgcDriver::ComputeContainSize_nid_postfix(100, 100, 100, 0, true); }, "ComputeContainSize allowUpscale: boundsHeight==0");

    expectThrow([] { AgcDriver::ComputeWidthForHeight_nid_postfix(0, 100, 100); }, "ComputeWidthForHeight: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeWidthForHeight_nid_postfix(100, 0, 100); }, "ComputeWidthForHeight: sourceHeight==0");

    expectThrow([] { AgcDriver::ComputeHeightForWidth_nid_postfix(0, 100, 100); }, "ComputeHeightForWidth: sourceWidth==0");
    expectThrow([] { AgcDriver::ComputeHeightForWidth_nid_postfix(100, 0, 100); }, "ComputeHeightForWidth: sourceHeight==0");

    expectDoubleEq(AgcDriver::ComputeContainScale_nid_postfix(100, 50, 1920, 1080, false), 1.0, "ComputeContainScale no upscale: 100x50 into 1920x1080");
    expectDoubleEq(AgcDriver::ComputeContainScale_nid_postfix(3840, 2160, 1920, 1080, false), 0.5, "ComputeContainScale no upscale: 3840x2160 into 1920x1080");
    expectDoubleEq(AgcDriver::ComputeContainScale_nid_postfix(100, 50, 400, 400, true), 4.0, "ComputeContainScale upscale: 100x50 into 400x400");
    expectDoubleEq(AgcDriver::ComputeContainScale_nid_postfix(100, 200, 400, 400, true), 2.0, "ComputeContainScale upscale: 100x200 into 400x400");

    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(100, 50, 1920, 1080, false), {100, 50}, "ComputeContainSize no upscale: 100x50 in 1920x1080");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(1920, 1080, 1024, 768, true), {1024, 576}, "ComputeContainSize upscale letterbox: 1920x1080 into 1024x768");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(1600, 900, 1024, 768, true), {1024, 576}, "ComputeContainSize upscale letterbox: 1600x900 into 1024x768");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(1920, 1080, 800, 600, true), {800, 450}, "ComputeContainSize upscale letterbox: 1920x1080 into 800x600");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(800, 600, 1920, 1080, true), {1440, 1080}, "ComputeContainSize upscale pillarbox: 800x600 into 1920x1080");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(800, 600, 1024, 768, false), {800, 600}, "ComputeContainSize no upscale: 800x600 in 1024x768");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(10000, 1, 10, 10, true), {10, 1}, "ComputeContainSize extreme: 10000x1 into 10x10");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(1, 10000, 10, 10, true), {1, 10}, "ComputeContainSize extreme: 1x10000 into 10x10");
    expectSizeEq(AgcDriver::ComputeContainSize_nid_postfix(100, 100, 101, 101, true), {101, 101}, "ComputeContainSize rounding: 100x100 into 101x101");

    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(100, 50, 100), 200, "ComputeWidthForHeight: 100x50 at height 100");
    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(100, 100, 101), 101, "ComputeWidthForHeight: 100x100 at height 101");
    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(3, 2, 5), 8, "ComputeWidthForHeight: 3x2 at height 5 (round 7.5->8)");
    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(100, 100, 1), 1, "ComputeWidthForHeight: 100x100 at height 1");
    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(1, 10000, 1), 1, "ComputeWidthForHeight extreme: 1x10000 at height 1 -> min 1");
    expectUintEq(AgcDriver::ComputeWidthForHeight_nid_postfix(1, 10000, 10), 1, "ComputeWidthForHeight extreme: 1x10000 at height 10 -> min 1");

    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(100, 50, 200), 100, "ComputeHeightForWidth: 100x50 at width 200");
    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(100, 100, 101), 101, "ComputeHeightForWidth: 100x100 at width 101");
    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(2, 3, 5), 8, "ComputeHeightForWidth: 2x3 at width 5 (round 7.5->8)");
    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(100, 100, 1), 1, "ComputeHeightForWidth: 100x100 at width 1");
    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(10000, 1, 1), 1, "ComputeHeightForWidth extreme: 10000x1 at width 1 -> min 1");
    expectUintEq(AgcDriver::ComputeHeightForWidth_nid_postfix(10000, 1, 10), 1, "ComputeHeightForWidth extreme: 10000x1 at width 10 -> min 1");

    std::printf("All tests passed\n");
    return 0;
}

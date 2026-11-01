#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTHDR_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTHDR_HPP

#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include <cmath>
#include <cstdint>

namespace AgcDriver {

struct HostDisplayHdr {
    bool hdr = false;
    float maxLuminance = 0.0f;
    float minLuminance = 0.0f;
    float maxFullFrameLuminance = 0.0f;
};

struct HostHdrStatus {
    bool available = false;
    bool surfaceHdr10 = false;
    HostDisplayHdr display{};
    std::uint64_t generation = 0;
    std::uint64_t displayGeneration = 0;
    float reportedToneMapPeak = 0.0f;
};

inline constexpr HostDisplayHdr UndescribedHdrDisplay{true, 1000.0f, 0.0f, 1000.0f};

inline HostDisplayHdr DisplayHdrFromOutput(bool hdr10ColorSpace, float maxLuminance, float minLuminance, float maxFullFrameLuminance) {
    if (!hdr10ColorSpace) return {};
    if (!std::isfinite(maxLuminance) || maxLuminance < 1.0f) return UndescribedHdrDisplay;
    HostDisplayHdr display{true, maxLuminance, 0.0f, maxLuminance};
    if (std::isfinite(minLuminance) && minLuminance >= 0.0f && minLuminance < maxLuminance) display.minLuminance = minLuminance;
    if (std::isfinite(maxFullFrameLuminance) && maxFullFrameLuminance >= 1.0f && maxFullFrameLuminance <= maxLuminance) display.maxFullFrameLuminance = maxFullFrameLuminance;
    return display;
}

inline float ToneMapPeakNits(const HostHdrStatus& status) {
    return status.available ? status.display.maxLuminance : SdrToneMapLuminanceNits;
}

inline float PqToSdrWhiteNits(const HostHdrStatus& status) {
    return status.reportedToneMapPeak >= 1.0f ? status.reportedToneMapPeak : ToneMapPeakNits(status);
}

void SetPresentingHdr10(bool hdr10);

}

extern "C" void AgcDriverSetHostDisplayHdr_nid_postfix(const AgcDriver::HostDisplayHdr& display);
extern "C" void AgcDriverSetSurfaceHdr10_nid_postfix(bool offered);
extern "C" void AgcDriverNoteReportedToneMapPeak_nid_postfix(float nits);
extern "C" void AgcDriverHostHdr_nid_postfix(AgcDriver::HostHdrStatus& status);
extern "C" bool AgcDriverPresentingHdr10_nid_postfix();

namespace AgcDriver {

inline HostHdrStatus CurrentHostHdr() {
    HostHdrStatus status;
    AgcDriverHostHdr_nid_postfix(status);
    return status;
}

}

#endif

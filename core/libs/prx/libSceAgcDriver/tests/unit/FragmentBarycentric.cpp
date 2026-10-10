#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/FragmentBarycentric.hpp"

#include <string>

namespace {

using Testing::Case;
using Testing::RequireEqual;

struct Driver {
    VkDriverId driver;
    bool reported;
    bool usable;
};

void RequireUsability(const Driver& item) {
    RequireEqual(AgcDriver::FragmentShaderBarycentricUsable(item.driver, item.reported), item.usable,
                 "driver " + std::to_string(static_cast<int>(item.driver)) + " reported " + std::to_string(item.reported ? 1 : 0) + " fragment barycentric usability");
}

const Case moltenVk{"FragmentShaderBarycentricUsable_MoltenVk_IsUnusableEvenWhenReported", [] {
    RequireUsability({VK_DRIVER_ID_MOLTENVK, true, false});
    RequireUsability({VK_DRIVER_ID_MOLTENVK, false, false});
}};

const Case reportingDrivers{"FragmentShaderBarycentricUsable_ReportedByRadvNvidiaOrAmd_IsUsable", [] {
    RequireUsability({VK_DRIVER_ID_MESA_RADV, true, true});
    RequireUsability({VK_DRIVER_ID_NVIDIA_PROPRIETARY, true, true});
    RequireUsability({VK_DRIVER_ID_AMD_PROPRIETARY, true, true});
}};

const Case unreported{"FragmentShaderBarycentricUsable_NotReported_IsUnusable", [] {
    RequireUsability({VK_DRIVER_ID_MESA_RADV, false, false});
    RequireUsability({VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS, false, false});
}};

} // namespace

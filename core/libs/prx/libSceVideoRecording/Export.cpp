#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoRecording/libSceVideoRecording.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80a80003);
constexpr int ErrorNotOpen = static_cast<int>(0x80a80006);

}

extern "C" {

int APS5_VABI sceVideoRecordingSetInfo_nid_disambig1(std::int32_t info, const void*, std::size_t) {
    switch (info) {
    case 0x9004:
    case 0xa004:
    case 0xa006:
        return ErrorNotOpen;
    default:
        return ErrorInvalidArgument;
    }
}

}

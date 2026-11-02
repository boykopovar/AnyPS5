#include "prx/libSceBgft/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int notSupported = static_cast<int>(0x80990006);
    std::byte heap[1024]{};
    const BgftInitParam initialized{heap, sizeof(heap)};
    Require(sceBgftServiceIntInit(&initialized) == 0);
    Require(sceBgftServiceIntInit(nullptr) == 0);

    char contentId[] = "UP0001-CUSA00000_00-AAAAAAAAAAAAAAAA";
    char text16[] = "https://example.invalid/package.pkg";
    char text32[] = "Example";
    char text40[] = "";
    char text48[] = "";
    char text64[] = "0";
    char text72[] = "";
    char text80[] = "";
    char text88[] = "";
    BgftStorageTaskParam params{};
    params.value0 = 1;
    params.value4 = 5;
    params.contentId = contentId;
    params.text16 = text16;
    params.text32 = text32;
    params.text40 = text40;
    params.text48 = text48;
    params.value56 = 7;
    params.text64 = text64;
    params.text72 = text72;
    params.text80 = text80;
    params.text88 = text88;
    params.value96 = 4096;
    params.value104 = 0;

    std::int32_t taskId = 0x5a5a5a5a;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(&params, &taskId) == notSupported);
    Require(taskId == 0x5a5a5a5a);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(nullptr, nullptr) == notSupported);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage() == notSupported);
    Require(sceBgftServiceIntTerm() == 0);
    return 0;
}

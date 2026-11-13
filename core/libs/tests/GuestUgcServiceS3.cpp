#include "prx/libSceUgcServiceS3/libSceUgcServiceS3.hpp"

#include <array>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int noClient = static_cast<int>(0x81b70002u);

}

int main() {
    const std::array<decltype(&sceUgcServiceS3CreateRequest), 12> checked{
        sceUgcServiceS3CreateRequest,
        sceUgcServiceS3Unknown02,
        sceUgcServiceS3CancelRequest,
        sceUgcServiceS3Unknown03,
        sceUgcServiceS3Unknown04,
        sceUgcServiceS3Unknown05,
        sceUgcServiceS3Unknown06,
        sceUgcServiceS3UploadFileWithGmsResponse,
        sceUgcServiceS3Unknown07,
        sceUgcServiceS3Unknown08,
        sceUgcServiceS3DownloadFileWithGmsResponse,
        sceUgcServiceS3Unknown09,
    };
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto call : checked) {
            Require(call() == noClient);
        }
        Require(sceUgcServiceS3Unknown01() == 0);
    }
    return 0;
}

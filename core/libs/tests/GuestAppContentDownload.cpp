#include "prx/libSceAppContent/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AppContent download: %s\n", message);
    std::exit(1);
}

int main(int argc, char** argv) {
    Require(argc == 2, "expected a quota in MiB");
    const auto quotaMiB = std::stoull(argv[1]);
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "could not install process parameters");
    const std::uint32_t sdkVersion = 0x01500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("appcontent-download-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "could not create test directory");
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":)" << quotaMiB << '}';
        Require(static_cast<bool>(param), "could not write metadata");
    }
    AppContentInitParam init{};
    AppContentBootParam boot{};
    Require(sceAppContentInitialize(&init, &boot) == 0, "could not initialize");
    AppContentMountPoint mount{};
    std::memcpy(mount.data, "/download0", 11);
    std::size_t available = 12345;
    Require(sceAppContentDownloadDataGetAvailableSpaceKb(nullptr, &available) == static_cast<int>(0x80D90002u), "null mount validation changed");
    Require(available == 12345, "invalid query wrote output");
    std::size_t blockSize = 12345;
    Require(sceAppContentDownloadDataGetBlockSize(nullptr, &blockSize) == static_cast<int>(0x80D90002u) && blockSize == 12345,
            "null block-size mount validation changed");
    Require(sceAppContentDownloadDataGetBlockSize(&mount, nullptr) == static_cast<int>(0x80D90002u), "a null block-size output was accepted");
    Require(sceAppContentDownloadDataGetBlockSize(&mount, &blockSize) == 0, "the download block size was refused");
    Require(blockSize > 0 && (blockSize & (blockSize - 1)) == 0, "the download block size is not a power of two");
    AppContentMountPoint temporary{};
    std::memcpy(temporary.data, "/temp0", 6);
    Require(sceAppContentDownloadDataGetBlockSize(&temporary, &blockSize) == static_cast<int>(0x80D90004u),
            "a non-download mount reported a block size");
    const auto backing = ResolvePath_nid_no_patch("/download0");
    if (quotaMiB > 18014398509481983ull) {
        const bool existed = std::filesystem::exists(backing);
        bool overflow = false;
        try {
            sceAppContentDownloadDataGetAvailableSpaceKb(&mount, &available);
        } catch (const std::overflow_error&) {
            overflow = true;
        }
        Require(overflow, "unrepresentable quota did not throw overflow_error");
        Require(available == 12345, "overflow changed output");
        Require(std::filesystem::exists(backing) == existed, "overflow created storage");
    } else {
        Require(sceAppContentDownloadDataGetAvailableSpaceKb(&mount, &available) == 0, "valid quota query failed");
        if (quotaMiB == 0) {
            Require(available == 0, "zero quota returned space");
        } else if (quotaMiB == 1) {
            Require(std::filesystem::space(backing).available > 2 * 1024 * 1024, "test needs 2 MiB of host space");
            Require(available == 1024, "empty one-MiB quota is incorrect");
            std::filesystem::create_directory(backing / "nested");
            for (const auto bytes : {0u, 1u, 1023u, 1024u, 1025u}) {
                const auto file = backing / "nested" / std::to_string(bytes);
                { std::ofstream output(file, std::ios::binary); }
                std::filesystem::resize_file(file, bytes);
            }
            Require(sceAppContentDownloadDataGetAvailableSpaceKb(&mount, &available) == 0 && available == 1019, "per-file KiB rounding is incorrect");
            std::filesystem::resize_file(backing / "nested" / "1025", 1024 * 1024);
            Require(sceAppContentDownloadDataGetAvailableSpaceKb(&mount, &available) == 0 && available == 0, "over-quota usage did not clamp to zero");
        } else {
            Require(available > 0, "largest representable quota returned no space");
        }
    }
    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
}

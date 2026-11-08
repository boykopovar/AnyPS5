#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios_directory_exists check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(condition) Require((condition), __LINE__)

static std::atomic<int> g_completed{0};
static std::atomic<int> g_lastError{0};

static int APS5_VABI CountEvents(void*, SceFiosOp, SceFiosOpEvent event, int error) {
    if (event == SCE_FIOS_OPEVENT_COMPLETE) {
        g_lastError = error;
        ++g_completed;
    }
    return 0;
}

static SceFiosOpAttr CountingAttr() {
    SceFiosOpAttr attr{};
    attr.pCallback = CountEvents;
    return attr;
}

int main() {
    std::uint32_t params[16] = {};
    params[0] = sizeof(params) << 1;
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    REQUIRE(sceFiosIsInitialized(nullptr));

    const std::filesystem::path directory = "fios_direxists_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "test.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out << "0123456789";
    }

    REQUIRE(sceFiosDirectoryExistsSync(nullptr, directory.generic_string().c_str()) == 1);
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, file.c_str()) == 0);
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, (directory / "missing").generic_string().c_str()) == 0);
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, nullptr) == 0);
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, "") == 0);

    const std::string tooLong(2000, 'a');
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, tooLong.c_str()) == 0);

    bool exists = false;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp op = sceFiosDirectoryExists(&attr, directory.generic_string().c_str(), &exists);
    REQUIRE(op > 0);
    REQUIRE(sceFiosOpSyncWait(op) == 1);
    REQUIRE(exists == true);
    REQUIRE(sceFiosOpGetActualCount(op) == 1);
    REQUIRE(sceFiosOpGetError(op) == SCE_FIOS_OK);
    REQUIRE(g_completed == 1);
    REQUIRE(g_lastError == SCE_FIOS_OK);
    sceFiosOpDelete(op);

    const SceFiosOp missingOp = sceFiosDirectoryExists(nullptr, (directory / "missing").generic_string().c_str(), &exists);
    REQUIRE(missingOp > 0);
    REQUIRE(sceFiosOpSyncWait(missingOp) == 0);
    REQUIRE(exists == false);
    sceFiosOpDelete(missingOp);

    sceFiosTerminate();
    REQUIRE(!sceFiosIsInitialized(nullptr));
    std::filesystem::remove_all(directory);
    std::puts("guest_fios_directory_exists ok");
    return 0;
}

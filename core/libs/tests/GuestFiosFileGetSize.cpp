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
        std::fprintf(stderr, "guest_fios_filegetsize check failed at line %d\n", line);
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

    const std::filesystem::path directory = "fios_filegetsize_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "test.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out << "0123456789ABCDEF";
    }

    REQUIRE(sceFiosFileGetSizeSync(nullptr, file.c_str()) == 16);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, "") == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, (directory / "missing").c_str()) == SCE_FIOS_ERROR_BAD_PATH);

    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp op = sceFiosFileGetSize(&attr, file.c_str());
    REQUIRE(op > 0);
    REQUIRE(sceFiosOpSyncWait(op) == 16);
    REQUIRE(sceFiosOpGetActualCount(op) == 16);
    REQUIRE(sceFiosOpGetError(op) == SCE_FIOS_OK);
    REQUIRE(g_completed == 1);
    REQUIRE(g_lastError == SCE_FIOS_OK);
    sceFiosOpDelete(op);

    const SceFiosOp badOp = sceFiosFileGetSize(nullptr, nullptr);
    REQUIRE(sceFiosOpGetError(badOp) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosOpSyncWait(badOp) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosOpDelete(badOp);

    sceFiosTerminate();
    REQUIRE(!sceFiosIsInitialized(nullptr));
    std::filesystem::remove_all(directory);
    std::puts("guest_fios_filegetsize ok");
    return 0;
}

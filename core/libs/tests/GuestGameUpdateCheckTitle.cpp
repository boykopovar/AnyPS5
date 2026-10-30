#include "prx/libSceGameUpdate/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int NotInitialized = static_cast<int>(0x80412801u);
constexpr int InvalidArgument = static_cast<int>(0x80412803u);
constexpr int InvalidSize = static_cast<int>(0x80412804u);
constexpr int RequestNotFound = static_cast<int>(0x80412805u);
constexpr int Aborted = static_cast<int>(0x80412807u);

using CheckTitleFn = int (APS5_VABI*)(int, std::uint32_t, GameUpdateCheckResult*);

CheckTitleFn ResolveCheckTitle() {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceGameUpdate.prx"), "sceGameUpdateCheckTitle"));
#else
    void* symbol = dlsym(RTLD_DEFAULT, "sceGameUpdateCheckTitle");
#endif
    if (!symbol) {
        std::fprintf(stderr, "sceGameUpdateCheckTitle is not exported by libSceGameUpdate\n");
        std::abort();
    }
    return reinterpret_cast<CheckTitleFn>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GameUpdate CheckTitle: %s\n", message);
    std::abort();
}

GameUpdateCheckResult FilledResult() {
    GameUpdateCheckResult result;
    std::memset(&result, 0x5a, sizeof(result));
    result.size = sizeof(result);
    return result;
}

bool Untouched(const GameUpdateCheckResult& result) {
    const GameUpdateCheckResult expected = FilledResult();
    return std::memcmp(&result, &expected, sizeof(result)) == 0;
}

}

int main() {
    const auto checkTitle = ResolveCheckTitle();
    GameUpdateCheckResult result = FilledResult();

    Require(checkTitle(0x20000000, 0, nullptr) == InvalidArgument, "a null result was taken before initialization");
    result.size = sizeof(result) + 8;
    Require(checkTitle(0x20000000, 0, &result) == InvalidSize, "a wrong result size was taken before initialization");
    result = FilledResult();
    Require(checkTitle(0x20000000, 0, &result) == NotInitialized && Untouched(result), "a check ran before initialization");

    Require(sceGameUpdateInitialize() == 0, "the library could not be initialized");
    for (int id : {0, -1, 0x20000000}) {
        Require(checkTitle(id, 0, &result) == RequestNotFound && Untouched(result), "a check ran for a request that was never created");
    }
    const int request = sceGameUpdateCreateRequest();
    Require(request >= 0x20000000, "a request could not be created");
    Require(checkTitle(request, 0, nullptr) == InvalidArgument, "a null result was taken for a live request");
    result.size = sizeof(result) - 1;
    Require(checkTitle(request, 0, &result) == InvalidSize, "a short result was taken for a live request");
    result = FilledResult();
    Require(checkTitle(request, 7, &result) == 0, "a live request was not checked");
    GameUpdateCheckResult cleared{};
    cleared.size = sizeof(cleared);
    Require(std::memcmp(&result, &cleared, sizeof(result)) == 0, "the result was not cleared to no update");

    Require(sceGameUpdateAbortRequest(request) == 0, "the request could not be aborted");
    result = FilledResult();
    Require(checkTitle(request, 0, &result) == Aborted && Untouched(result), "an aborted request was checked");
    Require(sceGameUpdateDeleteRequest(request) == 0, "the request could not be deleted");
    Require(checkTitle(request, 0, &result) == RequestNotFound && Untouched(result), "a deleted request was checked");
    Require(sceGameUpdateTerminate() == 0, "the library could not be terminated");
    Require(checkTitle(request, 0, &result) == NotInitialized && Untouched(result), "a check ran after termination");

    std::puts("GameUpdate CheckTitle tests passed");
    return 0;
}

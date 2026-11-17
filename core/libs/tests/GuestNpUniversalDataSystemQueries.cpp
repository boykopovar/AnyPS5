#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

template <typename Call>
static bool RejectsArgument(Call&& call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Stat>
static bool Zeroed(const Stat& stat) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&stat);
    for (std::size_t index = 0; index < sizeof(stat); ++index) {
        if (bytes[index] != 0) return false;
    }
    return true;
}

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80553102u);
    constexpr int invalidParamSize = static_cast<int>(0x80553112u);
    constexpr int alreadyInitialized = static_cast<int>(0x80553118u);
    Require(sceNpUniversalDataSystemInitialize(nullptr) == invalidArgument);
    const NpUniversalDataSystemInitParam init{sizeof(NpUniversalDataSystemInitParam), 0x10000};
    for (const std::size_t size : {std::size_t{0}, std::size_t{8}, std::size_t{0xf}, std::size_t{0x11}, std::size_t{0x18},
                                   std::size_t{0x20}, std::size_t{0x100}, ~std::size_t{0}}) {
        const NpUniversalDataSystemInitParam wrongSize{size, 0x10000};
        Require(sceNpUniversalDataSystemInitialize(&wrongSize) == invalidParamSize);
    }
    const NpUniversalDataSystemInitParam emptyPool{sizeof(NpUniversalDataSystemInitParam), 0};
    Require(sceNpUniversalDataSystemInitialize(&emptyPool) == invalidArgument);
    const NpUniversalDataSystemInitParam wrongSizeEmptyPool{0x18, 0};
    Require(sceNpUniversalDataSystemInitialize(&wrongSizeEmptyPool) == invalidParamSize);
    Require(sceNpUniversalDataSystemInitialize(&init) == 0);
    const NpUniversalDataSystemInitParam wrongSize{0x18, 0x10000};
    Require(sceNpUniversalDataSystemInitialize(&init) == alreadyInitialized);
    Require(sceNpUniversalDataSystemInitialize(&wrongSize) == alreadyInitialized);
    Require(sceNpUniversalDataSystemInitialize(nullptr) == alreadyInitialized);

    NpUniversalDataSystemMemoryStat memory;
    std::memset(&memory, 0x5a, sizeof(memory));
    Require(RejectsArgument([] { sceNpUniversalDataSystemGetMemoryStat(nullptr); }));
    Require(sceNpUniversalDataSystemGetMemoryStat(&memory) == 0 && Zeroed(memory));
    NpUniversalDataSystemStorageStat storage;
    std::memset(&storage, 0x5a, sizeof(storage));
    Require(RejectsArgument([] { sceNpUniversalDataSystemGetStorageStat(1, nullptr); }));
    Require(sceNpUniversalDataSystemGetStorageStat(1, &storage) == 0 && Zeroed(storage));

    NpUniversalDataSystemEvent* event = nullptr;
    Require(sceNpUniversalDataSystemCreateEvent("QueryEvent", nullptr, &event, nullptr) == 0 && event != nullptr);
    std::size_t size = 0;
    Require(sceNpUniversalDataSystemEventEstimateSize(nullptr, &size) == static_cast<int>(0x80553116u));
    Require(sceNpUniversalDataSystemEventEstimateSize(event, nullptr) == static_cast<int>(0x80553102u));
    Require(sceNpUniversalDataSystemEventEstimateSize(event, &size) == 0 && size == 34);

    char text[8];
    std::memset(text, 'x', sizeof(text));
    std::size_t length = 0;
    Require(sceNpUniversalDataSystemEventToString(nullptr, text, sizeof(text), &length) == static_cast<int>(0x80553116u));
    Require(text[0] == 'x' && length == 0);
    Require(sceNpUniversalDataSystemEventToString(event, text, sizeof(text), &length) == 0 && length == 61 && std::strcmp(text, "{\n    \"") == 0);
    length = 0;
    Require(sceNpUniversalDataSystemEventToString(event, nullptr, 0, &length) == 0 && length == 61);
    char tiny[2] = {'x', 'x'};
    Require(sceNpUniversalDataSystemEventToString(event, tiny, sizeof(tiny), nullptr) == 0 && tiny[0] == '{' && tiny[1] == '\0');
    Require(sceNpUniversalDataSystemDestroyEvent(event) == 0);

    int udsHandle = -1;
    Require(sceNpUniversalDataSystemCreateHandle(&udsHandle) == 0 && udsHandle != -1);
    Require(sceNpUniversalDataSystemDestroyHandle(udsHandle) == 0);
    Require(sceNpUniversalDataSystemAbortHandle(udsHandle) == 0);

    Require(sceNpUniversalDataSystemTerminate() == 0);
    Require(sceNpUniversalDataSystemInitialize(&init) == 0);
    Require(sceNpUniversalDataSystemInitialize(&init) == alreadyInitialized);

    std::puts("UDS query tests passed");
    return 0;
}

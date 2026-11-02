#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "UDS query check failed at line %d\n", line);
    std::abort();
}
#define CHECK(value) Require((value), __LINE__)

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
    CHECK(RejectsArgument([] { sceNpUniversalDataSystemInitialize(nullptr); }));
    const NpUniversalDataSystemInitParam init{sizeof(NpUniversalDataSystemInitParam), 0x10000};
    CHECK(sceNpUniversalDataSystemInitialize(&init) == 0);

    NpUniversalDataSystemMemoryStat memory;
    std::memset(&memory, 0x5a, sizeof(memory));
    CHECK(RejectsArgument([] { sceNpUniversalDataSystemGetMemoryStat(nullptr); }));
    CHECK(sceNpUniversalDataSystemGetMemoryStat(&memory) == 0 && Zeroed(memory));
    NpUniversalDataSystemStorageStat storage;
    std::memset(&storage, 0x5a, sizeof(storage));
    CHECK(RejectsArgument([] { sceNpUniversalDataSystemGetStorageStat(1, nullptr); }));
    CHECK(sceNpUniversalDataSystemGetStorageStat(1, &storage) == 0 && Zeroed(storage));

    NpUniversalDataSystemEvent* event = nullptr;
    CHECK(sceNpUniversalDataSystemCreateEvent("QueryEvent", nullptr, &event, nullptr) == 0 && event != nullptr);
    std::size_t size = 0;
    CHECK(sceNpUniversalDataSystemEventEstimateSize(nullptr, &size) == static_cast<int>(0x80553116u));
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, nullptr) == static_cast<int>(0x80553102u));
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, &size) == 0 && size == 34);

    char text[8];
    std::memset(text, 'x', sizeof(text));
    std::size_t length = 0;
    CHECK(sceNpUniversalDataSystemEventToString(nullptr, text, sizeof(text), &length) == static_cast<int>(0x80553116u));
    CHECK(text[0] == 'x' && length == 0);
    CHECK(sceNpUniversalDataSystemEventToString(event, text, sizeof(text), &length) == 0 && length == 61 && std::strcmp(text, "{\n    \"") == 0);
    length = 0;
    CHECK(sceNpUniversalDataSystemEventToString(event, nullptr, 0, &length) == 0 && length == 61);
    char tiny[2] = {'x', 'x'};
    CHECK(sceNpUniversalDataSystemEventToString(event, tiny, sizeof(tiny), nullptr) == 0 && tiny[0] == '{' && tiny[1] == '\0');
    CHECK(sceNpUniversalDataSystemDestroyEvent(event) == 0);
    std::puts("UDS query tests passed");
    return 0;
}

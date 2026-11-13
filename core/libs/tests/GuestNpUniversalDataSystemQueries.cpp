#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
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
    constexpr int invalidArgument = static_cast<int>(0x80553102u);
    constexpr int invalidParamSize = static_cast<int>(0x80553112u);
    constexpr int nullEvent = static_cast<int>(0x80553116u);
    CHECK(sceNpUniversalDataSystemInitialize(nullptr) == invalidArgument);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemInitialize(reinterpret_cast<const NpUniversalDataSystemInitParam*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    const NpUniversalDataSystemInitParam init{sizeof(NpUniversalDataSystemInitParam), 0x10000};
    for (const std::size_t size : {std::size_t{0}, std::size_t{8}, std::size_t{0xf}, std::size_t{0x11}, std::size_t{0x18},
                                   std::size_t{0x20}, std::size_t{0x100}, ~std::size_t{0}}) {
        const NpUniversalDataSystemInitParam wrongSize{size, 0x10000};
        CHECK(sceNpUniversalDataSystemInitialize(&wrongSize) == invalidParamSize);
    }
    const NpUniversalDataSystemInitParam emptyPool{sizeof(NpUniversalDataSystemInitParam), 0};
    CHECK(sceNpUniversalDataSystemInitialize(&emptyPool) == invalidArgument);
    const NpUniversalDataSystemInitParam wrongSizeEmptyPool{0x18, 0};
    CHECK(sceNpUniversalDataSystemInitialize(&wrongSizeEmptyPool) == invalidParamSize);
    CHECK(sceNpUniversalDataSystemInitialize(&init) == 0);

    NpUniversalDataSystemMemoryStat memory;
    std::memset(&memory, 0x5a, sizeof(memory));
    CHECK(RejectsArgument([] { sceNpUniversalDataSystemGetMemoryStat(nullptr); }));
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemGetMemoryStat(reinterpret_cast<NpUniversalDataSystemMemoryStat*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemGetMemoryStat(&memory) == 0 && Zeroed(memory));
    NpUniversalDataSystemStorageStat storage;
    std::memset(&storage, 0x5a, sizeof(storage));
    CHECK(RejectsArgument([] { sceNpUniversalDataSystemGetStorageStat(1, nullptr); }));
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemGetStorageStat(1, reinterpret_cast<NpUniversalDataSystemStorageStat*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemGetStorageStat(1, &storage) == 0 && Zeroed(storage));

    int context = -1;
    CHECK(sceNpUniversalDataSystemCreateContext(&context, 1, 0, 0) == 0 && context != -1);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateContext(reinterpret_cast<int*>(std::uintptr_t{0x10}), 1, 0, 0) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemDestroyContext(context) == 0);

    NpUniversalDataSystemEvent* event = nullptr;
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateEvent(reinterpret_cast<const char*>(std::uintptr_t{0x10}), nullptr, &event, nullptr) ==
          static_cast<int>(0x8055310Du));
    CHECK(event == nullptr && errno == EBUSY);
    CHECK(sceNpUniversalDataSystemCreateEvent("QueryEvent", nullptr, &event, nullptr) == 0 && event != nullptr);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateEvent("QueryEvent", nullptr, reinterpret_cast<NpUniversalDataSystemEvent**>(std::uintptr_t{0x10}), nullptr) == invalidArgument);
    CHECK(errno == EBUSY);
    NpUniversalDataSystemEvent* leaked = nullptr;
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateEvent("QueryEvent", nullptr, &leaked, reinterpret_cast<NpUniversalDataSystemEventPropertyObject**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    std::size_t size = 0;
    CHECK(sceNpUniversalDataSystemEventEstimateSize(nullptr, &size) == nullEvent);
    CHECK(sceNpUniversalDataSystemEventEstimateSize(reinterpret_cast<const NpUniversalDataSystemEvent*>(std::uintptr_t{0x10}), &size) == nullEvent);
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, nullptr) == static_cast<int>(0x80553102u));
    size = 0x1234;
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, reinterpret_cast<std::size_t*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(size == 0x1234 && errno == EBUSY);
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, &size) == 0 && size == 34);

    char text[8];
    std::memset(text, 'x', sizeof(text));
    std::size_t length = 0;
    CHECK(sceNpUniversalDataSystemEventToString(nullptr, text, sizeof(text), &length) == nullEvent);
    CHECK(text[0] == 'x' && length == 0);
    CHECK(sceNpUniversalDataSystemEventToString(reinterpret_cast<const NpUniversalDataSystemEvent*>(std::uintptr_t{0x10}), text, sizeof(text), &length) == nullEvent);
    CHECK(text[0] == 'x' && length == 0);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventToString(event, reinterpret_cast<char*>(std::uintptr_t{0x10}), 4, &length) == invalidArgument);
    CHECK(length == 0 && errno == EBUSY);
    length = 0x1234;
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventToString(event, nullptr, 0, reinterpret_cast<std::size_t*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(length == 0x1234 && errno == EBUSY);
    CHECK(sceNpUniversalDataSystemEventToString(event, text, sizeof(text), &length) == 0 && length == 61 && std::strcmp(text, "{\n    \"") == 0);
    length = 0;
    CHECK(sceNpUniversalDataSystemEventToString(event, nullptr, 0, &length) == 0 && length == 61);
    char tiny[2] = {'x', 'x'};
    CHECK(sceNpUniversalDataSystemEventToString(event, tiny, sizeof(tiny), nullptr) == 0 && tiny[0] == '{' && tiny[1] == '\0');
    CHECK(sceNpUniversalDataSystemDestroyEvent(event) == 0);
    CHECK(sceNpUniversalDataSystemEventEstimateSize(event, &size) == nullEvent);

    int udsHandle = -1;
    CHECK(sceNpUniversalDataSystemCreateHandle(&udsHandle) == 0 && udsHandle != -1);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateHandle(reinterpret_cast<int*>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemDestroyHandle(udsHandle) == 0);
    CHECK(sceNpUniversalDataSystemAbortHandle(udsHandle) == 0);

    NpUniversalDataSystemEventPropertyArray* array = nullptr;
    CHECK(sceNpUniversalDataSystemCreateEventPropertyArray(&array) == 0 && array != nullptr);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateEventPropertyArray(reinterpret_cast<NpUniversalDataSystemEventPropertyArray**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemEventPropertyArraySetInt32(array, 1) == 0);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventPropertyArraySetObject(array, nullptr, reinterpret_cast<NpUniversalDataSystemEventPropertyObject**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY && array->values.size() == 1);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventPropertyArraySetArray(array, nullptr, reinterpret_cast<NpUniversalDataSystemEventPropertyArray**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY && array->values.size() == 1);
    CHECK(sceNpUniversalDataSystemDestroyEventPropertyArray(array) == 0);

    NpUniversalDataSystemEventPropertyObject* object = nullptr;
    CHECK(sceNpUniversalDataSystemCreateEventPropertyObject(&object) == 0 && object != nullptr);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemCreateEventPropertyObject(reinterpret_cast<NpUniversalDataSystemEventPropertyObject**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY);
    CHECK(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "k", 1) == 0);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventPropertyObjectSetObject(object, "child", nullptr, reinterpret_cast<NpUniversalDataSystemEventPropertyObject**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY && object->properties.size() == 1);
    errno = EBUSY;
    CHECK(sceNpUniversalDataSystemEventPropertyObjectSetArray(object, "list", nullptr, reinterpret_cast<NpUniversalDataSystemEventPropertyArray**>(std::uintptr_t{0x10})) == invalidArgument);
    CHECK(errno == EBUSY && object->properties.size() == 1);
    CHECK(sceNpUniversalDataSystemDestroyEventPropertyObject(object) == 0);

    std::puts("UDS query tests passed");
    return 0;
}

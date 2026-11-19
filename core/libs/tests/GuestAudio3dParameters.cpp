#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include "SceTypes.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif


static_assert(sizeof(Audio3dOpenParameters) == 0x28);
static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void CheckDefaults(const void* memory) {
    Audio3dOpenParameters parameters{};
    std::memcpy(&parameters, memory, 0x20);
    Require(parameters.size_this == 0x20, "default parameter size must be 32 bytes");
    Require(parameters.granularity == 256, "incorrect default granularity");
    Require(parameters.rate == 0, "incorrect default rate");
    Require(parameters.max_objects == 512, "incorrect default object limit");
    Require(parameters.queue_depth == 2, "incorrect default queue depth");
    Require(parameters.buffer_mode == 2, "incorrect default buffer mode");
    Require(parameters.pad == 0, "reserved parameter bytes must be cleared");
}

static void CheckInvalidDestination() {
    bool threw = false;
    try {
        sceAudio3dGetDefaultOpenParameters(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null destination must throw invalid_argument");
    threw = false;
    errno = 71;
    try {
        sceAudio3dGetDefaultOpenParameters(reinterpret_cast<Audio3dOpenParameters*>(std::uintptr_t{0x10}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "unwritable destination must throw invalid_argument");
    Require(errno == 71, "default parameter copy failure preserves errno");
}

static void CheckShortBuffer() {
    struct GuardedParameters {
        std::array<unsigned char, 8> before;
        alignas(Audio3dOpenParameters) std::array<unsigned char, 0x20> parameters;
        std::array<unsigned char, 8> after;
    } guarded;
    static_assert(offsetof(GuardedParameters, after) == offsetof(GuardedParameters, parameters) + 0x20);
    std::memset(&guarded, 0xa5, sizeof(guarded));
    for (std::uint64_t size : std::array<std::uint64_t, 6>{0, 0x10, 0x18, 0x20, 0x28, UINT64_MAX}) {
        guarded.parameters.fill(0xa5);
        std::memcpy(guarded.parameters.data(), &size, sizeof(size));
        sceAudio3dGetDefaultOpenParameters(reinterpret_cast<Audio3dOpenParameters*>(guarded.parameters.data()));
        CheckDefaults(guarded.parameters.data());
        for (unsigned char value : guarded.before) Require(value == 0xa5, "parameter underrun");
        for (unsigned char value : guarded.after) Require(value == 0xa5, "parameter overrun");
    }
}

static void CheckExtendedBuffer() {
    Audio3dOpenParameters parameters;
    std::memset(&parameters, 0xa5, sizeof(parameters));
    for (unsigned char pattern : {0xa5, 0x5a}) {
        std::memset(&parameters, pattern, 0x20);
        sceAudio3dGetDefaultOpenParameters(&parameters);
        CheckDefaults(&parameters);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&parameters);
        for (std::size_t i = 0x20; i < sizeof(parameters); ++i) {
            Require(bytes[i] == 0xa5, "extended parameter bytes must remain unchanged");
        }
    }
}

static void CheckErrorStrings() {
    constexpr const char* messages[]{
        "port does not exist", "object does not exist", "invalid parameter",
        "out of memory", "out of resources", "not ready"
    };
    for (std::uint32_t index = 0; index < std::size(messages); ++index) {
        errno = 7;
        const char* result = sceAudio3dStrError(std::bit_cast<std::int32_t>(0x80ea0002u + index));
        Require(result && std::strcmp(result, messages[index]) == 0, "incorrect Audio3d error description");
        Require(errno == 7, "error description lookup must preserve errno");
        Require(std::strcmp(sceAudio3dStrError(0), "(unknown)") == 0 && std::strcmp(result, messages[index]) == 0,
                "error descriptions must remain valid after another lookup");
    }
    for (const auto code : std::array<std::uint32_t, 11>{0, 1, 0xffffffffu, 0x80000000u, 0x7fffffffu,
            0x80e9ffffu, 0x80ea0000u, 0x80ea0001u, 0x80ea0008u, 0x80ea0009u, 0x80eb0002u}) {
        errno = 7;
        const char* result = sceAudio3dStrError(std::bit_cast<std::int32_t>(code));
        Require(result && std::strcmp(result, "(unknown)") == 0 && errno == 7,
                "unrecognized codes must return the native fallback without changing errno");
    }
}

static void CheckAudioOutOutputsBounds() {
    errno = 53;
    Require(sceAudio3dAudioOutOutputs(reinterpret_cast<AudioOutOutputParam*>(UINTPTR_MAX - 7), 2) == static_cast<int>(0x80EA0004),
            "wrapping output param array must fail with invalid parameter");
    Require(errno == 53, "wrapping output param array preserves errno");

    errno = 54;
    Require(sceAudio3dAudioOutOutputs(
                reinterpret_cast<AudioOutOutputParam*>(UINTPTR_MAX - sizeof(AudioOutOutputParam) + 1), 1)
            == static_cast<int>(0x80EA0004),
            "single-entry wrapping output param array must fail with invalid parameter");
    Require(errno == 54, "single-entry wrapping output param array preserves errno");

    alignas(AudioOutOutputParam) std::array<unsigned char, sizeof(AudioOutOutputParam) + 1> bytes{};
    auto* unalignedParam = reinterpret_cast<AudioOutOutputParam*>(bytes.data() + 1);
    std::int16_t sample = 0;
    AudioOutOutputParam entry{};
    entry.handle = -1;
    entry.ptr = &sample;
    std::memcpy(bytes.data() + 1, &entry, sizeof(entry));
    errno = 55;
    Require(sceAudio3dAudioOutOutputs(unalignedParam, 1) == static_cast<int>(0x80EA0002),
            "misaligned non-null output param reaches invalid port error");
    Require(errno == 55, "misaligned non-null output param preserves errno");

    entry.ptr = nullptr;
    std::memcpy(bytes.data() + 1, &entry, sizeof(entry));
    errno = 56;
    Require(sceAudio3dAudioOutOutputs(unalignedParam, 1) == static_cast<int>(0x80EA0004),
            "misaligned null output param reports invalid parameter");
    Require(errno == 56, "misaligned null output param preserves errno");
}

static void CheckSparseBedWrite(std::uint32_t portId) {
    if (sizeof(void*) < 8) return;
    const auto bytes = static_cast<std::size_t>(std::uint64_t{0x40000000} * 8 * 4);
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* base = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS));
    Require(base != nullptr, "reserve sparse bed range succeeds");
    Require(VirtualAlloc(base, page, MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit first sparse bed page succeeds");
    Require(VirtualAlloc(base + bytes - page, page, MEM_COMMIT, PAGE_READWRITE) != nullptr,
            "commit last sparse bed page succeeds");
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0, "page size query succeeds");
    const auto page = static_cast<std::size_t>(pageSize);
    void* mapped = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    Require(mapped != MAP_FAILED, "reserve sparse bed range succeeds");
    auto* base = static_cast<unsigned char*>(mapped);
    Require(mprotect(base, page, PROT_READ | PROT_WRITE) == 0, "map first sparse bed page succeeds");
    Require(mprotect(base + bytes - page, page, PROT_READ | PROT_WRITE) == 0, "map last sparse bed page succeeds");
#endif
    std::memset(base, 0, page);
    std::memset(base + bytes - page, 0, page);

    errno = 64;
    Require(sceAudio3dBedWrite2(portId, 8, 1, base, 0x40000000, 0, false) == static_cast<int>(0x80EA0004),
            "sparse 32 GiB bed write with readable ends must fail with invalid parameter");
    Require(errno == 64, "sparse 32 GiB bed write preserves errno");

#ifdef _WIN32
    Require(VirtualFree(base, 0, MEM_RELEASE) != 0, "release sparse bed range succeeds");
#else
    Require(munmap(base, bytes) == 0, "unmap sparse bed range succeeds");
#endif
}

static void CheckBedWriteBounds() {
    Require(sceAudio3dInitialize(0) == 0, "initialize for bed bounds check succeeds");
    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    parameters.granularity = 0x40000000;
    std::uint32_t portId = 0xFFFFFFFFu;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &portId) == 0 && portId == 0,
            "open large-granularity port succeeds");

    errno = 61;
    Require(sceAudio3dBedWrite2(portId, 8, 1, reinterpret_cast<const void*>(std::uintptr_t{0x1000}), 0x40000000, 0, false)
            == static_cast<int>(0x80EA0004),
            "unmapped 32 GiB bed write must fail with invalid parameter");
    Require(errno == 61, "unmapped 32 GiB bed write preserves errno");

    errno = 62;
    Require(sceAudio3dBedWrite2(portId, 8, 1, reinterpret_cast<const void*>(UINTPTR_MAX - 3), 0x40000000, 0, false)
            == static_cast<int>(0x80EA0004),
            "wrapping 32 GiB bed write must fail with invalid parameter");
    Require(errno == 62, "wrapping 32 GiB bed write preserves errno");

    errno = 63;
    Require(sceAudio3dBedWrite2(portId, 2, 0, reinterpret_cast<const void*>(UINTPTR_MAX - 3), 1, 0, false)
            == static_cast<int>(0x80EA0004),
            "wrapping 4-byte bed write must fail with invalid parameter");
    Require(errno == 63, "wrapping 4-byte bed write preserves errno");

    CheckSparseBedWrite(portId);

    Require(sceAudio3dPortClose(portId) == 0, "close large-granularity port succeeds");

    parameters.granularity = 256;
    portId = 0xFFFFFFFFu;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &portId) == 0 && portId == 0,
            "reopen 256-granularity port succeeds");
    std::array<std::int16_t, 256 * 2> pcm{};
    Require(sceAudio3dBedWrite(portId, 2, 0, pcm.data(), 256) == 0,
            "valid stereo S16 bed write succeeds");
    Require(sceAudio3dPortClose(portId) == 0, "close 256-granularity port succeeds");
    Require(sceAudio3dTerminate() == 0, "terminate after bed bounds check succeeds");
}

int main() {
    CheckErrorStrings();
    CheckInvalidDestination();
    CheckShortBuffer();
    CheckExtendedBuffer();
    CheckAudioOutOutputsBounds();
    Require(sceAudio3dTerminate() == static_cast<int>(0x80EA0007), "terminate before initialize reports not ready");
    Require(sceAudio3dInitialize(1) == static_cast<int>(0x80EA0004), "positive reserved parameter rejected in sceAudio3dInitialize");
    Require(sceAudio3dInitialize(42) == static_cast<int>(0x80EA0004), "positive reserved parameter rejected in sceAudio3dInitialize");
    Require(sceAudio3dInitialize(-1) == static_cast<int>(0x80EA0004), "non-zero reserved parameter rejected in sceAudio3dInitialize");
    Require(sceAudio3dInitialize(0x1000) == static_cast<int>(0x80EA0004), "positive 0x1000 reserved parameter rejected");
    Require(sceAudio3dInitialize(INT64_MAX) == static_cast<int>(0x80EA0004), "INT64_MAX reserved parameter rejected");
    Require(sceAudio3dInitialize(INT64_MIN) == static_cast<int>(0x80EA0004), "INT64_MIN reserved parameter rejected");
    Require(sceAudio3dInitialize(0) == 0, "first initialize succeeds");
    Require(sceAudio3dInitialize(1) == static_cast<int>(0x80EA0004), "reserved parameter rejected while initialized");
    Require(sceAudio3dInitialize(-1) == static_cast<int>(0x80EA0004), "negative reserved parameter rejected while initialized");
    Require(sceAudio3dTerminate() == 0, "terminate while initialized succeeds");
    Require(sceAudio3dTerminate() == static_cast<int>(0x80EA0007), "second terminate reports not ready");
    Require(sceAudio3dInitialize(0) == 0, "re-initialize after terminate succeeds");
    Require(sceAudio3dTerminate() == 0, "second terminate succeeds");
    Require(sceAudio3dTerminate() == static_cast<int>(0x80EA0007), "terminate after second terminate reports not ready");
    CheckBedWriteBounds();
}

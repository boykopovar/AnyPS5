#include "prx/libSceJxr/GuestPixelInfo.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" int JxrReferenceCase_nid_no_patch(std::uint8_t, int, int, std::uint8_t*, std::uint64_t*);
using Query = std::int64_t (APS5_VABI*)(JxrPixelInfo*, std::uint8_t);
static_assert(sizeof(JxrPixelInfo) == 56 && alignof(JxrPixelInfo) == 8);
static_assert(offsetof(JxrPixelInfo, guid) == 0 && offsetof(JxrPixelInfo, channels) == 8);
static_assert(offsetof(JxrPixelInfo, colorFormat) == 16 && offsetof(JxrPixelInfo, bitDepth) == 20);
static_assert(offsetof(JxrPixelInfo, bitsPerUnit) == 24 && offsetof(JxrPixelInfo, flags) == 32);
static_assert(offsetof(JxrPixelInfo, interpretation) == 40 && offsetof(JxrPixelInfo, sampleFormat) == 52);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "JXR lookup: %s\n", message);
    std::exit(1);
}

struct Reference {
    std::array<std::uint8_t, 16> guid{};
    std::array<std::uint64_t, 9> fields{};
};

bool ReadReference(unsigned hash, bool reverse, bool white, Reference& expected) {
    const auto result = JxrReferenceCase_nid_no_patch(static_cast<std::uint8_t>(hash), reverse, white,
        expected.guid.data(), expected.fields.data());
    Require(result >= 0, "native lookup reference failed");
    return result != 0;
}

JxrPixelInfo BackwardInput(const Reference& source) {
    JxrPixelInfo input{};
    input.guid = reinterpret_cast<void*>(std::uintptr_t{1});
    input.channels = UINT64_MAX;
    input.colorFormat = -1;
    input.bitDepth = -1;
    input.bitsPerUnit = UINT32_MAX;
    input.flags = source.fields[4] | UINT64_C(0xfedcba9880000040);
    input.interpretation = static_cast<std::uint32_t>(source.fields[5]);
    input.samplesPerPixel = static_cast<std::uint32_t>(source.fields[6]);
    input.bitsPerSample = static_cast<std::uint32_t>(source.fields[7]);
    input.sampleFormat = static_cast<std::uint32_t>(source.fields[8]);
    return input;
}

void CheckValue(const JxrPixelInfo& actual, const Reference& expected) {
    const std::array<std::uint64_t, 9> fields{{actual.channels, static_cast<std::uint32_t>(actual.colorFormat),
        static_cast<std::uint32_t>(actual.bitDepth), actual.bitsPerUnit, actual.flags, actual.interpretation,
        actual.samplesPerPixel, actual.bitsPerSample, actual.sampleFormat}};
    Require(fields == expected.fields, "metadata differs from pinned native reference");
    Require(actual.guid != nullptr, "successful lookup returned null GUID");
    Require(std::memcmp(actual.guid, expected.guid.data(), 16) == 0, "GUID differs from pinned native reference");
}

class GuardedTail {
    void* memory;
    std::size_t page;
    void* data;
public:
    GuardedTail(const void* source, std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr, "guard allocation");
        DWORD previous;
        Require(VirtualProtect(static_cast<unsigned char*>(memory) + page, page, PAGE_NOACCESS, &previous) != 0, "guard protection");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED, "guard allocation");
        Require(mprotect(static_cast<unsigned char*>(memory) + page, page, PROT_NONE) == 0, "guard protection");
#endif
        Require(bytes <= page, "guard fixture size");
        data = static_cast<unsigned char*>(memory) + page - bytes;
        std::memcpy(data, source, bytes);
    }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0, "read-only protection");
#else
        Require(mprotect(memory, page, PROT_READ) == 0, "read-only protection");
#endif
    }
    ~GuardedTail() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }
    template<class T> T* As() const { return static_cast<T*>(data); }
};

void Cases(Query query) {
    Require(query(nullptr, 2) == -106, "invalid mode has signed 64-bit unsupported-format result");
    Reference forward;
    unsigned formats = 0;
    for (unsigned hash = 0; hash < 256; ++hash) {
        if (!ReadReference(hash, false, false, forward)) continue;
        ++formats;
        JxrPixelInfo input;
        std::memset(&input, 0xa5, sizeof(input));
        input.guid = forward.guid.data();
        const auto originalGuid = forward.guid;
        Require(query(&input, 0) == 0, "forward lookup");
        CheckValue(input, forward);
        Require(forward.guid == originalGuid, "lookup modified input GUID");
        Reference backward;
        Require(ReadReference(hash, true, false, backward), "backward native case missing");
        input = BackwardInput(forward);
        Require(query(&input, 1) == 0, "backward lookup ignores GUID and high flag bits");
        CheckValue(input, backward);
    }
    Require(formats != 0, "reference format inventory empty");
    Reference white;
    Require(ReadReference(5, false, false, forward), "black-white reference");
    Require(ReadReference(5, true, true, white), "white-is-zero reference");
    Require(forward.fields[5] == 1 && white.fields[5] == 0, "duplicate GUID reference order");
    auto input = BackwardInput(forward);
    input.interpretation = 0;
    Require(query(&input, 1) == 0, "backward white-is-zero lookup");
    CheckValue(input, white);
    Require(query(&input, 0) == 0, "forward lookup selects first duplicate GUID");
    CheckValue(input, forward);
    for (unsigned mode = 2; mode < 256; ++mode) {
        std::memset(&input, 0xa5, sizeof(input));
        std::array<std::byte, sizeof(input)> before;
        std::memcpy(before.data(), &input, sizeof(input));
        Require(query(&input, static_cast<std::uint8_t>(mode)) == -106, "invalid mode error");
        Require(std::memcmp(&input, before.data(), sizeof(input)) == 0, "invalid mode changed record");
        Require(query(reinterpret_cast<JxrPixelInfo*>(std::uintptr_t{1}), static_cast<std::uint8_t>(mode)) == -106,
            "invalid mode must not read inaccessible record");
    }
    std::array<std::uint8_t, 16> unknown{};
    std::memset(&input, 0xff, sizeof(input));
    input.guid = unknown.data();
    std::array<std::byte, sizeof(input)> before;
    std::memcpy(before.data(), &input, sizeof(input));
    Require(query(&input, 0) == -106 && std::memcmp(&input, before.data(), sizeof(input)) == 0, "unknown GUID unchanged");
    Require(query(&input, 1) == -106 && std::memcmp(&input, before.data(), sizeof(input)) == 0, "unknown TIFF fields unchanged");
    GuardedTail failedRecord(&input, sizeof(input));
    failedRecord.ReadOnly();
    Require(query(failedRecord.As<JxrPixelInfo>(), 0) == -106, "failed lookup does not write read-only record");
    const void* unknownPointer = unknown.data();
    GuardedTail failedPrefix(&unknownPointer, sizeof(unknownPointer));
    failedPrefix.ReadOnly();
    Require(query(failedPrefix.As<JxrPixelInfo>(), 0) == -106, "failed forward lookup only reads GUID pointer");
    Require(ReadReference(8, false, false, forward), "Gray8 reference");
    Require(forward.fields[0] == 1 && forward.fields[7] == 8, "Gray8 reference values");
    GuardedTail guid(forward.guid.data(), 16);
    guid.ReadOnly();
    input = {};
    input.guid = guid.As<void>();
    GuardedTail record(&input, sizeof(input));
    Require(query(record.As<JxrPixelInfo>(), 0) == 0, "bounded record and read-only GUID");
    CheckValue(*record.As<JxrPixelInfo>(), forward);
    input = BackwardInput(forward);
    input.guid = nullptr;
    Require(query(&input, 1) == 0, "backward lookup ignores null GUID");
    CheckValue(input, forward);
    alignas(16) std::array<std::uint8_t, 17> unalignedGuid{};
    std::memcpy(unalignedGuid.data() + 1, forward.guid.data(), 16);
    input = {};
    input.guid = unalignedGuid.data() + 1;
    Require(query(&input, 0) == 0, "unaligned GUID lookup");
    CheckValue(input, forward);
    const auto borrowedGuid = record.As<JxrPixelInfo>()->guid;
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) thread = std::thread([&] {
        for (unsigned i = 0; i < 32; ++i) {
            JxrPixelInfo value{};
            value.guid = forward.guid.data();
            Require(query(&value, 0) == 0, "concurrent independent lookup");
            CheckValue(value, forward);
            Require(value.guid == borrowedGuid, "borrowed GUID pointer changed across calls");
        }
    });
    for (auto& thread : threads) thread.join();
    for (const bool nullGuid : {false, true}) {
        bool threw = false;
        input = {};
        try { query(nullGuid ? &input : nullptr, 0); } catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "valid-mode null record/GUID is explicitly unsupported");
    }
    bool threw = false;
    try { query(nullptr, 1); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "backward null record is explicitly unsupported");
    std::printf("JXR guest lookup cases passed across %u reference hashes\n", formats);
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    try {
        if (argc == 2 && std::strcmp(argv[1], "reference") == 0) {
            Reference gray;
            Require(ReadReference(8, false, false, gray) && gray.fields[0] == 1 && gray.fields[7] == 8, "native Gray8 reference");
            unsigned formats = 0;
            for (unsigned hash = 0; hash < 256; ++hash) { Reference item; if (ReadReference(hash, false, false, item)) ++formats; }
            std::printf("Pinned native JXR reference passed with %u hashes\n", formats);
            return 0;
        }
        Require(argc == 3, "expected library and export mode");
        const char* name = std::strcmp(argv[2], "patched") == 0 ? "5Gjz1nLOKvo" : "PixelFormatLookup_nid_postfix";
#ifdef _WIN32
        const auto library = LoadLibraryA(argv[1]);
        Require(library != nullptr, "load requested library");
        const auto query = reinterpret_cast<Query>(GetProcAddress(library, name));
#else
        const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        Require(library != nullptr, "load requested library");
        const auto query = reinterpret_cast<Query>(dlsym(library, name));
#endif
        Require(query != nullptr, "missing lookup export");
        Cases(query);
#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "JXR lookup exception: %s\n", error.what());
        return 1;
    }
}

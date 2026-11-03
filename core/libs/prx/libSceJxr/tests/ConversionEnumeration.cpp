#include "prx/libSceJxr/libSceJxr.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" int JxrConversionSource_nid_no_patch(std::uint8_t index, void* bytes);
extern "C" std::int64_t JxrConversionReference_nid_no_patch(const void* source, std::uint32_t index, void* bytes);

namespace {

using Query = decltype(&PKFormatConverter_EnumConversions_nid_postfix);
constexpr JxrGuestGuid DontCare{0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0}};
constexpr JxrGuestGuid Rgb24{0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0d}};
constexpr JxrGuestGuid Bgr24{0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c}};

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR conversion enumeration: %s\n", message);
    std::exit(1);
}

bool Equal(const JxrGuestGuid* first, const JxrGuestGuid& second) {
    return first && std::memcmp(first, &second, sizeof(second)) == 0;
}

std::int64_t Check(Query query, const JxrGuestGuid& source, std::uint32_t index) {
    JxrGuestGuid expected{};
    const auto expectedResult = JxrConversionReference_nid_no_patch(&source, index, &expected);
    const auto original = source;
    struct Output {
        std::uint64_t before = 0x0123456789abcdef;
        const JxrGuestGuid* guid = nullptr;
        std::uint64_t after = 0xfedcba9876543210;
    } output;
    const auto result = query(&source, index, &output.guid);
    Require(result == expectedResult, "signed error or success differs from the reference");
    Require(Equal(output.guid, expected), "target GUID differs from the reference");
    Require(output.before == 0x0123456789abcdef && output.after == 0xfedcba9876543210, "output pointer write exceeded its bounds");
    Require(std::memcmp(&source, &original, sizeof(source)) == 0, "source GUID modified");
    return result;
}

void Cases(Query query) {
    const JxrGuestGuid* first = nullptr;
    Require(query(&Rgb24, 0, &first) == 0 && Equal(first, Bgr24), "first RGB24 conversion is BGR24");
    const auto retained = *first;
    unsigned formats = 0;
    unsigned conversions = 0;
    for (unsigned sourceIndex = 0; sourceIndex < 256; ++sourceIndex) {
        JxrGuestGuid source{};
        if (!JxrConversionSource_nid_no_patch(static_cast<std::uint8_t>(sourceIndex), &source)) break;
        ++formats;
        bool ended = false;
        for (std::uint32_t index = 0; index < 256; ++index) {
            const auto result = Check(query, source, index);
            if (result == -108) { ended = true; break; }
            Require(result == 0, "unexpected enumeration result");
            ++conversions;
        }
        Require(ended, "conversion list did not terminate");
        Require(Check(query, source, std::numeric_limits<std::uint32_t>::max()) == -108, "maximum index must be missing");
    }
    Require(formats == 84 && conversions > 0, "complete GUID reference corpus was not exercised");
    JxrGuestGuid unknown;
    std::memset(&unknown, 0xff, sizeof(unknown));
    Require(Check(query, unknown, 0) == -108 && Check(query, DontCare, 0) == -108, "unknown source has no conversions");
    const JxrGuestGuid* missing = first;
    Require(query(&unknown, 0, &missing) == -108 && Equal(missing, DontCare), "missing index stores DontCare");
    Require(Equal(first, retained), "borrowed GUID invalidated by later calls");
    const JxrGuestGuid* again = nullptr;
    Require(query(&Rgb24, 0, &again) == 0 && again == first, "repeated lookup has stable storage");
    bool threw = false;
    try { query(nullptr, 0, &again); }
    catch (const std::exception&) { threw = true; }
    Require(threw && again == first, "null source throws before changing output");
    threw = false;
    try { query(&Rgb24, 0, nullptr); }
    catch (const std::exception&) { threw = true; }
    Require(threw, "null target throws");
    for (const std::size_t offset : {std::size_t{0}, std::size_t{8}}) {
        alignas(8) std::array<std::byte, 24> storage{};
        std::memcpy(storage.data(), &Rgb24, sizeof(Rgb24));
        const auto original = storage;
        threw = false;
        try {
            query(reinterpret_cast<const JxrGuestGuid*>(storage.data()), 0,
                  reinterpret_cast<const JxrGuestGuid**>(storage.data() + offset));
        } catch (const std::exception&) { threw = true; }
        Require(threw && storage == original, "overlapping arguments throw before mutation");
    }
    std::printf("JXR conversion enumeration: %u formats and %u conversions, bounds, misses and lifetime passed\n", formats, conversions);
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 3, "expected library path and export name");
#ifdef _WIN32
    auto library = LoadLibraryA(argv[1]);
    if (!library) std::fprintf(stderr, "LoadLibrary error %lu\n", GetLastError());
    Require(library != nullptr, "library load");
    auto symbol = GetProcAddress(library, argv[2]);
#else
    auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr, "library load");
    auto symbol = dlsym(library, argv[2]);
#endif
    Require(symbol != nullptr, "enumeration export");
    try { Cases(reinterpret_cast<Query>(symbol)); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}

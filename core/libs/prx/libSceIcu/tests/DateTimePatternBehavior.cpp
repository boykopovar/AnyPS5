#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libSceIcu/tests/DateTimePatternOracle.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using IcuRecoveredTest::Require;

struct Handle {
    UDateTimePatternGenerator* guest;
    void* native;
    explicit Handle(const char* locale) {
        std::int32_t status = 0;
        guest = udatpg_open_nid_postfix(locale, &status);
        std::int32_t expected = 0;
        native = PatternOracleOpen(locale, &expected);
        Require(guest && native && status == expected && status <= 0, "guest generator matches native open and status");
    }
    ~Handle() {
        udatpg_close_nid_postfix(guest);
        PatternOracleClose(native);
    }
};

template<class Function>
void Refuses(Function&& function) {
    bool refused = false;
    try { function(); } catch (const std::exception&) { refused = true; }
    Require(refused, "invalid guest memory or opaque handle is refused");
}

std::int32_t Convert(UDateTimePatternGenerator* handle, std::int32_t operation, const char16_t* input, std::int32_t length,
                     std::int32_t options, std::int32_t field, std::int32_t width,
                     char16_t* output, std::int32_t capacity, std::int32_t* status) {
    if (operation == 0) return udatpg_getBestPattern_nid_postfix(handle, input, length, output, capacity, status);
    if (operation == 1) return udatpg_getBestPatternWithOptions_nid_postfix(handle, input, length,
        static_cast<UDateTimePatternMatchOptions>(options), output, capacity, status);
    if (operation == 2) return udatpg_getSkeleton_nid_postfix(handle, input, length, output, capacity, status);
    return udatpg_getFieldDisplayName_nid_postfix(handle, field, width, output, capacity, status);
}

void Compare(Handle& handle, std::int32_t operation, const char16_t* input, std::int32_t length,
             std::int32_t options, std::int32_t field, std::int32_t width,
             std::int32_t capacity, std::int32_t incoming = 0, bool nullOutput = false) {
    std::array<char16_t, 260> actual;
    actual.fill(0xa55a);
    auto expected = actual;
    auto status = incoming;
    auto nativeStatus = incoming;
    const auto wanted = PatternOracleConvert(handle.native, operation, input, length, options, field, width,
        nullOutput ? nullptr : expected.data() + 2, capacity, &nativeStatus);
    const auto count = Convert(handle.guest, operation, input, length, options, field, width,
        nullOutput ? nullptr : actual.data() + 2, capacity, &status);
    if (count != wanted || status != nativeStatus || actual != expected)
        std::fprintf(stderr, "pattern differential: operation=%d cap=%d incoming=%d result=%d/%d status=%d/%d\n",
            operation, capacity, incoming, count, wanted, status, nativeStatus);
    Require(count == wanted && status == nativeStatus && actual == expected, "native result/status/termination/full guarded output match");
}

void Literal(const char* locale, const char16_t* skeleton, std::u16string_view expected) {
    Handle handle(locale);
    std::array<char16_t, 128> output{};
    std::int32_t status = 0;
    const auto count = udatpg_getBestPattern_nid_postfix(handle.guest, skeleton, -1, output.data(), output.size(), &status);
    Require(status <= 0 && count == static_cast<std::int32_t>(expected.size()) &&
        std::u16string_view(output.data(), count) == expected && output[count] == 0, "fixed locale/calendar pattern literal");
}

void Literals() {
    Literal("en_US", u"yMd", u"M/d/y");
    Literal("de_DE", u"yMd", u"d.M.y");
    Literal("ja_JP", u"yMd", u"y/M/d");
    Literal("ja_JP@calendar=japanese", u"GyMd", u"GGGGGy/M/d");
    for (const auto [locale, expected] : {std::pair{"en", u"year"}, std::pair{"de", u"Jahr"}, std::pair{"ja", u"年"}, std::pair{"fr", u"année"}}) {
        Handle handle(locale);
        std::array<char16_t, 32> output{};
        std::int32_t status = 0;
        const auto count = udatpg_getFieldDisplayName_nid_postfix(handle.guest, 1, 0, output.data(), output.size(), &status);
        Require(status <= 0 && std::u16string_view(output.data(), count) == expected, "fixed non-ASCII field-label literal");
    }
    for (const auto* locale : {"en_US", "de_DE", "ja_JP", "fr_FR", "ar_EG", "th_TH@calendar=buddhist", "en_US@hours=h23", "zz_ZZ"}) {
        Handle handle(locale);
        for (const auto* skeleton : {u"yMd", u"GyMMMd", u"jmm", u"Hms", u"HHHmmmsss", u"EEEEyMMMMd", u"'é' yyyy-MM-dd"})
            for (auto option : {0, 1 << 11, 1 << 12, 1 << 13, 65535, 1 << 20})
                Compare(handle, 1, skeleton, -1, option, 0, 0, 128);
        for (int field = 0; field < 16; ++field)
            for (int width = 0; width < 3; ++width) Compare(handle, 3, nullptr, 0, 0, field, width, 128);
    }
    Handle handle("en_US");
    const std::array<std::pair<std::int32_t, std::u16string_view>, 5> options{{
        {0, u"HH:mm:ss"}, {1 << 11, u"HHH:mm:ss"}, {1 << 12, u"HH:mmm:ss"},
        {1 << 13, u"HH:mm:sss"}, {65535, u"HHH:mmm:sss"}}};
    for (const auto& [option, expected] : options) {
        std::array<char16_t, 32> output{};
        std::int32_t status = 0;
        const auto count = udatpg_getBestPatternWithOptions_nid_postfix(handle.guest, u"HHHmmmsss", 9,
            static_cast<UDateTimePatternMatchOptions>(option), output.data(), output.size(), &status);
        Require(status <= 0 && std::u16string_view(output.data(), count) == expected, "fixed hour/minute/second option-width literal");
    }
}

void Buffers() {
    Handle handle("en_US");
    const std::array<char16_t, 9> unusual{u'y', 0, u'M', u'd', u'\'', 0xd800, 0xd83d, 0xde00, u'\''};
    for (int operation = 0; operation < 4; ++operation) {
        for (const auto* input : {u"yMd", u"yyyy '日' MMM d", u"", unusual.data()}) {
            const auto length = input == unusual.data() ? 9 : -1;
            for (auto capacity : {0, 1, 2, 4, 5, 6, 32})
                for (auto incoming : {0, -1, -124, 7})
                    Compare(handle, operation, input, length, 65535, 1, 0, capacity, incoming);
            Compare(handle, operation, input, length, 0, 1, 0, 0, 0, true);
        }
        for (const auto capacity : {-1, 1, 32}) Compare(handle, operation, u"yMd", -1, 0, 1, 0, capacity, 0, true);
        for (const auto capacity : {-2, -1}) Compare(handle, operation, u"yMd", -1, 0, 1, 0, capacity);
        std::array<char16_t, 64> actual{}, expected{};
        std::copy_n(u"yMd", 4, actual.begin());
        expected = actual;
        std::int32_t status = 0, nativeStatus = 0;
        const auto wanted = PatternOracleConvert(handle.native, operation, expected.data(), 3, 0, 1, 0, expected.data(), 64, &nativeStatus);
        const auto count = Convert(handle.guest, operation, actual.data(), 3, 0, 1, 0, actual.data(), 64, &status);
        Require(count == wanted && status == nativeStatus && actual == expected, "overlapping input/output snapshots preserve native result");
    }
}

void Lifetime() {
    std::array<char16_t, 32> output{};
    for (auto* irrelevant : {static_cast<UDateTimePatternGenerator*>(nullptr), reinterpret_cast<UDateTimePatternGenerator*>(1)}) {
        std::int32_t status = 0;
        const auto count = udatpg_getSkeleton_nid_postfix(irrelevant, u"yyyy '日' MMM d", -1, output.data(), output.size(), &status);
        Require(status <= 0 && std::u16string_view(output.data(), count) == u"yyyyMMMd", "static skeleton ignores null/fake generator handle");
    }
    for (int i = 0; i < 128; ++i) {
        std::int32_t status = 0;
        auto* old = udatpg_open_nid_postfix("en_US", &status);
        Require(old && status <= 0, "lifetime generator opens");
        udatpg_close_nid_postfix(old);
        auto* fresh = udatpg_open_nid_postfix("de_DE", &status);
        Require(fresh && status <= 0 && fresh != old, "closed opaque token is never reused");
        Refuses([&] { udatpg_close_nid_postfix(old); });
        Refuses([&] { status = 0; udatpg_getBestPattern_nid_postfix(old, u"yMd", 3, output.data(), 32, &status); });
        udatpg_close_nid_postfix(fresh);
    }
    Handle handle("en_US");
    std::atomic<bool> entered = false;
    std::atomic<int> successes = 0, refusals = 0, wrong = 0;
    auto* racing = handle.guest;
    std::thread worker([&] {
        entered.store(true);
        for (int i = 0; i < 256; ++i) {
            std::array<char16_t, 32> result{};
            std::int32_t status = 0;
            try {
                const auto count = udatpg_getBestPattern_nid_postfix(racing, u"yMd", 3, result.data(), result.size(), &status);
                if (status <= 0 && count == 5 && std::u16string_view(result.data(), 5) == u"M/d/y") ++successes;
                else ++wrong;
            } catch (const std::runtime_error&) { ++refusals; }
        }
    });
    while (!entered.load()) std::this_thread::yield();
    udatpg_close_nid_postfix(racing);
    handle.guest = nullptr;
    worker.join();
    Require(wrong == 0 && successes + refusals == 256, "concurrent close/op preserves valid results or controlled stale refusal");
    for (int field : {-1, 16, std::numeric_limits<int>::max()})
        Refuses([&] { std::int32_t status = 0; udatpg_getFieldDisplayName_nid_postfix(nullptr, field, 0, output.data(), 32, &status); });
    for (int width : {-1, 3, std::numeric_limits<int>::max()})
        Refuses([&] { std::int32_t status = 0; udatpg_getFieldDisplayName_nid_postfix(nullptr, 1, width, output.data(), 32, &status); });
}

void Memory() {
    constexpr std::size_t page = 0x4000, bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate boundary fixture");
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0, "map boundary fixture");
    auto* data = static_cast<std::uint8_t*>(address);
    std::fill_n(data, bytes, 0xa5);
    Require(sceKernelMprotect(data, page, 0) == 0 && sceKernelMprotect(data + page * 2, page, 0) == 0, "protect boundary fixture guards");
    auto* status = reinterpret_cast<std::int32_t*>(data + page + 64);
    auto* locale = reinterpret_cast<char*>(data + 2 * page - 6);
    auto* inaccessible = reinterpret_cast<char16_t*>(data + 2 * page);
    std::memcpy(locale, "en_US", 6);
    *status = 0;
    auto* handle = udatpg_open_nid_postfix(locale, status);
    Require(handle && *status <= 0, "locale NUL at final mapped byte is not overread");
    std::array<char16_t, 32> result{};
    auto* input = reinterpret_cast<char16_t*>(data + 2 * page - 8);
    std::memcpy(input, u"yMd", 8);
    *status = 0;
    Require(udatpg_getBestPattern_nid_postfix(handle, input, -1, result.data(), result.size(), status) == 5 &&
        *status <= 0 && std::u16string_view(result.data(), 5) == u"M/d/y", "UTF16 NUL at mapped page end is not overread");
    input = reinterpret_cast<char16_t*>(data + 2 * page - 6);
    std::memcpy(input, u"yMd", 6);
    *status = 0;
    Require(udatpg_getBestPattern_nid_postfix(handle, input, 3, result.data(), result.size(), status) == 5 &&
        *status <= 0, "explicit UTF16 input needs no terminator or extra page");
    auto* output = reinterpret_cast<char16_t*>(data + 2 * page - 12);
    output[-1] = 0xa55a;
    *status = 0;
    Require(udatpg_getBestPattern_nid_postfix(handle, u"yMd", 3, output, std::numeric_limits<std::int32_t>::max(), status) == 5 &&
        *status <= 0 && std::u16string_view(output, 5) == u"M/d/y" && output[5] == 0 && output[-1] == 0xa55a,
        "large capacity only allocates and touches actual result plus terminator");
    for (int operation = 0; operation < 4; ++operation) {
        *status = 7;
        Require(Convert(reinterpret_cast<UDateTimePatternGenerator*>(1), operation, inaccessible, -1, 0, -1, -1,
            inaccessible, 16, status) == (operation == 3 ? -1 : 0) && *status == 7, "prefailed status bypasses protected arguments and handle");
        Refuses([&] { Convert(handle, operation, u"yMd", 3, 0, 1, 0, result.data(), 32, nullptr); });
        Refuses([&] { Convert(handle, operation, u"yMd", 3, 0, 1, 0, result.data(), 32, reinterpret_cast<std::int32_t*>(inaccessible)); });
        *status = 0;
        Refuses([&] { Convert(handle, operation, u"yMd", 3, 0, 1, 0, inaccessible, 32, status); });
        if (operation != 3) {
            *status = 0;
            Refuses([&] { Convert(handle, operation, inaccessible, -1, 0, 1, 0, result.data(), 32, status); });
            *status = 0;
            Refuses([&] { Convert(handle, operation, u"yMd", -2, 0, 1, 0, result.data(), 32, status); });
        }
    }
    *status = 7;
    Require(sceKernelMprotect(data + page, page, 1) == 0, "make failed status read-only");
    Require(udatpg_open_nid_postfix(reinterpret_cast<const char*>(inaccessible), status) == nullptr && *status == 7,
        "failed read-only status is not rewritten by open");
    Require(udatpg_getBestPattern_nid_postfix(handle, inaccessible, -1, inaccessible, 16, status) == 0 && *status == 7,
        "failed read-only status is not rewritten by conversion");
    Require(sceKernelMprotect(data + page, page, 3) == 0, "restore status write permission");
    *status = 0;
    Require(sceKernelMprotect(data + page, page, 1) == 0, "make successful status read-only");
    Refuses([&] { udatpg_getBestPattern_nid_postfix(handle, u"yMd", 3, nullptr, 0, status); });
    Require(*status == 0, "refused status write preserves read-only input");
    Require(sceKernelMprotect(data + page, page, 3) == 0, "restore boundary fixture");
    udatpg_close_nid_postfix(handle);
    Refuses([&] { udatpg_close_nid_postfix(reinterpret_cast<UDateTimePatternGenerator*>(inaccessible)); });
    Refuses([&] { udatpg_open_nid_postfix("en_US", nullptr); });
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0, "unmap boundary fixture");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release boundary fixture");
}

void OpenMapped() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate pattern fixture memory");
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0, "map pattern fixture memory");
    auto* data = static_cast<std::uint8_t*>(address);
    std::fill_n(data, bytes, 0xa5);
    Require(sceKernelMprotect(data, page, 0) == 0, "protect pattern fixture first guard");
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0, "protect pattern fixture last guard");
    auto* locale = reinterpret_cast<char*>(data + page + 64);
    auto* status = reinterpret_cast<std::int32_t*>(data + page + 96);
    auto* skeleton = reinterpret_cast<char16_t*>(data + page + 128);
    auto* output = reinterpret_cast<char16_t*>(data + page + 256);
    std::memcpy(locale, "en_US", 6);
    std::memcpy(skeleton, u"yMd", 8);
    *status = 0;
    std::fputs("pattern fixture: valid mapped udatpg_open\n", stderr);
    auto* handle = udatpg_open_nid_postfix(locale, status);
    Require(handle != nullptr && *status <= 0, "valid mapped locale opens a usable pattern generator");
    const auto count = udatpg_getBestPattern_nid_postfix(handle, skeleton, 3, output + 1, 32, status);
    Require(count == 5 && *status <= 0, "mapped yMd pattern has its required length");
    Require(std::u16string_view(output + 1, 5) == u"M/d/y" && output[6] == 0, "mapped en_US yMd pattern and terminator");
    Require(output[0] == 0xa5a5 && output[7] == 0xa5a5 && output[33] == 0xa5a5, "mapped pattern output guards remain intact");
    udatpg_close_nid_postfix(handle);
    udatpg_close_nid_postfix(nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0, "unmap pattern fixture memory");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release pattern fixture memory");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "open";
        if (mode == "open") OpenMapped();
        else if (mode == "literals") Literals();
        else if (mode == "buffers") Buffers();
        else if (mode == "lifetime") Lifetime();
        else if (mode == "memory") Memory();
        else Require(false, "known pattern fixture selector");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pattern fixture failure: %s\n", error.what());
        return 1;
    }
}

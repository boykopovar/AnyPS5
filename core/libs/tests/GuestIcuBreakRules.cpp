#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ubrk.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#undef ubrk_open
#undef ubrk_close
#undef ubrk_first
#undef ubrk_next
#undef ubrk_current
#undef ubrk_getRuleStatus

namespace {

using OpenRules = UBreakIterator* (APS5_VABI*)(const char16_t*, std::int32_t, const char16_t*, std::int32_t, UParseError*, std::int32_t*);
using StatusVector = std::int32_t (APS5_VABI*)(UBreakIterator*, std::int32_t*, std::int32_t, std::int32_t*);
using Locale = const char* (APS5_VABI*)(const UBreakIterator*, std::int32_t, std::int32_t*);
using Clone = UBreakIterator* (APS5_VABI*)(const UBreakIterator*, void*, std::int32_t*, std::int32_t*);

static_assert(std::is_same_v<decltype(&ubrk_openRules_nid_postfix), OpenRules>);
static_assert(std::is_same_v<decltype(&ubrk_getRuleStatusVec_nid_postfix), StatusVector>);
static_assert(std::is_same_v<decltype(&ubrk_getLocaleByType_nid_postfix), Locale>);
static_assert(std::is_same_v<decltype(&ubrk_safeClone_nid_postfix), Clone>);

const auto Rules = &ubrk_openRules_nid_postfix;
const auto Tags = &ubrk_getRuleStatusVec_nid_postfix;
const auto GetLocale = &ubrk_getLocaleByType_nid_postfix;
const auto Copy = &ubrk_safeClone_nid_postfix;

constexpr auto RuleText = u"!!forward; [a-z]+ {100}; [a-z]+ {200}; [0-9]+ {300}; .;";
constexpr auto Input = u"abc12\U0001f642x";

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU break rules: %s\n", message);
    std::abort();
}

struct GuestCloser {
    void operator()(UBreakIterator* iterator) const { ubrk_close(iterator); }
};

using Iterator = std::unique_ptr<UBreakIterator, GuestCloser>;

Iterator Parent() {
    std::int32_t status = 0;
    auto* iterator = static_cast<UBreakIterator*>(ubrk_open(UBRK_WORD, "en_US", u"abc 12", -1, &status));
    Require(iterator && status <= 0, "existing locale iterator opens");
    return Iterator(iterator);
}

void Boundaries(UBreakIterator* iterator) {
    Require(ubrk_first(iterator) == 0, "custom first boundary");
    for (const int boundary : {3, 5, 7, 8, -1}) Require(ubrk_next(iterator) == boundary, "custom BMP and supplementary boundaries");
}

void ControlCase() {
    auto parent = Parent();
    Require(ubrk_first(parent.get()) == 0 && ubrk_next(parent.get()) == 3 && ubrk_next(parent.get()) == 4,
            "existing parent fixture boundaries");
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UBreakIterator, decltype(&ubrk_close_78)> native(
        ubrk_openRules_78(RuleText, -1, Input, -1, nullptr, &status), ubrk_close_78);
    Require(native && U_SUCCESS(status), "native custom rules compile");
    Boundaries(native.get());
    ubrk_first_78(native.get());
    Require(ubrk_next_78(native.get()) == 3, "native tagged match boundary");
    std::array<std::int32_t, 2> tags{};
    Require(ubrk_getRuleStatusVec_78(native.get(), tags.data(), tags.size(), &status) == 2 &&
            tags[0] == 100 && tags[1] == 200 && U_SUCCESS(status), "native overlapping tagged rules");
}

void RulesCase() {
    std::int32_t status = 0;
    UParseError error{};
    Iterator iterator(Rules(RuleText, -1, Input, -1, &error, &status));
    Require(iterator && status <= 0, "compile real custom rules");
    Boundaries(iterator.get());
    status = 0;
    const auto* invalid = u"[broken";
    Iterator rejected(Rules(invalid, -1, nullptr, 0, &error, &status));
    Require(!rejected && status > 0 && error.offset >= 0, "rule syntax exposes parse error");
    UParseError expected;
    std::memset(&expected, 0x55, sizeof(expected));
    error = expected;
    UErrorCode nativeStatus = U_ZERO_ERROR;
    std::unique_ptr<UBreakIterator, decltype(&ubrk_close_78)> native(
        ubrk_openRules_78(invalid, -1, nullptr, 0, &expected, &nativeStatus), ubrk_close_78);
    status = 0;
    rejected.reset(Rules(invalid, -1, nullptr, 0, &error, &status));
    Require(!rejected && status == nativeStatus && std::memcmp(&expected, &error, sizeof(error)) == 0,
            "syntax status and complete parse context match ICU");
    const auto length = static_cast<std::int32_t>(std::char_traits<char16_t>::length(RuleText));
    const auto counted = std::u16string(RuleText) + u"[broken";
    const auto countedInput = std::u16string(Input) + u"extra";
    status = 0;
    Iterator exact(Rules(counted.data(), length, countedInput.data(), 8, nullptr, &status));
    Require(exact && status <= 0, "counted rules ignore trailing invalid syntax");
    Boundaries(exact.get());
    const auto longRules = std::u16string(9000, u' ') + RuleText;
    UErrorCode warning = static_cast<UErrorCode>(-128);
    std::unique_ptr<UBreakIterator, decltype(&ubrk_close_78)> nativeLong(
        ubrk_openRules_78(longRules.data(), static_cast<std::int32_t>(longRules.size()), Input, -1, nullptr, &warning), ubrk_close_78);
    status = -128;
    Iterator longIterator(Rules(longRules.data(), static_cast<std::int32_t>(longRules.size()), Input, -1, nullptr, &status));
    Require(nativeLong && longIterator && status == warning && status == U_ZERO_ERROR,
            "long rules match ICU compiler warning reset");
    Boundaries(longIterator.get());
    status = 0;
    Iterator empty(Rules(RuleText, -1, nullptr, INT32_MIN, nullptr, &status));
    Require(empty && status <= 0 && ubrk_first(empty.get()) == 0 && ubrk_next(empty.get()) == -1,
            "null text ignores its unused length");
}

void TagsCase() {
    auto iterator = Parent();
    Require(ubrk_first(iterator.get()) == 0 && ubrk_next(iterator.get()) == 3, "parent word boundary");
    std::int32_t status = 0;
    std::array<std::int32_t, 4> values{0x1234, 0x1234, 0x1234, 0x1234};
    const auto count = Tags(iterator.get(), values.data() + 1, 2, &status);
    Require(count == 1 && status <= 0 && values[1] == 200, "parent iterator status vector");
    Require(values.front() == 0x1234 && values[2] == 0x1234 && values.back() == 0x1234, "status vector preserves canaries");
    status = 0;
    Iterator custom(Rules(RuleText, -1, Input, -1, nullptr, &status));
    Require(custom && status <= 0 && ubrk_first(custom.get()) == 0 && ubrk_next(custom.get()) == 3, "custom tagged match");
    for (const int capacity : {-1, 0, 1, 2, 3, INT32_MAX}) {
        for (const int incoming : {-128, 0, 1}) {
            std::array<std::int32_t, 5> expected;
            expected.fill(0x1234);
            auto actual = expected;
            UErrorCode nativeStatus = static_cast<UErrorCode>(incoming);
            const auto count = ubrk_getRuleStatusVec_78(custom.get(), expected.data() + 1, capacity, &nativeStatus);
            status = incoming;
            Require(Tags(custom.get(), actual.data() + 1, capacity, &status) == count && status == nativeStatus && actual == expected,
                    "status-vector counts, partial writes and warnings match ICU without allocating caller capacity");
        }
    }
}

void LocaleCase() {
    auto iterator = Parent();
    for (const int type : {int(ULOC_VALID_LOCALE), int(ULOC_ACTUAL_LOCALE)}) {
        UErrorCode expectedStatus = U_ZERO_ERROR;
        const auto* expected = ubrk_getLocaleByType_78(iterator.get(), static_cast<ULocDataLocaleType>(type), &expectedStatus);
        std::int32_t status = 0;
        const auto* actual = GetLocale(iterator.get(), type, &status);
        Require(status == expectedStatus && actual && expected && std::strcmp(actual, expected) == 0, "parent iterator locale matches ICU");
    }
}

void CustomLocaleCase() {
    std::int32_t status = 0;
    Iterator iterator(Rules(RuleText, -1, nullptr, 0, nullptr, &status));
    Require(iterator && status <= 0, "custom rules without text open");
    for (const int type : {ULOC_VALID_LOCALE, ULOC_ACTUAL_LOCALE}) {
        status = 0;
        const auto* locale = GetLocale(iterator.get(), type, &status);
        Require(locale && locale[0] == 0 && status == 0, "ICU68 custom rules have empty locale metadata");
    }
}

void RequestedLocaleCase() {
    auto iterator = Parent();
    for (const int type : {INT32_MIN, -1, 2, 3, INT32_MAX}) {
        std::int32_t status = 0;
        Require(!GetLocale(iterator.get(), type, &status) && status == U_ILLEGAL_ARGUMENT_ERROR,
                "ICU68 refuses requested and invalid locale metadata");
    }
}

void CloneCase() {
    auto iterator = Parent();
    Require(ubrk_first(iterator.get()) == 0 && ubrk_next(iterator.get()) == 3, "position original before cloning");
    std::int32_t status = 0;
    std::int32_t size = 0;
    Require(!Copy(iterator.get(), nullptr, &size, &status) && size == 1 && status == 0, "clone preflight");
    Iterator clone(Copy(iterator.get(), nullptr, nullptr, &status));
    Require(clone && status == U_SAFECLONE_ALLOCATED_WARNING && ubrk_current(clone.get()) == 3, "ICU68 clone warning without deprecated size pointer");
    Require(ubrk_next(iterator.get()) == 4 && ubrk_current(clone.get()) == 3, "clone has independent position");
    iterator.reset();
    Require(ubrk_next(clone.get()) == 4, "clone survives original close");
    for (const int inputSize : {-1, 1, INT32_MAX}) {
        size = inputSize;
        status = -128;
        Iterator sized(Copy(clone.get(), reinterpret_cast<void*>(std::uintptr_t{0x10}), &size, &status));
        Require(sized && size == 1 && status == U_SAFECLONE_ALLOCATED_WARNING,
                "deprecated stack buffer is ignored and allocated clone warning replaces input warning");
    }
    status = 0;
    Iterator custom(Rules(RuleText, -1, Input, -1, nullptr, &status));
    Require(custom && status <= 0, "custom original opens");
    status = 0;
    Iterator customClone(Copy(custom.get(), nullptr, nullptr, &status));
    Require(customClone && status == U_SAFECLONE_ALLOCATED_WARNING, "custom clone opens");
    custom.reset();
    Boundaries(customClone.get());
    status = 0;
    const auto* locale = GetLocale(customClone.get(), ULOC_ACTUAL_LOCALE, &status);
    Require(locale && locale[0] == 0 && status == 0, "custom clone retains ICU68 locale identity");
}

struct Guarded {
    char* memory;
    std::size_t page;

    Guarded() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "allocate guarded pages");
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0, "protect guard page");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = static_cast<char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED, "allocate guarded pages");
        Require(mprotect(memory + page, page, PROT_NONE) == 0, "protect guard page");
#endif
    }

    ~Guarded() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }

    void* Forbidden() const { return memory + page; }
};

template<class Call>
void Unreadable(Call call) {
    errno = 73;
    try {
        call();
    } catch (const std::runtime_error&) {
        Require(errno == 73, "checked guest memory failure preserves errno");
        return;
    }
    Require(false, "unusable guest memory is refused");
}

void GuardsCase() {
    Guarded pages;
    const auto length = std::char_traits<char16_t>::length(RuleText);
    auto* rules = reinterpret_cast<char16_t*>(pages.memory + pages.page) - length;
    std::memcpy(rules, RuleText, length * sizeof(char16_t));
    std::int32_t status = 0;
    Iterator exact(Rules(rules, static_cast<std::int32_t>(length), Input, -1, nullptr, &status));
    Require(exact && status <= 0, "counted rules ending exactly at guard page");
    Boundaries(exact.get());
    const auto* forbiddenText = static_cast<const char16_t*>(pages.Forbidden());
    auto* forbiddenStatus = static_cast<std::int32_t*>(pages.Forbidden());
    auto* forbiddenIterator = static_cast<UBreakIterator*>(pages.Forbidden());
    auto* forbiddenParse = static_cast<UParseError*>(pages.Forbidden());
    auto* forbiddenValues = static_cast<std::int32_t*>(pages.Forbidden());
    status = 1;
    Require(!Rules(forbiddenText, -1, forbiddenText, -1, forbiddenParse, &status) && status == 1,
            "failed rule status does not read unrelated pointers");
    Require(Tags(forbiddenIterator, forbiddenValues, INT32_MAX, &status) == 0 && status == 1,
            "failed vector status does not read unrelated pointers");
    Require(!GetLocale(forbiddenIterator, INT32_MAX, &status) && status == 1,
            "failed locale status does not read unrelated pointers");
    Require(!Copy(forbiddenIterator, pages.Forbidden(), forbiddenStatus, &status) && status == 1,
            "failed clone status does not read unrelated pointers");
    Require(!Rules(forbiddenText, -1, nullptr, 0, forbiddenParse, nullptr), "null rule status returns before pointer access");
    Require(!Copy(forbiddenIterator, pages.Forbidden(), forbiddenStatus, nullptr), "null clone status returns before pointer access");
    status = 0;
    Unreadable([&] { Rules(forbiddenText, -1, nullptr, 0, nullptr, &status); });
    Unreadable([&] { Rules(RuleText, -1, forbiddenText, 1, nullptr, &status); });
    Unreadable([&] { Rules(rules, static_cast<std::int32_t>(length + 1), nullptr, 0, nullptr, &status); });
    Unreadable([&] { Rules(RuleText, -1, nullptr, 0, forbiddenParse, &status); });
    Unreadable([&] { Rules(RuleText, -1, nullptr, 0, nullptr, forbiddenStatus); });
    Unreadable([&] { Tags(exact.get(), forbiddenValues, 1, &status); });
    Unreadable([&] { GetLocale(exact.get(), 0, forbiddenStatus); });
    Unreadable([&] { Copy(exact.get(), nullptr, forbiddenStatus, &status); });
    Require(ubrk_first(exact.get()) == 0 && ubrk_next(exact.get()) == 3, "position partial guarded vector");
    auto* one = reinterpret_cast<std::int32_t*>(pages.memory + pages.page) - 1;
    *one = 0;
    status = 0;
    Require(Tags(exact.get(), one, 1, &status) == 2 && status == U_BUFFER_OVERFLOW_ERROR && *one == 100,
            "partial status vector stops before inaccessible memory");
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "rules";
        if (mode == "control") ControlCase();
        else if (mode == "rules") RulesCase();
        else if (mode == "tags") TagsCase();
        else if (mode == "locale") LocaleCase();
        else if (mode == "locale-custom") CustomLocaleCase();
        else if (mode == "locale-requested") RequestedLocaleCase();
        else if (mode == "clone") CloneCase();
        else if (mode == "guards") GuardsCase();
        else return 2;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ICU break rules runtime failure: %s\n", error.what());
        return 1;
    }
}

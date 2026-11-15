#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstddef>
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

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidState = 27;
constexpr std::uint8_t DefaultLtr = 0xfe;
constexpr std::int32_t Ltr = 0;
constexpr std::int32_t Rtl = 1;
constexpr std::int32_t Neutral = 3;
constexpr std::int32_t LeftToRight = 0;
constexpr std::int32_t RightToLeft = 1;
constexpr std::int32_t EuropeanNumber = 2;
constexpr std::int32_t RightToLeftArabic = 13;
constexpr std::int32_t ClassDefault = 23;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Expect(false, what);
}

void ExpectVisualRun(Icu::BiDi* bidi, std::int32_t run, std::int32_t start, std::int32_t length, std::int32_t direction,
                     const char* what) {
    std::int32_t actualStart = -1;
    std::int32_t actualLength = -1;
    ExpectInt(ubidi_getVisualRun_nid_postfix(bidi, run, &actualStart, &actualLength), direction, what);
    ExpectInt(actualStart, start, what);
    ExpectInt(actualLength, length, what);
}

std::int32_t APS5_VABI MarkXRightToLeft(const void* context, std::int32_t character) {
    ++*static_cast<std::int32_t*>(const_cast<void*>(context));
    return character == u'x' ? RightToLeft : ClassDefault;
}

std::int32_t APS5_VABI DefaultClass(const void*, std::int32_t) {
    return ClassDefault;
}

void TestParagraph() {
    auto* bidi = ubidi_open_nid_postfix();
    Expect(bidi != nullptr, "ubidi_open");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 0, "ubidi_getParaLevel before setPara");
    std::int32_t error = Success;
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), -1, "ubidi_countRuns before setPara");
    ExpectInt(error, InvalidState, "ubidi_countRuns before setPara status");

    const char16_t text[] = u"abc אבג def";
    error = Success;
    ubidi_setPara_nid_postfix(bidi, text, -1, DefaultLtr, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 0, "ubidi_getParaLevel");
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 3, "ubidi_countRuns");
    ExpectVisualRun(bidi, 0, 0, 4, Ltr, "ubidi_getVisualRun(0)");
    ExpectVisualRun(bidi, 1, 4, 3, Rtl, "ubidi_getVisualRun(1)");
    ExpectVisualRun(bidi, 2, 7, 4, Ltr, "ubidi_getVisualRun(2)");
    ExpectInt(ubidi_getVisualRun_nid_postfix(bidi, 3, nullptr, nullptr), Ltr, "ubidi_getVisualRun out of range");

    std::int32_t limit = -1;
    std::uint8_t level = 0xff;
    ubidi_getLogicalRun_nid_postfix(bidi, 5, &limit, &level);
    ExpectInt(limit, 7, "ubidi_getLogicalRun limit");
    ExpectInt(level, 1, "ubidi_getLogicalRun level");
    ubidi_getLogicalRun_nid_postfix(bidi, 0, &limit, &level);
    ExpectInt(limit, 4, "ubidi_getLogicalRun(0) limit");
    ExpectInt(level, 0, "ubidi_getLogicalRun(0) level");

    const char16_t hebrew[] = u"אב abc";
    ubidi_setPara_nid_postfix(bidi, hebrew, -1, DefaultLtr, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara(RTL first)");
    ExpectInt(ubidi_getParaLevel_nid_postfix(bidi), 1, "ubidi_getParaLevel(RTL first)");
    ubidi_close_nid_postfix(bidi);
}

void TestClassCallback() {
    auto* bidi = ubidi_open_nid_postfix();
    std::int32_t calls = 0;
    Icu::BiDiClassCallback oldFunction = DefaultClass;
    const void* oldContext = &oldFunction;
    std::int32_t error = Success;
    ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, &oldFunction, &oldContext, &error);
    ExpectInt(error, Success, "ubidi_setClassCallback");
    Expect(oldFunction == nullptr && oldContext == nullptr, "ubidi_setClassCallback initial callback");

    const char16_t text[] = u"axb";
    ubidi_setPara_nid_postfix(bidi, text, -1, 0, nullptr, &error);
    ExpectInt(error, Success, "ubidi_setPara with callback");
    Expect(calls >= 3, "ubidi_setPara called the class callback");
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 3, "ubidi_countRuns with callback");
    ExpectVisualRun(bidi, 1, 1, 1, Rtl, "ubidi_getVisualRun with callback");

    ubidi_setClassCallback_nid_postfix(bidi, nullptr, nullptr, &oldFunction, &oldContext, &error);
    Expect(oldFunction == MarkXRightToLeft && oldContext == &calls, "ubidi_setClassCallback previous callback");
    ubidi_setPara_nid_postfix(bidi, text, -1, 0, nullptr, &error);
    ExpectInt(ubidi_countRuns_nid_postfix(bidi, &error), 1, "ubidi_countRuns after removing callback");

    error = Success;
    ubidi_setClassCallback_nid_postfix(nullptr, MarkXRightToLeft, &calls, nullptr, nullptr, &error);
    ExpectInt(error, IllegalArgument, "ubidi_setClassCallback(null)");
    ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, nullptr, nullptr, nullptr);
    ubidi_close_nid_postfix(bidi);
}

void TestStatelessHelpers() {
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"123 אabc", -1), Rtl, "ubidi_getBaseDirection(Hebrew)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"abc", -1), Ltr, "ubidi_getBaseDirection(Latin)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"123 !", -1), Neutral, "ubidi_getBaseDirection(neutral)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(u"א", 0), Neutral, "ubidi_getBaseDirection(empty)");
    ExpectInt(ubidi_getBaseDirection_nid_postfix(nullptr, -1), Neutral, "ubidi_getBaseDirection(null)");

    const std::uint8_t levels[] = {0, 0, 1, 1, 1, 0};
    std::int32_t map[6] = {};
    ubidi_reorderVisual_nid_postfix(levels, 6, map);
    const std::int32_t expected[] = {0, 1, 4, 3, 2, 5};
    for (int i = 0; i < 6; ++i) ExpectInt(map[i], expected[i], "ubidi_reorderVisual");

    ExpectInt(ubidi_getClass_nid_postfix(u'A'), LeftToRight, "ubidi_getClass(A)");
    ExpectInt(ubidi_getClass_nid_postfix(0x05D0), RightToLeft, "ubidi_getClass(U+05D0)");
    ExpectInt(ubidi_getClass_nid_postfix(u'7'), EuropeanNumber, "ubidi_getClass(7)");
    ExpectInt(ubidi_getClass_nid_postfix(0x0627), RightToLeftArabic, "ubidi_getClass(U+0627)");
}

void TestArguments() {
    std::int32_t error = Success;
    ubidi_setPara_nid_postfix(nullptr, u"a", -1, 0, nullptr, &error);
    ExpectInt(error, IllegalArgument, "ubidi_setPara(null)");
    ExpectThrow<std::invalid_argument>([] { ubidi_getLogicalRun_nid_postfix(nullptr, 0, nullptr, nullptr); },
                                       "ubidi_getLogicalRun(null)");
    ubidi_close_nid_postfix(nullptr);
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(data != nullptr, "VirtualAlloc");
#else
        const long page = sysconf(_SC_PAGESIZE);
        Expect(page > 0, "sysconf");
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Expect(data != nullptr && data != MAP_FAILED, "mmap");
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Expect(sizeof(value) <= bytes, "ReadOnlyPage size");
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Expect(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0, "VirtualProtect");
#else
        Expect(mprotect(data, bytes, PROT_READ) == 0, "mprotect");
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
void RejectsFaulted(TCall call, const char* what) {
    errno = EINVAL;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Expect(threw, what);
    ExpectInt(errno, EINVAL, "errno preserved");
}

void TestGuestPointers() {
    auto* badInt = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    auto* badByte = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    auto* badCallback = reinterpret_cast<Icu::BiDiClassCallback*>(std::uintptr_t{0x10});

    RejectsFaulted([&] { ubidi_setPara_nid_postfix(nullptr, u"a", -1, 0, nullptr, badInt); },
                   "ubidi_setPara bad error");
    RejectsFaulted([&] { ubidi_countRuns_nid_postfix(nullptr, badInt); },
                   "ubidi_countRuns bad error");
    RejectsFaulted([&] { ubidi_setClassCallback_nid_postfix(nullptr, MarkXRightToLeft, nullptr, nullptr, nullptr, badInt); },
                   "ubidi_setClassCallback bad error");

    {
        ReadOnlyPage page;
        auto* roStatus = page.Set<std::int32_t>(IllegalArgument);
        page.Protect();
        errno = EINVAL;
        ubidi_setPara_nid_postfix(nullptr, u"a", -1, 0, nullptr, roStatus);
        ExpectInt(*roStatus, IllegalArgument, "ro failure status unchanged by setPara");
        ExpectInt(errno, EINVAL, "errno preserved");
        ExpectInt(ubidi_countRuns_nid_postfix(nullptr, roStatus), -1, "countRuns short-circuits on ro failure");
        ExpectInt(*roStatus, IllegalArgument, "ro failure status unchanged by countRuns");
        ExpectInt(errno, EINVAL, "errno preserved");
        ubidi_setClassCallback_nid_postfix(nullptr, MarkXRightToLeft, nullptr, nullptr, nullptr, roStatus);
        ExpectInt(*roStatus, IllegalArgument, "ro failure status unchanged by setClassCallback");
        ExpectInt(errno, EINVAL, "errno preserved");
    }

    {
        ReadOnlyPage page;
        auto* roSuccess = page.Set<std::int32_t>(Success);
        page.Protect();
        RejectsFaulted([&] { ubidi_setPara_nid_postfix(nullptr, u"a", -1, 0, nullptr, roSuccess); },
                       "ubidi_setPara ro error");
        ExpectInt(*roSuccess, Success, "ro error unchanged after setPara");
        RejectsFaulted([&] { ubidi_countRuns_nid_postfix(nullptr, roSuccess); },
                       "ubidi_countRuns ro error");
        ExpectInt(*roSuccess, Success, "ro error unchanged after countRuns");
        RejectsFaulted([&] { ubidi_setClassCallback_nid_postfix(nullptr, MarkXRightToLeft, nullptr, nullptr, nullptr, roSuccess); },
                       "ubidi_setClassCallback ro error");
        ExpectInt(*roSuccess, Success, "ro error unchanged after setClassCallback");
    }

    auto* bidi = ubidi_open_nid_postfix();
    Expect(bidi != nullptr, "ubidi_open");
    std::int32_t status = Success;
    const char16_t text[] = u"abc אב";
    ubidi_setPara_nid_postfix(bidi, text, -1, DefaultLtr, nullptr, &status);
    ExpectInt(status, Success, "ubidi_setPara setup");

    {
        ReadOnlyPage page;
        auto* roInt = page.Set<std::int32_t>(77);
        page.Protect();
        std::int32_t localStart = -5;
        std::int32_t localLength = -6;
        RejectsFaulted([&] { ubidi_getVisualRun_nid_postfix(bidi, 0, roInt, &localLength); },
                       "ubidi_getVisualRun ro start");
        ExpectInt(*roInt, 77, "ro start unchanged");
        ExpectInt(localLength, -6, "length untouched when start unwritable");
        RejectsFaulted([&] { ubidi_getVisualRun_nid_postfix(bidi, 0, &localStart, roInt); },
                       "ubidi_getVisualRun ro length");
        ExpectInt(*roInt, 77, "ro length unchanged");
        ExpectInt(localStart, -5, "start untouched when length unwritable");
        RejectsFaulted([&] { ubidi_getVisualRun_nid_postfix(bidi, 0, badInt, &localLength); },
                       "ubidi_getVisualRun bad start");
        ExpectInt(localLength, -6, "length untouched on bad start");
    }

    {
        ReadOnlyPage intPage;
        auto* roLimit = intPage.Set<std::int32_t>(88);
        intPage.Protect();
        ReadOnlyPage bytePage;
        auto* roLevel = bytePage.Set<std::uint8_t>(99);
        bytePage.Protect();
        std::int32_t localLimit = -7;
        std::uint8_t localLevel = 250;
        RejectsFaulted([&] { ubidi_getLogicalRun_nid_postfix(bidi, 0, roLimit, &localLevel); },
                       "ubidi_getLogicalRun ro limit");
        ExpectInt(*roLimit, 88, "ro limit unchanged");
        ExpectInt(localLevel, 250, "level untouched when limit unwritable");
        RejectsFaulted([&] { ubidi_getLogicalRun_nid_postfix(bidi, 0, &localLimit, roLevel); },
                       "ubidi_getLogicalRun ro level");
        ExpectInt(*roLevel, 99, "ro level unchanged");
        ExpectInt(localLimit, -7, "limit untouched when level unwritable");
        RejectsFaulted([&] { ubidi_getLogicalRun_nid_postfix(bidi, 0, &localLimit, badByte); },
                       "ubidi_getLogicalRun bad level");
        ExpectInt(localLimit, -7, "limit untouched on bad level");
    }

    {
        const std::uint8_t levels[] = {0, 1};
        std::int32_t map[2] = {11, 22};
        RejectsFaulted([&] { ubidi_reorderVisual_nid_postfix(badByte, 2, map); },
                       "ubidi_reorderVisual bad levels");
        Expect(map[0] == 11 && map[1] == 22, "map untouched on bad levels");
        RejectsFaulted([&] { ubidi_reorderVisual_nid_postfix(levels, 2, badInt); },
                       "ubidi_reorderVisual bad map");
        ReadOnlyPage page;
        auto* roMap = page.Set<std::int32_t>(55);
        page.Protect();
        RejectsFaulted([&] { ubidi_reorderVisual_nid_postfix(levels, 1, roMap); },
                       "ubidi_reorderVisual ro map");
        ExpectInt(*roMap, 55, "ro map unchanged");
    }

    {
        std::int32_t calls = 0;
        status = Success;
        Icu::BiDiClassCallback localOldFunction = DefaultClass;
        const void* localOldContext = &status;
        RejectsFaulted([&] {
            ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, badCallback, &localOldContext, &status);
        }, "ubidi_setClassCallback bad oldFunction");
        Expect(localOldContext == &status, "oldContext untouched when oldFunction invalid");
        ExpectInt(status, Success, "status unchanged when oldFunction invalid");

        ReadOnlyPage contextPage;
        const void* sentinel = &calls;
        auto* roOldContext = contextPage.Set<const void*>(sentinel);
        contextPage.Protect();
        RejectsFaulted([&] {
            ubidi_setClassCallback_nid_postfix(bidi, MarkXRightToLeft, &calls, &localOldFunction, roOldContext, &status);
        }, "ubidi_setClassCallback ro oldContext");
        Expect(localOldFunction == DefaultClass && *roOldContext == sentinel,
               "oldFunction and ro oldContext untouched on failure");

        status = Success;
        const char16_t checkText[] = u"axb";
        ubidi_setPara_nid_postfix(bidi, checkText, -1, 0, nullptr, &status);
        ExpectInt(status, Success, "ubidi_setPara after failed setClassCallback");
        ExpectInt(calls, 0, "failed setClassCallback did not install callback");
        ExpectInt(ubidi_countRuns_nid_postfix(bidi, &status), 1, "original callback state preserved");
    }

    ubidi_close_nid_postfix(bidi);
}

}

int main() {
    TestParagraph();
    TestClassCallback();
    TestStatelessHelpers();
    TestArguments();
    TestGuestPointers();
    return 0;
}

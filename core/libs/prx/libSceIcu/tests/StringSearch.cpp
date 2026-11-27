#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace UsearchOpen {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireNoThrow;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;

static_assert(std::is_same_v<decltype(&usearch_open_nid_postfix), UStringSearch* (APS5_VABI*)(const char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, UBreakIterator*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED);
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
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TAction>
void Rejects(TAction action) {
    errno = EINVAL;
    bool threw = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(errno == EINVAL);
}

void GuestPointers(const char16_t* pattern, const char16_t* text) {
    auto* badStatus = reinterpret_cast<std::int32_t*>(0x10);
    Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", nullptr, badStatus); });
    Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, nullptr, badStatus); });

    {
        ReadOnlyPage page;
        auto* roFailing = page.Set<std::int32_t>(IllegalArgument);
        page.Protect();
        errno = EINVAL;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, nullptr, roFailing) == nullptr);
        Require(errno == EINVAL);
        Require(*roFailing == IllegalArgument);
    }

    {
        ReadOnlyPage page;
        auto* roPassing = page.Set<std::int32_t>(0);
        page.Protect();
        Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, nullptr, roPassing); });
        Require(*roPassing == 0);
    }

    std::int32_t status = 0;
    Rejects([&] { usearch_open_nid_postfix(reinterpret_cast<const char16_t*>(0x10), -1, text, 8, "en", nullptr, &status); });
    Require(status == 0);
    Rejects([&] {
        usearch_open_nid_postfix(reinterpret_cast<const char16_t*>(0x10), std::numeric_limits<std::int32_t>::max(), text, 8,
                                 "en", nullptr, &status);
    });
    Require(status == 0);
    Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, reinterpret_cast<const char*>(0x10), nullptr, &status); });
    Require(status == 0);
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto pattern = reinterpret_cast<const char16_t*>(storage.data());
    const auto text = reinterpret_cast<const char16_t*>(storage.data());

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, "en", nullptr, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(usearch_open_nid_postfix(nullptr, -1, nullptr, -1, nullptr, nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        auto* search = usearch_open_nid_postfix(u"ana", -1, u"bananas", -1, "en", nullptr, &status);
        Require(search != nullptr);
        Require(status <= 0);

        status = incoming;
        Require(usearch_open_nid_postfix(nullptr, 0, nullptr, 0, "de", nullptr, &status) == nullptr);
        Require(status == IllegalArgument);

        status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, nullptr, &status) == nullptr);
        Require(status == IllegalArgument);
    }

    std::int32_t status = 0;
    Require(usearch_open_nid_postfix(u"ana", 3, u"bananas", 7, "root", nullptr, &status) != nullptr);
    Require(status <= 0);
    for (const auto length : {0, -2}) {
        status = 0;
        Require(usearch_open_nid_postfix(u"ana", length, u"bananas", 7, "en", nullptr, &status) == nullptr);
        Require(status == IllegalArgument);
        status = 0;
        Require(usearch_open_nid_postfix(u"ana", 3, u"bananas", length, "en", nullptr, &status) == nullptr);
        Require(status == IllegalArgument);
    }

    status = 0;
    Require(usearch_open_nid_postfix(reinterpret_cast<const char16_t*>(0x10), 3, text, 0, "en", nullptr, &status) == nullptr);
    Require(status == IllegalArgument);
    status = 0;
    Require(usearch_open_nid_postfix(pattern, 0, reinterpret_cast<const char16_t*>(0x10), 3, "en", nullptr, &status) == nullptr);
    Require(status == IllegalArgument);

    Require(Unchanged(storage));
    RequireThrows([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", nullptr, nullptr); });
    GuestPointers(pattern, text);

    status = 0;
    auto* iterator = ubrk_open(0, "en", u"old", 3, &status);
    Require(iterator != nullptr && status <= 0);
    auto* restricted = usearch_open_nid_postfix(u"ana", 3, u"bananas", 7, "en", static_cast<UBreakIterator*>(iterator), &status);
    Require(restricted != nullptr && status <= 0);
    Require(ubrk_last(iterator) == 3);
    ubrk_close(iterator);
    status = 0;
    auto* fallbackIterator = ubrk_open(0, "zz_ZZ", u"bananas", 7, &status);
    Require(fallbackIterator != nullptr && status < 0);
    status = 0;
    auto* fallbackSearch = usearch_open_nid_postfix(
        u"ana", 3, u"bananas", 7, "root", static_cast<UBreakIterator*>(fallbackIterator), &status);
    Require(fallbackSearch != nullptr && status == 0);
    ubrk_close(fallbackIterator);
    u_cleanup_nid_postfix();
}

} // namespace UsearchOpen

namespace UsearchObject {

using IcuRecoveredTest::Filler;
using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;
using IcuRecoveredTest::Unchanged;
using UsearchOpen::ReadOnlyPage;
using UsearchOpen::Rejects;

static_assert(std::is_same_v<decltype(&usearch_getCollator_nid_postfix), UCollator* (APS5_VABI*)(const UStringSearch*)>);
static_assert(std::is_same_v<decltype(&usearch_getMatchedLength_nid_postfix), std::int32_t (APS5_VABI*)(const UStringSearch*)>);
static_assert(std::is_same_v<decltype(&usearch_next_nid_postfix), std::int32_t (APS5_VABI*)(UStringSearch*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&usearch_reset_nid_postfix), void (APS5_VABI*)(UStringSearch*)>);
static_assert(std::is_same_v<decltype(&usearch_setAttribute_nid_postfix), void (APS5_VABI*)(UStringSearch*, std::int32_t, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&usearch_setOffset_nid_postfix), void (APS5_VABI*)(UStringSearch*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&usearch_setPattern_nid_postfix), void (APS5_VABI*)(UStringSearch*, const char16_t*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&usearch_setText_nid_postfix), void (APS5_VABI*)(UStringSearch*, const char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t Done = -1;
constexpr std::int32_t AttributeValueCount = 5;
constexpr std::array<std::int32_t, 7> Failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
constexpr std::array<std::int32_t, 4> Passing{0, -128, -124, -1};
constexpr char16_t Empty[] = u"";

void WithoutStatus(UStringSearch* search) {
    Require(usearch_getCollator_nid_postfix(nullptr) == nullptr);
    Require(usearch_getMatchedLength_nid_postfix(nullptr) == Done);
    usearch_reset_nid_postfix(nullptr);
    RequireThrows([&] { usearch_getCollator_nid_postfix(search); });
    RequireThrows([&] { usearch_getMatchedLength_nid_postfix(search); });
    RequireThrows([&] { usearch_reset_nid_postfix(search); });
}

void FailingStatus(UStringSearch* search, const char16_t* words) {
    for (const auto incoming : Failing) {
        for (auto* target : {search, static_cast<UStringSearch*>(nullptr)}) {
            std::int32_t status = incoming;
            Require(usearch_next_nid_postfix(target, &status) == Done);
            usearch_setOffset_nid_postfix(target, 3, &status);
            usearch_setAttribute_nid_postfix(target, 0, 1, &status);
            usearch_setAttribute_nid_postfix(target, 7, 1, &status);
            usearch_setPattern_nid_postfix(target, words, 4, &status);
            usearch_setPattern_nid_postfix(target, nullptr, 0, &status);
            usearch_setText_nid_postfix(target, words, 8, &status);
            usearch_setText_nid_postfix(target, nullptr, 0, &status);
            Require(status == incoming);
            usearch_setAttribute_nid_postfix(target, 0, AttributeValueCount, &status);
            Require(status == IllegalArgument);
        }
    }
}

void PassingStatus(UStringSearch* search, const char16_t* words) {
    for (const auto incoming : Passing) {
        std::int32_t status = incoming;
        Require(usearch_next_nid_postfix(nullptr, &status) == Done);
        usearch_setOffset_nid_postfix(nullptr, 3, &status);
        RequireThrows([&] { usearch_next_nid_postfix(search, &status); });
        RequireThrows([&] { usearch_setOffset_nid_postfix(search, 3, &status); });
        for (const auto attribute : {0, 1, 2}) {
            usearch_setAttribute_nid_postfix(nullptr, attribute, 1, &status);
            RequireThrows([&] { usearch_setAttribute_nid_postfix(search, attribute, 1, &status); });
            RequireThrows([&] { usearch_setAttribute_nid_postfix(search, attribute, AttributeValueCount, &status); });
        }
        for (const auto length : {-1, 4, -2}) {
            RequireThrows([&] { usearch_setPattern_nid_postfix(search, words, length, &status); });
        }
        for (const auto length : {-1, 8}) {
            RequireThrows([&] { usearch_setText_nid_postfix(search, words, length, &status); });
        }
        RequireThrows([&] { usearch_setText_nid_postfix(search, Empty, -1, &status); });
        Require(status == incoming);

        for (const auto attribute : {-1, 3, 4, 0x7fffffff}) {
            status = incoming;
            usearch_setAttribute_nid_postfix(nullptr, attribute, 1, &status);
            Require(status == incoming);
            usearch_setAttribute_nid_postfix(search, attribute, 1, &status);
            Require(status == IllegalArgument);
        }
        status = incoming;
        usearch_setAttribute_nid_postfix(nullptr, 0, AttributeValueCount, &status);
        Require(status == IllegalArgument);

        const auto illegalPattern = [&](UStringSearch* target, const char16_t* pattern, std::int32_t length) {
            status = incoming;
            usearch_setPattern_nid_postfix(target, pattern, length, &status);
            Require(status == IllegalArgument);
        };
        illegalPattern(nullptr, words, 4);
        illegalPattern(search, nullptr, 4);
        illegalPattern(nullptr, nullptr, 0);
        illegalPattern(search, words, 0);
        illegalPattern(search, Empty, -1);

        const auto illegalText = [&](UStringSearch* target, const char16_t* text, std::int32_t length) {
            status = incoming;
            usearch_setText_nid_postfix(target, text, length, &status);
            Require(status == IllegalArgument);
        };
        illegalText(nullptr, words, 8);
        illegalText(search, nullptr, 8);
        illegalText(search, words, 0);
        illegalText(search, words, -2);
        illegalText(search, words, std::numeric_limits<std::int32_t>::min());
    }
}

void NullStatus(UStringSearch* search, const char16_t* words) {
    for (auto* target : {search, static_cast<UStringSearch*>(nullptr)}) {
        RequireThrows([&] { usearch_next_nid_postfix(target, nullptr); });
        RequireThrows([&] { usearch_setOffset_nid_postfix(target, 3, nullptr); });
        RequireThrows([&] { usearch_setAttribute_nid_postfix(target, 0, 1, nullptr); });
        RequireThrows([&] { usearch_setPattern_nid_postfix(target, words, 4, nullptr); });
        RequireThrows([&] { usearch_setText_nid_postfix(target, words, 8, nullptr); });
    }
}

void GuestPointers(UStringSearch* search, const char16_t* words) {
    auto* badStatus = reinterpret_cast<std::int32_t*>(0x10);
    Rejects([&] { usearch_next_nid_postfix(nullptr, badStatus); });
    Rejects([&] { usearch_setOffset_nid_postfix(nullptr, 3, badStatus); });
    Rejects([&] { usearch_setAttribute_nid_postfix(nullptr, 0, 1, badStatus); });
    Rejects([&] { usearch_setPattern_nid_postfix(nullptr, words, 4, badStatus); });
    Rejects([&] { usearch_setText_nid_postfix(nullptr, words, 8, badStatus); });

    {
        ReadOnlyPage page;
        auto* roFailing = page.Set<std::int32_t>(IllegalArgument);
        page.Protect();
        errno = EINVAL;
        Require(usearch_next_nid_postfix(search, roFailing) == Done);
        usearch_setOffset_nid_postfix(search, 3, roFailing);
        usearch_setAttribute_nid_postfix(search, 0, 1, roFailing);
        usearch_setPattern_nid_postfix(nullptr, nullptr, 0, roFailing);
        usearch_setText_nid_postfix(nullptr, nullptr, 0, roFailing);
        Require(errno == EINVAL);
        Rejects([&] { usearch_setAttribute_nid_postfix(search, 0, AttributeValueCount, roFailing); });
        Require(*roFailing == IllegalArgument);
    }

    {
        ReadOnlyPage page;
        auto* roPassing = page.Set<std::int32_t>(0);
        page.Protect();
        errno = EINVAL;
        Require(usearch_next_nid_postfix(nullptr, roPassing) == Done);
        usearch_setOffset_nid_postfix(nullptr, 3, roPassing);
        usearch_setAttribute_nid_postfix(nullptr, 0, 1, roPassing);
        Require(errno == EINVAL);
        Rejects([&] { usearch_setAttribute_nid_postfix(search, 3, 1, roPassing); });
        Rejects([&] { usearch_setAttribute_nid_postfix(nullptr, 0, AttributeValueCount, roPassing); });
        Rejects([&] { usearch_setPattern_nid_postfix(nullptr, words, 4, roPassing); });
        Rejects([&] { usearch_setText_nid_postfix(nullptr, words, 8, roPassing); });
        Require(*roPassing == 0);
    }

    std::int32_t status = 0;
    Rejects([&] { usearch_setPattern_nid_postfix(search, reinterpret_cast<const char16_t*>(0x10), -1, &status); });
    Require(status == 0);
}

} // namespace

void Run() {
    const auto storage = Filler();
    const auto search = reinterpret_cast<UStringSearch*>(const_cast<std::uint32_t*>(storage.data()));
    const auto words = reinterpret_cast<const char16_t*>(storage.data());

    WithoutStatus(search);
    FailingStatus(search, words);
    PassingStatus(search, words);
    NullStatus(search, words);
    Require(Unchanged(storage));
    GuestPointers(search, words);
}

} // namespace UsearchObject

void CheckIcuStringSearch() {
    UsearchOpen::Run();
    UsearchObject::Run();
}

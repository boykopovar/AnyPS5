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
constexpr std::int32_t UnsupportedError = 16;

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

void GuestPointers(const char16_t* pattern, const char16_t* text, UBreakIterator* iterator) {
    auto* badStatus = reinterpret_cast<std::int32_t*>(0x10);
    Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, badStatus); });
    Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, iterator, badStatus); });

    {
        ReadOnlyPage page;
        auto* roFailing = page.Set<std::int32_t>(IllegalArgument);
        page.Protect();
        errno = EINVAL;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, iterator, roFailing) == nullptr);
        Require(errno == EINVAL);
        Require(*roFailing == IllegalArgument);
    }

    {
        ReadOnlyPage page;
        auto* roPassing = page.Set<std::int32_t>(0);
        page.Protect();
        Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, iterator, roPassing); });
        Require(*roPassing == 0);
        Rejects([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, roPassing); });
        Require(*roPassing == 0);
    }
}

constexpr std::int32_t SearchDone = -1;
constexpr std::int32_t AttributeValueCount = 5;

void FailingStatus(std::int32_t incoming, UStringSearch* search, const char16_t* text) {
    std::int32_t status = incoming;
    Require(usearch_next_nid_postfix(search, &status) == SearchDone);
    Require(status == incoming);
    usearch_setOffset_nid_postfix(search, 3, &status);
    Require(status == incoming);
    usearch_setText_nid_postfix(search, text, 8, &status);
    usearch_setText_nid_postfix(nullptr, nullptr, 0, &status);
    Require(status == incoming);
    usearch_setPattern_nid_postfix(search, text, 4, &status);
    usearch_setPattern_nid_postfix(nullptr, nullptr, 0, &status);
    Require(status == incoming);
    for (std::int32_t value = -1; value < AttributeValueCount; ++value) {
        usearch_setAttribute_nid_postfix(search, 0, value, &status);
        Require(status == incoming);
    }
    usearch_setAttribute_nid_postfix(search, 0, AttributeValueCount, &status);
    Require(status == IllegalArgument);
}

void PassingStatus(std::int32_t incoming, UStringSearch* search, const char16_t* text) {
    constexpr char16_t empty[1]{};
    constexpr char16_t terminated[3]{u'a', u'b', 0};
    std::int32_t status = incoming;
    Require(usearch_next_nid_postfix(nullptr, &status) == SearchDone);
    usearch_setOffset_nid_postfix(nullptr, 3, &status);
    for (std::int32_t value = -1; value < AttributeValueCount; ++value) {
        for (std::int32_t attribute = 0; attribute < 4; ++attribute) usearch_setAttribute_nid_postfix(nullptr, attribute, value, &status);
    }
    Require(status == incoming);
    RequireThrows([&] { usearch_next_nid_postfix(search, &status); });
    RequireThrows([&] { usearch_setOffset_nid_postfix(search, 3, &status); });
    RequireThrows([&] { usearch_setAttribute_nid_postfix(search, 0, 1, &status); });
    RequireThrows([&] { usearch_setText_nid_postfix(search, text, 8, &status); });
    RequireThrows([&] { usearch_setText_nid_postfix(search, terminated, -1, &status); });
    RequireThrows([&] { usearch_setText_nid_postfix(search, empty, -1, &status); });
    RequireThrows([&] { usearch_setPattern_nid_postfix(search, text, 4, &status); });
    RequireThrows([&] { usearch_setPattern_nid_postfix(search, terminated, -1, &status); });
    RequireThrows([&] { usearch_setPattern_nid_postfix(search, text, -2, &status); });
    Require(status == incoming);

    const auto illegal = [&](auto call) {
        status = incoming;
        call();
        Require(status == IllegalArgument);
    };
    illegal([&] { usearch_setAttribute_nid_postfix(nullptr, 0, AttributeValueCount, &status); });
    illegal([&] { usearch_setText_nid_postfix(nullptr, text, 8, &status); });
    illegal([&] { usearch_setText_nid_postfix(search, nullptr, 8, &status); });
    illegal([&] { usearch_setText_nid_postfix(search, text, -2, &status); });
    illegal([&] { usearch_setText_nid_postfix(search, text, 0, &status); });
    illegal([&] { usearch_setText_nid_postfix(search, empty, 0, &status); });
    illegal([&] { usearch_setPattern_nid_postfix(nullptr, text, 4, &status); });
    illegal([&] { usearch_setPattern_nid_postfix(search, nullptr, 4, &status); });
    illegal([&] { usearch_setPattern_nid_postfix(search, text, 0, &status); });
    illegal([&] { usearch_setPattern_nid_postfix(search, empty, -1, &status); });
}

void Handles(const char16_t* text, UStringSearch* search) {
    Require(usearch_getCollator_nid_postfix(nullptr) == nullptr);
    Require(usearch_getMatchedLength_nid_postfix(nullptr) == SearchDone);
    RequireNoThrow([] { usearch_reset_nid_postfix(nullptr); });
    RequireThrows([&] { usearch_getCollator_nid_postfix(search); });
    RequireThrows([&] { usearch_getMatchedLength_nid_postfix(search); });
    RequireThrows([&] { usearch_reset_nid_postfix(search); });

    for (const auto incoming : std::array<std::int32_t, 7>{1, 2, 7, 15, 16, 27, 0x7fffffff}) FailingStatus(incoming, search, text);
    for (const auto incoming : std::array<std::int32_t, 4>{0, -128, -124, -1}) PassingStatus(incoming, search, text);

    RequireThrows([] { usearch_next_nid_postfix(nullptr, nullptr); });
    RequireThrows([] { usearch_setOffset_nid_postfix(nullptr, 0, nullptr); });
    RequireThrows([] { usearch_setAttribute_nid_postfix(nullptr, 0, 0, nullptr); });
    RequireThrows([] { usearch_setText_nid_postfix(nullptr, nullptr, 0, nullptr); });
    RequireThrows([] { usearch_setPattern_nid_postfix(nullptr, nullptr, 0, nullptr); });
}

void HandleGuestPointers(const char16_t* text) {
    auto* badStatus = reinterpret_cast<std::int32_t*>(0x10);
    Rejects([&] { usearch_next_nid_postfix(nullptr, badStatus); });
    Rejects([&] { usearch_setOffset_nid_postfix(nullptr, 0, badStatus); });
    Rejects([&] { usearch_setAttribute_nid_postfix(nullptr, 0, 0, badStatus); });
    Rejects([&] { usearch_setText_nid_postfix(nullptr, text, 8, badStatus); });
    Rejects([&] { usearch_setPattern_nid_postfix(nullptr, text, 4, badStatus); });

    ReadOnlyPage page;
    auto* roPassing = page.Set<std::int32_t>(0);
    page.Protect();
    errno = EINVAL;
    Require(usearch_next_nid_postfix(nullptr, roPassing) == SearchDone);
    usearch_setOffset_nid_postfix(nullptr, 0, roPassing);
    usearch_setAttribute_nid_postfix(nullptr, 0, 0, roPassing);
    Require(errno == EINVAL);
    Rejects([&] { usearch_setAttribute_nid_postfix(nullptr, 0, AttributeValueCount, roPassing); });
    Rejects([&] { usearch_setText_nid_postfix(nullptr, text, 8, roPassing); });
    Rejects([&] { usearch_setPattern_nid_postfix(nullptr, text, 4, roPassing); });
    Require(*roPassing == 0);
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    const auto storage = Filler();
    const auto pattern = reinterpret_cast<const char16_t*>(storage.data());
    const auto text = reinterpret_cast<const char16_t*>(storage.data());
    const auto iterator = reinterpret_cast<UBreakIterator*>(const_cast<std::uint32_t*>(storage.data()));

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(usearch_open_nid_postfix(nullptr, -1, nullptr, -1, nullptr, nullptr, &status) == nullptr);
        Require(status == incoming);
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { usearch_open_nid_postfix(nullptr, 0, nullptr, 0, "de", nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        Require(usearch_open_nid_postfix(pattern, 4, text, 8, nullptr, iterator, &status) == nullptr);
        Require(status == IllegalArgument);
    }

    Require(Unchanged(storage));
    RequireThrows([&] { usearch_open_nid_postfix(pattern, 4, text, 8, "en", iterator, nullptr); });
    GuestPointers(pattern, text, iterator);

    const auto search = reinterpret_cast<UStringSearch*>(const_cast<std::uint32_t*>(storage.data()));
    Handles(text, search);
    HandleGuestPointers(text);
    Require(Unchanged(storage));
}

} // namespace UsearchOpen

void CheckIcuStringSearch() {
    UsearchOpen::Run();
}

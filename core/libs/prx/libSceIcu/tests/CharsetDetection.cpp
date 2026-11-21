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
constexpr std::int32_t MissingResource = 2;

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

void ExpectBest(Icu::CharsetDetector* detector, const char* text, std::int32_t length, const char* name, std::int32_t confidence) {
    std::int32_t error = Success;
    ucsdet_setText_nid_postfix(detector, text, length, &error);
    ExpectInt(error, Success, name);
    std::int32_t found = 0;
    const auto** matches = ucsdet_detectAll_nid_postfix(detector, &found, &error);
    ExpectInt(error, Success, name);
    Expect(matches != nullptr && found > 0, name);
    const char* actual = ucsdet_getName_nid_postfix(matches[0], &error);
    Expect(actual != nullptr && std::strcmp(actual, name) == 0, name);
    ExpectInt(ucsdet_getConfidence_nid_postfix(matches[0], &error), confidence, name);
    for (std::int32_t i = 1; i < found; ++i) {
        Expect(ucsdet_getConfidence_nid_postfix(matches[i], &error) <= confidence, "ucsdet_detectAll order");
    }
}

void TestDetection() {
    std::int32_t error = Success;
    auto* detector = ucsdet_open_nid_postfix(&error);
    ExpectInt(error, Success, "ucsdet_open");
    Expect(detector != nullptr, "ucsdet_open");

    std::int32_t found = -1;
    Expect(ucsdet_detectAll_nid_postfix(detector, &found, &error) == nullptr, "ucsdet_detectAll without text");
    ExpectInt(error, MissingResource, "ucsdet_detectAll without text status");

    ExpectBest(detector, "Gr\xC3\xBC\xC3\x9F" "e aus M\xC3\xBCnchen, \xC3\xA7" "a va tr\xC3\xA8s bien", -1, "UTF-8", 100);
    const char utf16[] = {'\xFE', '\xFF', '\0', 'h', '\0', 'i', '\0', '!'};
    ExpectBest(detector, utf16, sizeof(utf16), "UTF-16BE", 100);
    const char utf16le[] = {'\xFF', '\xFE', 'h', '\0', 'i', '\0', '!', '\0'};
    ExpectBest(detector, utf16le, sizeof(utf16le), "UTF-16LE", 100);
    ucsdet_close_nid_postfix(detector);
    ucsdet_close_nid_postfix(nullptr);
}

void TestFilterAndStatus() {
    std::int32_t error = Success;
    auto* detector = ucsdet_open_nid_postfix(&error);
    ExpectInt(ucsdet_enableInputFilter_nid_postfix(detector, 1), 0, "ucsdet_enableInputFilter initial");
    ExpectInt(ucsdet_enableInputFilter_nid_postfix(detector, 0), 1, "ucsdet_enableInputFilter previous");
    ExpectInt(ucsdet_enableInputFilter_nid_postfix(nullptr, 1), 0, "ucsdet_enableInputFilter(null)");

    std::int32_t failing = IllegalArgument;
    Expect(ucsdet_open_nid_postfix(&failing) == nullptr, "ucsdet_open with failing status");
    std::int32_t found = 0;
    Expect(ucsdet_detectAll_nid_postfix(nullptr, &found, &failing) == nullptr, "ucsdet_detectAll with failing status");
    Expect(ucsdet_getName_nid_postfix(nullptr, &failing) == nullptr, "ucsdet_getName with failing status");
    ExpectInt(ucsdet_getConfidence_nid_postfix(nullptr, &failing), 0, "ucsdet_getConfidence with failing status");
    ExpectInt(failing, IllegalArgument, "failing status unchanged");

    ExpectThrow<std::invalid_argument>([] { ucsdet_open_nid_postfix(nullptr); }, "ucsdet_open(null status)");
    ExpectThrow<std::invalid_argument>([detector] {
        std::int32_t status = Success;
        ucsdet_setText_nid_postfix(detector, nullptr, -1, &status);
    }, "ucsdet_setText(null, -1)");
    ExpectThrow<std::invalid_argument>([] {
        std::int32_t status = Success;
        ucsdet_getName_nid_postfix(nullptr, &status);
    }, "ucsdet_getName(null)");
    ucsdet_close_nid_postfix(detector);
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
    RejectsFaulted([&] { ucsdet_open_nid_postfix(badInt); }, "ucsdet_open bad error");
    RejectsFaulted([&] { ucsdet_setText_nid_postfix(nullptr, "a", 1, badInt); }, "ucsdet_setText bad error");
    std::int32_t found = -1;
    RejectsFaulted([&] { ucsdet_detectAll_nid_postfix(nullptr, &found, badInt); }, "ucsdet_detectAll bad error");
    ExpectInt(found, -1, "found unchanged on bad error");
    RejectsFaulted([&] { ucsdet_getName_nid_postfix(nullptr, badInt); }, "ucsdet_getName bad error");
    RejectsFaulted([&] { ucsdet_getConfidence_nid_postfix(nullptr, badInt); }, "ucsdet_getConfidence bad error");

    {
        ReadOnlyPage page;
        auto* roFail = page.Set<std::int32_t>(IllegalArgument);
        page.Protect();
        errno = EINVAL;
        Expect(ucsdet_open_nid_postfix(roFail) == nullptr, "ucsdet_open ro failure");
        ucsdet_setText_nid_postfix(nullptr, "a", 1, roFail);
        Expect(ucsdet_detectAll_nid_postfix(nullptr, &found, roFail) == nullptr, "ucsdet_detectAll ro failure");
        Expect(ucsdet_getName_nid_postfix(nullptr, roFail) == nullptr, "ucsdet_getName ro failure");
        ExpectInt(ucsdet_getConfidence_nid_postfix(nullptr, roFail), 0, "ucsdet_getConfidence ro failure");
        ExpectInt(*roFail, IllegalArgument, "ro failure status unchanged");
        ExpectInt(errno, EINVAL, "errno preserved");
    }

    std::int32_t error = Success;
    auto* detector = ucsdet_open_nid_postfix(&error);
    Expect(detector != nullptr && error == Success, "setup detector");
    ucsdet_setText_nid_postfix(detector, "Hello UTF-8 World", -1, &error);
    ExpectInt(error, Success, "setup setText");
    const auto** matches = ucsdet_detectAll_nid_postfix(detector, &found, &error);
    Expect(matches != nullptr && found > 0 && error == Success, "setup detectAll");

    {
        ReadOnlyPage page;
        auto* roSuccess = page.Set<std::int32_t>(Success);
        page.Protect();
        RejectsFaulted([&] { ucsdet_open_nid_postfix(roSuccess); }, "ucsdet_open ro success error");
        RejectsFaulted([&] { ucsdet_setText_nid_postfix(detector, "abc", 3, roSuccess); }, "ucsdet_setText ro error");
        std::int32_t localFound = -7;
        RejectsFaulted([&] { ucsdet_detectAll_nid_postfix(detector, &localFound, roSuccess); }, "ucsdet_detectAll ro error");
        ExpectInt(localFound, -7, "found untouched when error unwritable");
        RejectsFaulted([&] { ucsdet_getName_nid_postfix(matches[0], roSuccess); }, "ucsdet_getName ro error");
        RejectsFaulted([&] { ucsdet_getConfidence_nid_postfix(matches[0], roSuccess); }, "ucsdet_getConfidence ro error");
        ExpectInt(*roSuccess, Success, "ro success error unchanged");
    }

    {
        ReadOnlyPage page;
        auto* roFound = page.Set<std::int32_t>(123);
        page.Protect();
        error = Success;
        RejectsFaulted([&] { ucsdet_detectAll_nid_postfix(detector, roFound, &error); }, "ucsdet_detectAll ro found");
        ExpectInt(*roFound, 123, "ro found unchanged");
        ExpectInt(error, Success, "error unchanged when found unwritable");
        RejectsFaulted([&] { ucsdet_detectAll_nid_postfix(detector, badInt, &error); }, "ucsdet_detectAll bad found");
        ExpectInt(error, Success, "error unchanged on bad found");
    }

    ucsdet_close_nid_postfix(detector);
}

}

int main() {
    TestDetection();
    TestFilterAndStatus();
    TestGuestPointers();
    return 0;
}

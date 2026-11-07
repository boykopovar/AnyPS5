#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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

}

int main() {
    TestDetection();
    TestFilterAndStatus();
    return 0;
}

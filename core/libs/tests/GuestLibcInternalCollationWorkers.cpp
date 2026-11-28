#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>

namespace {

static_assert(std::is_same_v<decltype(&_Strxfrmx_nid_postfix),
                             size_t (APS5_VABI*)(char*, const char*, size_t, LibcCollationState*)>);
static_assert(std::is_same_v<decltype(&_Wcscollx_nid_postfix),
                             int (APS5_VABI*)(const char16_t*, const char16_t*, LibcCollationState*)>);
static_assert(std::is_same_v<decltype(&_Wcsxfrmx_nid_postfix),
                             size_t (APS5_VABI*)(char16_t*, const char16_t*, size_t, LibcCollationState*)>);
static_assert(std::is_same_v<decltype(&_Wcsftime_nid_postfix),
                             size_t (APS5_VABI*)(char16_t*, size_t, const char16_t*, const GuestTm*, void*)>);

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "collation worker check failed at line %d\n", line);
    std::abort();
}
#define REQUIRE(value) Require((value), __LINE__)

template <typename TAction>
bool Rejects(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int Sign(int value) {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

void CheckStrxfrmx() {
    LibcCollationState* state = _Getpcostate_nid_postfix();
    char out[16];

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "alpha", sizeof(out), nullptr) == 5);
    REQUIRE(std::memcmp(out, "alpha", 6) == 0);
    REQUIRE(out[6] == '#');

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "alpha", 6, state) == 5);
    REQUIRE(std::memcmp(out, "alpha", 6) == 0);
    REQUIRE(out[6] == '#');

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "alpha", 5, state) == 5);
    REQUIRE(std::memcmp(out, "alpha", 5) == 0);
    REQUIRE(out[5] == '#');

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "alpha", 2, state) == 5);
    REQUIRE(out[0] == 'a' && out[1] == 'l' && out[2] == '#');

    REQUIRE(_Strxfrmx_nid_postfix(nullptr, "alpha", 0, state) == 5);

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "", sizeof(out), state) == 0);
    REQUIRE(out[0] == '\0' && out[1] == '#');

    const char high[] = {'\x80', 'z', '\0'};
    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, high, sizeof(out), state) == 2);
    REQUIRE(std::memcmp(out, high, 3) == 0);

    std::memset(out, '#', sizeof(out));
    REQUIRE(_Strxfrmx_nid_postfix(out, "a\0z", sizeof(out), state) == 1);
    REQUIRE(out[0] == 'a' && out[1] == '\0' && out[2] == '#');

    char expected[16];
    std::memset(out, '#', sizeof(out));
    std::memset(expected, '#', sizeof(expected));
    REQUIRE(_Strxfrmx_nid_postfix(out, "collate", 4, state) == strxfrm_nid_postfix(expected, "collate", 4));
    REQUIRE(std::memcmp(out, expected, sizeof(out)) == 0);

    REQUIRE(Rejects([&] { (void)_Strxfrmx_nid_postfix(out, nullptr, sizeof(out), state); }));
    REQUIRE(Rejects([&] { (void)_Strxfrmx_nid_postfix(nullptr, "alpha", 1, state); }));
}

void CheckWcsxfrmx() {
    LibcCollationState* state = _Getpwcostate_nid_postfix();
    constexpr char16_t Fill = u'#';
    char16_t out[16];
    const auto reset = [&] { for (char16_t& unit : out) unit = Fill; };

    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"alpha", 16, nullptr) == 5);
    REQUIRE(std::memcmp(out, u"alpha", 6 * sizeof(char16_t)) == 0);
    REQUIRE(out[6] == Fill);

    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"alpha", 6, state) == 5);
    REQUIRE(std::memcmp(out, u"alpha", 6 * sizeof(char16_t)) == 0);
    REQUIRE(out[6] == Fill);

    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"alpha", 5, state) == 5);
    REQUIRE(std::memcmp(out, u"alpha", 5 * sizeof(char16_t)) == 0);
    REQUIRE(out[5] == Fill);

    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"alpha", 2, state) == 5);
    REQUIRE(out[0] == u'a' && out[1] == u'l' && out[2] == Fill);

    REQUIRE(_Wcsxfrmx_nid_postfix(nullptr, u"alpha", 0, state) == 5);

    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"", 16, state) == 0);
    REQUIRE(out[0] == u'\0' && out[1] == Fill);

    const char16_t high[] = {0xFFFF, 0xD800, u'z', 0};
    reset();
    REQUIRE(_Wcsxfrmx_nid_postfix(out, high, 16, state) == 3);
    REQUIRE(std::memcmp(out, high, sizeof(high)) == 0);

    char16_t expected[16];
    reset();
    for (char16_t& unit : expected) unit = Fill;
    REQUIRE(_Wcsxfrmx_nid_postfix(out, u"collate", 4, state) == wcsxfrm_nid_postfix(expected, u"collate", 4));
    REQUIRE(std::memcmp(out, expected, sizeof(out)) == 0);

    REQUIRE(Rejects([&] { (void)_Wcsxfrmx_nid_postfix(out, nullptr, 16, state); }));
    REQUIRE(Rejects([&] { (void)_Wcsxfrmx_nid_postfix(nullptr, u"alpha", 1, state); }));
}

void CheckWcscollx() {
    LibcCollationState* state = _Getpwcostate_nid_postfix();
    REQUIRE(_Wcscollx_nid_postfix(u"", u"", nullptr) == 0);
    REQUIRE(_Wcscollx_nid_postfix(u"alpha", u"alpha", state) == 0);
    REQUIRE(Sign(_Wcscollx_nid_postfix(u"alpha", u"beta", state)) == -1);
    REQUIRE(Sign(_Wcscollx_nid_postfix(u"beta", u"alpha", state)) == 1);
    REQUIRE(Sign(_Wcscollx_nid_postfix(u"al", u"alpha", state)) == -1);
    REQUIRE(Sign(_Wcscollx_nid_postfix(u"alpha", u"al", state)) == 1);

    const char16_t high[] = {0xFF00, 0};
    REQUIRE(Sign(_Wcscollx_nid_postfix(high, u"a", state)) == 1);
    REQUIRE(Sign(_Wcscollx_nid_postfix(u"a", high, state)) == -1);
    REQUIRE(_Wcscollx_nid_postfix(u"a\0z", u"a", state) == 0);

    const char16_t* pairs[][2] = {{u"alpha", u"beta"}, {u"zeta", u"eta"}, {u"same", u"same"}, {high, u"b"}};
    for (const auto& pair : pairs) {
        REQUIRE(Sign(_Wcscollx_nid_postfix(pair[0], pair[1], state)) == Sign(wcscoll_nid_postfix(pair[0], pair[1])));
    }

    REQUIRE(Rejects([&] { (void)_Wcscollx_nid_postfix(nullptr, u"a", state); }));
    REQUIRE(Rejects([&] { (void)_Wcscollx_nid_postfix(u"a", nullptr, state); }));
}

void CheckWcsftime() {
    GuestTm tm{};
    tm.tm_year = 120;
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_hour = 12;
    tm.tm_min = 34;
    tm.tm_sec = 56;

    char16_t buffer[64];
    constexpr char16_t Fill = u'#';
    const auto reset = [&] { for (char16_t& unit : buffer) unit = Fill; };

    reset();
    size_t result = _Wcsftime_nid_postfix(buffer, 64, u"%Y-%m-%d", &tm, nullptr);
    REQUIRE(result == 10);
    REQUIRE(std::memcmp(buffer, u"2020-01-01", 10 * sizeof(char16_t)) == 0);

    reset();
    int dummyLocale = 42;
    result = _Wcsftime_nid_postfix(buffer, 64, u"%Y-%m-%d", &tm, &dummyLocale);
    REQUIRE(result == 10);
    REQUIRE(std::memcmp(buffer, u"2020-01-01", 10 * sizeof(char16_t)) == 0);

    reset();
    char16_t expected[64];
    for (char16_t& unit : expected) unit = Fill;
    size_t expectedLen = wcsftime_nid_postfix(reinterpret_cast<std::uint16_t*>(expected), 64, reinterpret_cast<const std::uint16_t*>(u"%H:%M:%S"), &tm);
    result = _Wcsftime_nid_postfix(buffer, 64, u"%H:%M:%S", &tm, nullptr);
    REQUIRE(result == expectedLen);
    REQUIRE(std::memcmp(buffer, expected, sizeof(buffer)) == 0);

    REQUIRE(_Wcsftime_nid_postfix(nullptr, 0, u"%Y", &tm, nullptr) == 0);

    REQUIRE(Rejects([&] { (void)_Wcsftime_nid_postfix(buffer, 64, nullptr, &tm, nullptr); }));
    REQUIRE(Rejects([&] { (void)_Wcsftime_nid_postfix(buffer, 64, u"%Y", nullptr, nullptr); }));
    REQUIRE(Rejects([&] { (void)_Wcsftime_nid_postfix(nullptr, 64, u"%Y", &tm, nullptr); }));
}

}

int main() {
    CheckStrxfrmx();
    CheckWcsxfrmx();
    CheckWcscollx();
    CheckWcsftime();
    return 0;
}

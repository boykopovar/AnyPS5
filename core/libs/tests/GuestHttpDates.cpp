#include "prx/libSceHttp/src/Client.cpp"

#include <cstdio>

int main() {
    using namespace std::chrono;
    const sys_seconds now = sys_days{year{2026} / 11 / 11} + hours{12} + minutes{34} + seconds{56};
    const sys_seconds century = sys_days{year{2050} / 1 / 1};
    const sys_seconds nextCentury = sys_days{year{2090} / 1 / 1};
    struct Case {
        const char* input;
        sys_seconds received;
        const char* expected;
    };
    const Case cases[] = {
        {"Wednesday, 01-Jan-20 00:00:00 GMT", now, "Wed, 01 Jan 2020 00:00:00 GMT"},
        {"Wed Jan  1 00:00:00 2020", now, "Wed, 01 Jan 2020 00:00:00 GMT"},
        {"Wednesday, 11-Nov-76 12:34:55 GMT", now, "Wed, 11 Nov 2076 12:34:55 GMT"},
        {"Wednesday, 11-Nov-76 12:34:56 GMT", now, "Wed, 11 Nov 2076 12:34:56 GMT"},
        {"Thursday, 11-Nov-76 12:34:57 GMT", now, "Thu, 11 Nov 1976 12:34:57 GMT"},
        {"Friday, 12-Nov-76 00:00:00 GMT", now, "Fri, 12 Nov 1976 00:00:00 GMT"},
        {"Wednesday, 11-Nov-76 12:34:57 GMT", now, "Wednesday, 11-Nov-76 12:34:57 GMT"},
        {"Friday, 01-Jan-00 00:00:00 GMT", century, "Fri, 01 Jan 2100 00:00:00 GMT"},
        {"Saturday, 01-Jan-00 00:00:01 GMT", century, "Sat, 01 Jan 2000 00:00:01 GMT"},
        {"Monday, 01-Jan-20 00:00:00 GMT", nextCentury, "Mon, 01 Jan 2120 00:00:00 GMT"},
        {"Sat, 31 Dec 2016 23:59:60 GMT", now, "Sat, 31 Dec 2016 23:59:60 GMT"},
        {"Wed Jax  1 00:00:00 2020", now, "Wed Jax  1 00:00:00 2020"},
    };
    int passed = 0;
    for (const auto& test : cases) {
        const auto actual = CanonicalDate(test.input, test.received);
        if (actual == test.expected) {
            ++passed;
        } else {
            std::fprintf(stderr, "HTTP date %s: expected %s, received %s\n", test.input, test.expected, actual.c_str());
        }
    }
    std::printf("HTTP date deterministic cases: %d/12\n", passed);
    return passed == 12 ? 0 : 1;
}

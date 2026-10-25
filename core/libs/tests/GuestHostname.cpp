#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Hostname check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

int main() {
    constexpr std::string_view expected = "AnyPS5";
    for (std::size_t length = 0; length <= expected.size() + 2; ++length) {
        std::vector<char> buffer(length + 2, '#');
        *__error_nid_postfix() = 71;
        const int result = gethostname_nid_postfix(buffer.data() + 1, length);
        Check(buffer.front() == '#' && buffer.back() == '#');
        if (length <= expected.size()) {
            Check(result == -1 && *__error_nid_postfix() == 63);
            Check(std::memcmp(buffer.data() + 1, expected.data(), length) == 0);
        } else {
            Check(result == 0 && *__error_nid_postfix() == 71);
            Check(std::strcmp(buffer.data() + 1, expected.data()) == 0);
            for (std::size_t index = expected.size() + 2; index < buffer.size(); ++index) Check(buffer[index] == '#');
        }
    }
    std::array<char, expected.size() + 3> oversized;
    oversized.fill('#');
    *__error_nid_postfix() = 71;
    Check(gethostname_nid_postfix(oversized.data() + 1, std::numeric_limits<std::size_t>::max()) == 0);
    Check(*__error_nid_postfix() == 71);
    Check(std::strcmp(oversized.data() + 1, expected.data()) == 0);
    Check(oversized.front() == '#' && oversized.back() == '#');
    *__error_nid_postfix() = 22;
    Check(gethostname_nid_postfix(nullptr, 0) == 0 && *__error_nid_postfix() == 22);
    Check(gethostname_nid_postfix(nullptr, 1024) == 0 && *__error_nid_postfix() == 22);
    std::thread worker([expected] {
        char byte = '#';
        *__error_nid_postfix() = 0;
        Check(gethostname_nid_postfix(&byte, 0) == -1 && *__error_nid_postfix() == 63 && byte == '#');
        std::array<char, expected.size() + 1> name{};
        Check(gethostname_nid_postfix(name.data(), name.size()) == 0);
        Check(std::strcmp(name.data(), expected.data()) == 0 && *__error_nid_postfix() == 63);
    });
    worker.join();
    Check(*__error_nid_postfix() == 22);
}

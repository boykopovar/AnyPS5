#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

int main() {
    constexpr std::string_view expected = "";
    for (std::size_t length = 0; length <= expected.size() + 2; ++length) {
        std::vector<char> buffer(length + 2, '#');
        *__error_nid_postfix() = 71;
        const int result = gethostname_nid_postfix(buffer.data() + 1, length);
        Require(buffer.front() == '#' && buffer.back() == '#');
        if (length <= expected.size()) {
            Require(result == -1 && *__error_nid_postfix() == 63);
            Require(std::memcmp(buffer.data() + 1, expected.data(), length) == 0);
        } else {
            Require(result == 0 && *__error_nid_postfix() == 71);
            Require(std::strcmp(buffer.data() + 1, expected.data()) == 0);
            for (std::size_t index = expected.size() + 2; index < buffer.size(); ++index) Require(buffer[index] == '#');
        }
    }
    std::array<char, expected.size() + 3> oversized;
    oversized.fill('#');
    *__error_nid_postfix() = 71;
    Require(gethostname_nid_postfix(oversized.data() + 1, std::numeric_limits<std::size_t>::max()) == 0);
    Require(*__error_nid_postfix() == 71);
    Require(std::strcmp(oversized.data() + 1, expected.data()) == 0);
    Require(oversized.front() == '#' && oversized.back() == '#');
    *__error_nid_postfix() = 22;
    Require(gethostname_nid_postfix(nullptr, 0) == 0 && *__error_nid_postfix() == 22);
    Require(gethostname_nid_postfix(nullptr, 1024) == 0 && *__error_nid_postfix() == 22);
    std::thread worker([expected] {
        char byte = '#';
        *__error_nid_postfix() = 0;
        Require(gethostname_nid_postfix(&byte, 0) == -1 && *__error_nid_postfix() == 63 && byte == '#');
        std::array<char, expected.size() + 1> name{};
        Require(gethostname_nid_postfix(name.data(), name.size()) == 0);
        Require(std::strcmp(name.data(), expected.data()) == 0 && *__error_nid_postfix() == 63);
    });
    worker.join();
    Require(*__error_nid_postfix() == 22);
}

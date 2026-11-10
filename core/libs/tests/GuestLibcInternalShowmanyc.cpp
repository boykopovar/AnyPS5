#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <type_traits>

static_assert(std::is_same_v<decltype(&_ZNSt15basic_streambufIcSt11char_traitsIcEE9showmanycEv),
                             std::streamsize (APS5_VABI *)(std::basic_streambuf<char>*)>);
static_assert(std::is_same_v<decltype(&_ZNSt15basic_streambufIwSt11char_traitsIwEE9showmanycEv),
                             std::streamsize (APS5_VABI *)(std::basic_streambuf<wchar_t>*)>);

int main() {
    std::stringbuf narrow("data");
    std::wstringbuf wide(L"data");
    if (_ZNSt15basic_streambufIcSt11char_traitsIcEE9showmanycEv(&narrow) != 0 || narrow.str() != "data") {
        std::fputs("narrow showmanyc did not return zero without changing the buffer\n", stderr);
        return EXIT_FAILURE;
    }
    if (_ZNSt15basic_streambufIwSt11char_traitsIwEE9showmanycEv(&wide) != 0 || wide.str() != L"data") {
        std::fputs("wide showmanyc did not return zero without changing the buffer\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

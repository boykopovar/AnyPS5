#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <type_traits>

static_assert(std::is_same_v<decltype(&libSceLibcInternalInventory_047_nid_no_patch),
                             std::streamsize (APS5_VABI *)(std::basic_streambuf<char>*)>);
static_assert(std::is_same_v<decltype(&libSceLibcInternalInventory_050_nid_no_patch),
                             std::streamsize (APS5_VABI *)(std::basic_streambuf<wchar_t>*)>);

int main() {
    std::stringbuf narrow("data");
    std::wstringbuf wide(L"data");
    if (libSceLibcInternalInventory_047_nid_no_patch(&narrow) != 0 || narrow.str() != "data") {
        std::fputs("narrow showmanyc did not return zero without changing the buffer\n", stderr);
        return EXIT_FAILURE;
    }
    if (libSceLibcInternalInventory_050_nid_no_patch(&wide) != 0 || wide.str() != L"data") {
        std::fputs("wide showmanyc did not return zero without changing the buffer\n", stderr);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

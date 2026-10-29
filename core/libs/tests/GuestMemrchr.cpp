#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

int main() {
    const unsigned char bytes[] = {0xff, 0x01, 0xff, 0x00};
    const auto* found = static_cast<const unsigned char*>(memrchr_nid_postfix(bytes, 0xff, sizeof(bytes)));
    if (found != bytes + 2) std::abort();
    found = static_cast<const unsigned char*>(memrchr_nid_postfix(bytes, 0x1ff, sizeof(bytes)));
    if (found != bytes + 2) std::abort();
    found = static_cast<const unsigned char*>(memrchr_nid_postfix(bytes, 0xff, 2));
    if (found != bytes) std::abort();
    if (memrchr_nid_postfix(bytes, 0x02, sizeof(bytes)) != nullptr) std::abort();
}

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

    *__error_nid_postfix() = 42;
    if (memrchr_nid_postfix(bytes, 0xff, 0) != nullptr) std::abort();
    if (memrchr_nid_postfix(nullptr, 0x00, 0) != nullptr) std::abort();
    if (memrchr_nid_postfix(bytes, -1, sizeof(bytes)) != bytes + 2) std::abort();
    if (memrchr_nid_postfix(bytes, 0x00, sizeof(bytes)) != bytes + 3) std::abort();
    if (memrchr_nid_postfix(bytes, -256, sizeof(bytes)) != bytes + 3) std::abort();
    if (memrchr_nid_postfix(bytes, 0x01, sizeof(bytes)) != bytes + 1) std::abort();
    if (memrchr_nid_postfix(bytes, 0x01, 1) != nullptr) std::abort();

    const unsigned char single[] = {0x7a};
    if (memrchr_nid_postfix(single, 0x7a, sizeof(single)) != single) std::abort();
    const unsigned char nulled[] = {0x00, 0x41, 0x00, 0x42};
    if (memrchr_nid_postfix(nulled, 0x00, sizeof(nulled)) != nulled + 2) std::abort();
    if (memrchr_nid_postfix(nulled, 0x41, sizeof(nulled)) != nulled + 1) std::abort();
    if (*__error_nid_postfix() != 42) std::abort();
}

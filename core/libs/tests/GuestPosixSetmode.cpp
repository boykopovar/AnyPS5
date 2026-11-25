#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestPosixSetmode check failed\n", stderr);
        std::abort();
    }
}

namespace {

struct Case {
    const char* mode;
    std::uint16_t start;
    std::uint16_t expected;
};

constexpr Case Cases[] = {
    {"755", 0, 0755},
    {"u+x", 0644, 0744},
    {"a-w", 0666, 0444},
    {"go-w", 0664, 0644},
    {"+X", 0644, 0644},
    {"+X", 0744, 0755},
    {"=r", 0777, 0444},
    {"ug=rw", 0700, 0660},
    {"o+t", 0755, 0755},
    {"u+x+w", 0600, 0700},
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const std::uint16_t saved = umask_nid_postfix(022);

    for (const Case& test : Cases) {
        void* list = setmode_nid_postfix(test.mode);
        Require(list != nullptr);
        Require(getmode_nid_postfix(list, test.start) == test.expected);
        free_nid_postfix(list);
    }

    const char* malformed[] = {"", "8", "u+q", "0777777"};
    for (const char* mode : malformed) {
        *__error_nid_postfix() = 0;
        Require(setmode_nid_postfix(mode) == nullptr);
        Require(*__error_nid_postfix() == 22);
    }

    umask_nid_postfix(saved);
    return 0;
}

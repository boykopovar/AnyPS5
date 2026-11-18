#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdio>
#include <cstdlib>

struct Case {
    const char* pattern;
    const char* input;
    int flags;
    int expected;
};

int main() {
    static const Case cases[] = {
        {"[.]hidden", ".hidden", 0x00, 0},
        {"[.]hidden", ".hidden", 0x04, 1},
        {"dir/[.]hidden", "dir/.hidden", 0x04, 0},
        {"dir/[.]hidden", "dir/.hidden", 0x06, 1},
        {"dir/a[.]hidden", "dir/a.hidden", 0x06, 0},
    };
    for (const Case& entry : cases) {
        const int actual = fnmatch_nid_postfix(entry.pattern, entry.input, entry.flags);
        if (actual != entry.expected) {
            std::fprintf(stderr, "pattern=\"%s\" input=\"%s\" flags=0x%x expected=%d actual=%d\n",
                entry.pattern, entry.input, entry.flags, entry.expected, actual);
            std::abort();
        }
    }
    return 0;
}

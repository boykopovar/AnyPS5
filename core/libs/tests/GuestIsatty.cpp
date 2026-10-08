#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI isatty_nid_postfix(int);
}

int main() {
    errno = 0;
    if (isatty_nid_postfix(-1) != 0 || errno != EBADF) return 1;
    std::FILE* file = std::tmpfile();
    if (!file) return 2;
#ifdef _WIN32
    const int descriptor = ::_fileno(file);
#else
    const int descriptor = ::fileno(file);
#endif
    if (descriptor < 0) return 3;
    const int result = isatty_nid_postfix(descriptor);
    std::fclose(file);
    return result == 0 ? 0 : 4;
}

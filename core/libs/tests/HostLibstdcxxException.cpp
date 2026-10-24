#include <cstdio>
#include <string>
#include <unistd.h>

extern "C" const char* HostLibstdcxxExceptionFailure(const char* path);

int main() {
    const std::string path = "/proc/" + std::to_string(getpid()) + "/missing/file";
    if (const char* failure = HostLibstdcxxExceptionFailure(path.c_str())) {
        std::fprintf(stderr, "%s\n", failure);
        return 1;
    }
    std::puts("host libstdc++ exceptions caught");
    return 0;
}

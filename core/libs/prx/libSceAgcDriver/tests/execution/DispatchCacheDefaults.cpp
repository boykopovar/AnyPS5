#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
#ifdef _WIN32
    _putenv_s("APS5_DISPATCH_CACHE_BYTES", "");
#else
    unsetenv("APS5_DISPATCH_CACHE_BYTES");
#endif
    const auto bytes = AgcDriver::DriverDetail::Driver::DispatchCacheBytes();
    if (bytes != (std::uint64_t{1} << 30u)) {
        std::fprintf(stderr, "the default dispatch cache is %llu bytes, not 1 GiB\n", static_cast<unsigned long long>(bytes));
        return 1;
    }
    std::printf("The default dispatch cache is 1 GiB\n");
    return 0;
}

#include <cstdlib>

extern "C" int aps5_preload() {
    const char* result = std::getenv("HOST_PRELOAD_FIXTURE_RESULT");
    return result == nullptr ? 7 : std::atoi(result);
}

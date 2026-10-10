#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct Pending {
    std::size_t args = 0;
    const void* argp = nullptr;
} pending;

[[noreturn]] void Fail(const char* message) noexcept {
    std::fprintf(stderr, "Guest module argument fixture: %s\n", message);
    std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}

FILE* Events() {
    const auto* path = std::getenv("ANYPS5_GUEST_MODULE_ARGS");
    if (!path) Fail("Missing ANYPS5_GUEST_MODULE_ARGS");
    auto* file = std::fopen(path, "ab");
    if (!file) Fail("Cannot open argument event file");
    return file;
}

void Write(const char* text) {
    auto* file = Events();
    const auto written = std::fputs(text, file);
    const auto closed = std::fclose(file);
    if (written == EOF || closed != 0) Fail("Cannot write argument event");
}

}

extern "C" void APS5_VABI RecordArgs(std::size_t args, const void* argp) {
    char line[80];
    std::snprintf(line, sizeof(line), "args=%016zx argp=%016zx\n", args,
                  static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(argp)));
    Write(line);
}

extern "C" void SetPendingArgs(std::size_t args, const void* argp) {
    pending.args = args;
    pending.argp = argp;
}

extern "C" const void* __aps5_get_pending_module_args_nid_no_patch() {
    return &pending;
}

extern "C" void __aps5_set_module_init_result_nid_no_patch(int result) {
    char line[40];
    std::snprintf(line, sizeof(line), "result=%d\n", result);
    Write(line);
}

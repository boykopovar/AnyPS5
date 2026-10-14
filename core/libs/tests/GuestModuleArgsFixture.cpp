#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct PendingArgs {
    std::size_t args;
    const void* argp;
};

extern "C" const void* __aps5_get_pending_module_args_nid_no_patch();

namespace {

struct ArgsRecorder {
    ArgsRecorder() {
        const auto* path = std::getenv("ANYPS5_GUEST_MODULE_ARGS");
        if (!path) std::abort();
        auto* file = std::fopen(path, "ab");
        if (!file) std::abort();
        const auto* pending = static_cast<const PendingArgs*>(__aps5_get_pending_module_args_nid_no_patch());
        std::size_t sum = 0;
        if (pending->argp) {
            const auto* bytes = static_cast<const std::uint8_t*>(pending->argp);
            for (std::size_t index = 0; index < pending->args; ++index) sum += bytes[index];
        }
        std::fprintf(file, "args=%zu argp=%zx sum=%zu\n", pending->args, static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pending->argp)), sum);
        std::fclose(file);
    }
};

const ArgsRecorder recorder;

}

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Posix spawn errors check failed at line %d\n", line);
        std::exit(EXIT_FAILURE);
    }
}
#define Check(value) Require((value), __LINE__)

int main(int count, char** arguments) {
    if (count == 2 && std::strcmp(arguments[1], "spawn-errors-child") == 0) return 43;
    std::filesystem::create_directories("app0");
    {
        std::ofstream image("app0/eboot.bin");
        image << "image";
        Check(image.good());
    }
    std::filesystem::remove("app0/spawn-error-loop");
    std::filesystem::create_symlink("spawn-error-loop", "app0/spawn-error-loop");
    const std::array cases{
        std::pair{std::string("/app0/spawn-error-loop"), 62},
        std::pair{std::string("/app0/") + std::string(256, 'x'), 63},
        std::pair{std::string("/app0/eboot.bin"), 0}
    };
    char program[] = "/app0/eboot.bin";
    char role[] = "spawn-errors-child";
    char* childArguments[] = {program, role, nullptr};
    bool passed = true;
    for (auto spawn : std::array{&posix_spawn_nid_postfix, &posix_spawnp_nid_postfix}) {
        for (const auto& [path, expected] : cases) {
            PosixSpawnFileActions* actions = nullptr;
            Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
            Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, 9, path.c_str(), 0, 0) == 0);
            constexpr std::int32_t Sentinel = -12345;
            std::int32_t process = Sentinel;
            const int result = spawn(&process, program, &actions, nullptr, childArguments, nullptr);
            Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
            if (result == 0) {
                Check(process > 0);
                int status = -1;
                Check(waitpid_nid_postfix(process, &status, 0) == process);
                Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 43);
            } else {
                Check(process == Sentinel);
            }
            if (result != expected) {
                std::fprintf(stderr, "Spawn action %s: expected guest error %d, observed %d\n", path.c_str(), expected, result);
                passed = false;
            }
        }
    }
    Check(std::filesystem::remove("app0/spawn-error-loop"));
    Check(std::filesystem::remove("app0/eboot.bin"));
    Check(std::filesystem::remove("app0"));
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

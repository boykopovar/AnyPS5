#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Posix spawn optional PID check failed at line %d\n", line);
        std::exit(EXIT_FAILURE);
    }
}
#define Check(value) Require((value), __LINE__)

int main(int count, char** arguments) {
#ifdef _WIN32
    constexpr int ChildArgument = 2;
#else
    constexpr int ChildArgument = 1;
#endif
    if (count == ChildArgument + 1 && std::strcmp(arguments[ChildArgument], "spawn-optional-pid-child") == 0) {
        std::puts("spawned-with-actions");
        std::fflush(stdout);
        return 43;
    }
    std::filesystem::create_directories("app0");
    {
        std::ofstream image("app0/eboot.bin");
        image << "image";
        Check(image.good());
    }
    char program[] = "/app0/eboot.bin";
    char role[] = "spawn-optional-pid-child";
    char* childArguments[] = {program, role, nullptr};
    const std::array spawnFunctions{&posix_spawn_nid_postfix, &posix_spawnp_nid_postfix};
    for (std::size_t index = 0; index < spawnFunctions.size(); ++index) {
        const auto spawn = spawnFunctions[index];
        std::fprintf(stderr, "Spawn wrapper %zu: testing a null PID output\n", index);
        Check(spawn(nullptr, program, nullptr, nullptr, childArguments, nullptr) == 0);
        int status = -1;
        Check(waitpid_nid_postfix(-1, &status, 0) > 0);
        Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 43);
        Check(spawn(nullptr, "/app0/missing.bin", nullptr, nullptr, childArguments, nullptr) == 2);
        Check(spawn(nullptr, program, nullptr, nullptr, nullptr, nullptr) == 14);

        PosixSpawnFileActions* actions = nullptr;
        PosixSpawnAttributes* attributes = nullptr;
        Check(posix_spawn_file_actions_init_nid_postfix(&actions) == 0);
        Check(posix_spawn_file_actions_addopen_nid_postfix(&actions, 1, "/app0/spawn_out.txt", 0x601, 0644) == 0);
        Check(posix_spawnattr_init_nid_postfix(&attributes) == 0);
        Check(posix_spawnattr_setflags_nid_postfix(&attributes, 0) == 0);
        Check(spawn(nullptr, program, &actions, &attributes, childArguments, nullptr) == 0);
        int actionStatus = -1;
        Check(waitpid_nid_postfix(-1, &actionStatus, 0) > 0);
        Check((actionStatus & 0x7f) == 0 && ((actionStatus >> 8) & 0xff) == 43);
        Check(posix_spawnattr_destroy_nid_postfix(&attributes) == 0);
        Check(posix_spawn_file_actions_destroy_nid_postfix(&actions) == 0);
        std::ifstream output("app0/spawn_out.txt", std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(output)), std::istreambuf_iterator<char>());
        Check(content.find("spawned-with-actions") != std::string::npos);
        output.close();
        Check(std::filesystem::remove("app0/spawn_out.txt"));
    }
    Check(std::filesystem::remove("app0/eboot.bin"));
    Check(std::filesystem::remove("app0"));
}

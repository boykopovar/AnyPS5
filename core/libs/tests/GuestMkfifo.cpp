#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
#include <filesystem>
#include <string>
#include <sys/stat.h>

static void Require(bool condition) { if (!condition) std::abort(); }

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::filesystem::path base = argv[1];
    std::filesystem::remove_all(base);
    Require(std::filesystem::create_directory(base));
#ifdef _WIN32
    bool notImplemented = false;
    try {
        mkfifo_nid_postfix((base / "pipe").string().c_str(), 0600);
    } catch (const std::runtime_error&) {
        notImplemented = true;
    }
    Require(notImplemented);
    std::filesystem::remove_all(base);
    return 0;
#else

    const std::string fifo = (base / "pipe").string();
    Require(mkfifo_nid_postfix(fifo.c_str(), 0600) == 0);
    struct stat status{};
    Require(::stat(fifo.c_str(), &status) == 0 && S_ISFIFO(status.st_mode));

    *__error_nid_postfix() = 0;
    Require(mkfifo_nid_postfix(fifo.c_str(), 0600) != 0 && *__error_nid_postfix() == 17);

    const std::string missing = (base / "absent" / "pipe").string();
    *__error_nid_postfix() = 0;
    Require(mkfifo_nid_postfix(missing.c_str(), 0600) != 0 && *__error_nid_postfix() == 2);

    std::filesystem::remove_all(base);
    return 0;
#endif
}

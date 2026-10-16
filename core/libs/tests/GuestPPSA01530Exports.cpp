#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI scePadUnknown_n3kSX62fgNo();
}

namespace {

struct Export {
    const char* name;
    int (APS5_VABI *function)();
};

const Export exports[] = {
    {"scePadUnknown_n3kSX62fgNo", scePadUnknown_n3kSX62fgNo},
};

bool ReportsNotImplemented(const Export& entry) {
    try {
        entry.function();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == std::string(entry.name) + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", entry.name, error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", entry.name);
    return false;
}

}

int main() {
    bool correct = true;
    for (const auto& entry : exports) correct &= ReportsNotImplemented(entry);
    return correct ? 0 : 1;
}

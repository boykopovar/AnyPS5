#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>
extern "C" int APS5_VABI daemon_nid_postfix(int, int);
static void Require(bool value) { if (!value) std::abort(); }
static bool ReportsNotImplemented(int noChdir, int noClose) {
    try {
        daemon_nid_postfix(noChdir, noClose);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "daemon: forking into the background not implemented";
    }
    return false;
}
int main() {
    Require(ReportsNotImplemented(1, 0));
    Require(ReportsNotImplemented(0, 0));
    Require(ReportsNotImplemented(1, 1));
}

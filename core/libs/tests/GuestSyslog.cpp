#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
void APS5_VABI syslog_nid_postfix(int, const char*, ...);
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const char* path = "guest_syslog_output.txt";
    Require(std::freopen(path, "w", stderr) != nullptr);
    syslog_nid_postfix(6, "plain message");
    syslog_nid_postfix(3, "formatted %s %d", "value", 42);
    std::fflush(stderr);
    std::fclose(stderr);
    FILE* input = std::fopen(path, "r");
    Require(input != nullptr);
    std::string actual;
    char chunk[256];
    std::size_t count;
    while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) actual.append(chunk, count);
    std::fclose(input);
    std::remove(path);
    Require(actual == "plain messageformatted value 42");
    bool threw = false;
    try {
        syslog_nid_postfix(6, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

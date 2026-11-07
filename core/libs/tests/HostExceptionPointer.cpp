#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::exception_ptr caught;
    try {
        throw std::runtime_error("host failure");
    } catch (...) {
        caught = std::current_exception();
    }
    Require(caught != nullptr);
    try {
        std::rethrow_exception(caught);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "host failure");
        return 0;
    }
    return 1;
}

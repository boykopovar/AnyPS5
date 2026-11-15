#include "prx/libkernel/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(expectedName) != std::string_view::npos);
        Require(std::string_view(error.what()).find("not implemented") != std::string_view::npos);
        return;
    }
    std::abort();
}

} // namespace

int main() {
    RequireThrowsUnimplemented([] { (void)physhm_open_nid_postfix(); }, "physhm_open");
    RequireThrowsUnimplemented([] { (void)physhm_unlink_nid_postfix(); }, "physhm_unlink");
    RequireThrowsUnimplemented([] { (void)physhm_open_nid_postfix(); }, "physhm_open");
    RequireThrowsUnimplemented([] { (void)physhm_unlink_nid_postfix(); }, "physhm_unlink");
    return 0;
}

#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UText check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

} // namespace

int main() {
    UText* text = nullptr;
    Require(utext_close(nullptr) == nullptr);
    Require(utext_close(text) == nullptr);
    Require(text == nullptr);

    alignas(8) std::uint8_t dummy[16]{};
    RequireThrows([&] { utext_close(reinterpret_cast<UText*>(dummy)); });
    for (const auto byte : dummy) Require(byte == 0);

    std::puts("ICU UText tests passed");
}

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <type_traits>

namespace UtextClose {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&utext_close), UText* (APS5_VABI*)(UText*)>);

void Run() {
    UText* text = nullptr;
    Require(utext_close(nullptr) == nullptr);
    Require(utext_close(text) == nullptr);
    Require(text == nullptr);

    alignas(8) std::uint8_t dummy[16]{};
    RequireThrows([&] { utext_close(reinterpret_cast<UText*>(dummy)); });
    for (const auto byte : dummy) Require(byte == 0);
}

} // namespace UtextClose

void CheckIcuUText() {
    UtextClose::Run();
}

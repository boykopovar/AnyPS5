#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <stdexcept>

extern "C" int APS5_VABI sceAgcGetIsTrinityMode(bool* isTrinityMode);

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected an exception");
    Require(error.what()[0] != '\0', "empty exception message");
}

void VerifyTrinityMode() {
    std::array<bool, 3> flags{true, true, true};
    Require(sceAgcGetIsTrinityMode(&flags[1]) == 0, "Trinity mode query did not return 0");
    Require(!flags[1], "base PS5 GPU reported as Trinity");
    Require(flags[0] && flags[2], "Trinity mode query wrote past its one-byte flag");
}

void VerifyRejections() {
    ExpectFailure([] { sceAgcGetIsTrinityMode(nullptr); });
}

}

namespace {

const Testing::Case trinity{"GetIsTrinityMode_BasePs5_ReportsFalseWithoutOverrun", [] {
    VerifyTrinityMode();
}};

const Testing::Case rejections{"GetIsTrinityMode_NullOutput_Throws", [] {
    VerifyRejections();
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

extern "C" std::uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(std::uint32_t operation);
extern "C" std::uint32_t* APS5_VABI sceAgcDcbContextStateOp_0100(CommandBuffer* buf, std::uint32_t operation);

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected an exception");
    Require(error.what()[0] != '\0', "empty exception message");
}

void VerifySizes() {
    const std::array<std::uint64_t, 4> sizes{20, 108, 108, 128};
    for (std::uint32_t operation = 0; operation < sizes.size(); ++operation) {
        Require(sceAgcDcbContextStateOpGetSize(operation) == sizes[operation], "context state size mismatch");
        std::array<std::uint32_t, 64> words{};
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(),
                             nullptr, nullptr, 0};
        Require(sceAgcDcbContextStateOp_0100(&buffer, operation) == words.data(), "context state op did not start at the cursor");
        const auto written = static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * sizeof(std::uint32_t);
        Require(written == sizes[operation], "context state size differs from the written packets");
    }
}

void VerifyRejections() {
    ExpectFailure([] { sceAgcDcbContextStateOpGetSize(4); });
    ExpectFailure([] { sceAgcDcbContextStateOpGetSize(0xffffffffu); });
}

}

namespace {

const Testing::Case sizes{"ContextStateOpGetSize_EveryOperation_MatchesWrittenBytes", [] {
    VerifySizes();
}};

const Testing::Case rejections{"ContextStateOpGetSize_UnknownOperation_Throws", [] {
    VerifyRejections();
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}

#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/RegistrationPreparationBudget.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        using AgcDriver::DriverDetail::RegistrationPreparationCommitBudgetBytes;
        using AgcDriver::DriverDetail::RegistrationPreparationAllowed;
        constexpr std::uint64_t GiB = 1024ull * 1024 * 1024;
        constexpr std::uint64_t defaultBudget = 8ull * GiB;
        Require(RegistrationPreparationCommitBudgetBytes(nullptr, defaultBudget) == defaultBudget, "an unset budget should preserve the host-derived default");
        Require(RegistrationPreparationCommitBudgetBytes("0", defaultBudget) == 0, "zero GiB should disable registration preparation");
        Require(RegistrationPreparationCommitBudgetBytes("0.5", defaultBudget) == GiB / 2, "fractional GiB should convert to bytes");
        Require(RegistrationPreparationCommitBudgetBytes("2", defaultBudget) == GiB * 2, "whole GiB should convert to bytes");
        for (const auto* invalid : {"", "-1", "1x", "inf", "nan", "1e1000"}) {
            bool rejected = false;
            try {
                static_cast<void>(RegistrationPreparationCommitBudgetBytes(invalid, defaultBudget));
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Require(rejected, "invalid budget text should be rejected");
        }
        Require(RegistrationPreparationAllowed(0, 1), "preparation should be allowed below the budget");
        Require(RegistrationPreparationAllowed(1023, 1024), "preparation should be allowed immediately below the budget");
        Require(!RegistrationPreparationAllowed(1024, 1024), "preparation should defer at the budget");
        Require(!RegistrationPreparationAllowed(1025, 1024), "preparation should defer above the budget");
        Require(!RegistrationPreparationAllowed(0, 0), "a zero budget should defer every preparation");
        const auto unlimited = std::numeric_limits<std::uint64_t>::max();
        Require(RegistrationPreparationAllowed(unlimited - 1, unlimited), "the maximum budget should allow values below it");
        Require(!RegistrationPreparationAllowed(unlimited, unlimited), "the maximum budget should defer at its limit");
        std::cout << "registration prepare budget tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

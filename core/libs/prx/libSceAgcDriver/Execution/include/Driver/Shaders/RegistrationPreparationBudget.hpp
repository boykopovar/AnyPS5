#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERS_REGISTRATIONPREPARATIONBUDGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERS_REGISTRATIONPREPARATIONBUDGET_HPP

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

inline std::uint64_t RegistrationPreparationCommitBudgetBytes(const char* text, std::uint64_t defaultBudgetBytes) {
    if (text == nullptr) return defaultBudgetBytes;
    constexpr double GiB = 1024.0 * 1024.0 * 1024.0;
    char* end = nullptr;
    errno = 0;
    const auto value = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(value) || value < 0 || value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max()) / GiB) {
        throw std::runtime_error("APS5_REGISTRATION_PREPARE_COMMIT_GB must be a non-negative number of GiB");
    }
    return static_cast<std::uint64_t>(value * GiB);
}

constexpr bool RegistrationPreparationAllowed(std::uint64_t processPrivateBytes, std::uint64_t commitBudgetBytes) noexcept {
    return processPrivateBytes < commitBudgetBytes;
}

}

#endif

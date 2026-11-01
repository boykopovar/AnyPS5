#include <exception>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>

static bool CatchesConversion() {
    try {
        (void)std::stoi("x");
    } catch (const std::invalid_argument& error) {
        return std::string(error.what()) == "stoi";
    }
    return false;
}

static bool CatchesStreamFailure(const char* path) {
    try {
        std::ofstream file;
        file.exceptions(std::ios::failbit);
        file.open(path);
    } catch (const std::ios_base::failure&) {
        return true;
    }
    return false;
}

static bool CatchesStreamFailureAsException(const char* path) {
    try {
        std::ofstream file;
        file.exceptions(std::ios::failbit);
        file.open(path);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static bool RethrowsStoredStreamFailure(const char* path) {
    std::exception_ptr failure;
    try {
        std::ofstream file;
        file.exceptions(std::ios::failbit);
        file.open(path);
    } catch (...) {
        failure = std::current_exception();
    }
    if (!failure) return false;
    try {
        std::rethrow_exception(failure);
    } catch (const std::ios_base::failure&) {
        return true;
    }
    return false;
}

extern "C" const char* HostLibstdcxxExceptionFailure(const char* path) {
    if (!CatchesConversion()) return "std::stoi failure was not caught as std::invalid_argument";
    if (!CatchesStreamFailure(path)) return "std::ofstream::open failure was not caught as std::ios_base::failure";
    if (!CatchesStreamFailureAsException(path)) return "std::ofstream::open failure was not caught as std::exception";
    if (!RethrowsStoredStreamFailure(path)) return "a stored std::ofstream::open failure was not rethrown";
    return nullptr;
}

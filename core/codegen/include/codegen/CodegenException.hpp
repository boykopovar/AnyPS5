#ifndef CODEGEN_CODEGENEXCEPTION_HPP
#define CODEGEN_CODEGENEXCEPTION_HPP

#include <domain/Types.hpp>
#include <stdexcept>
#include <string>

namespace Codegen {

struct CodegenException : std::runtime_error {
    Domain::FileByteOffset FailureOffset;

    explicit CodegenException(const std::string& message, const Domain::FileByteOffset failureOffset = 0)
        : std::runtime_error(message), FailureOffset(failureOffset) {}
};

}

#endif

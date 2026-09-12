#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_HPP

#include <stdexcept>

#include "general/LogMacros.hpp"
#include "general/VabiMacros.hpp"
#include "general/ExportMacros.hpp"

extern "C" void NotImplemented_nid_no_patch(const char* funcName);

#define APS5_INVALID_ARG_EX throw std::invalid_argument(std::string(__func__) + ": invalid argument")

#endif

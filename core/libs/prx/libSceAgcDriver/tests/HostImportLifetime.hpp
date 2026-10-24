#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_HOSTIMPORTLIFETIME_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_HOSTIMPORTLIFETIME_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

int RunHostImportLifetimeTests(const AgcDriver::Graphics::Context& context);

int RunHostImportMutationRaceTests(const AgcDriver::Graphics::Context& context);

#endif

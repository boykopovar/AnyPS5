#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_VIEWALIASES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_VIEWALIASES_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

namespace AgcDriver::Graphics {
class Recorder;
}

void RunViewAliasTests(const AgcDriver::Graphics::Context& context, AgcDriver::Graphics::Recorder& recorder);

#endif

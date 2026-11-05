#ifndef RELINKER_ANALYSIS_CALLSITERESOLVER_HPP
#define RELINKER_ANALYSIS_CALLSITERESOLVER_HPP

#include <relinker/domain/ICallSiteResolver.hpp>
#include <memory>

namespace Relinker {

std::shared_ptr<ICallSiteResolver> MakeCallSiteResolver();

}

#endif

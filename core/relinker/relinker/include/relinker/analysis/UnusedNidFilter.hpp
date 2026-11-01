#ifndef RELINKER_ANALYSIS_UNUSEDNIDFILTER_HPP
#define RELINKER_ANALYSIS_UNUSEDNIDFILTER_HPP

#include <relinker/domain/IUnusedNidFilter.hpp>
#include <memory>
#include <domain/IProgress.hpp>

namespace Relinker {

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter();
std::shared_ptr<IUnusedNidFilter> MakeStrictUnusedNidFilter(Domain::IProgress& progress);

}

#endif

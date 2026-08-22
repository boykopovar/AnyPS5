#ifndef RELINKER_ANALYSIS_UNUSEDNIDFILTER_HPP
#define RELINKER_ANALYSIS_UNUSEDNIDFILTER_HPP

#include <relinker/domain/IUnusedNidFilter.hpp>
#include <memory>

namespace Relinker {

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter();

}

#endif

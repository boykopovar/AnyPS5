#ifndef RELINKER_PIPELINE_RELINKERPIPELINE_HPP
#define RELINKER_PIPELINE_RELINKERPIPELINE_HPP

#include <relinker/domain/IRelinkerPipeline.hpp>
#include <relinker/domain/IElfReader.hpp>
#include <relinker/domain/ISyscallScanner.hpp>
#include <relinker/domain/ICallSiteResolver.hpp>
#include <relinker/domain/IValidationPolicy.hpp>
#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>
#include <relinker/domain/IUnusedNidFilter.hpp>
#include <memory>

namespace Relinker {

class RelinkerPipeline : public IRelinkerPipeline {
public:
    RelinkerPipeline(std::shared_ptr<IElfReader> elfReader, std::shared_ptr<ISyscallScanner> syscallScanner, std::shared_ptr<ICallSiteResolver> callSiteResolver, std::shared_ptr<IValidationPolicy> validationPolicy, std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder, std::shared_ptr<IUnusedNidFilter> unusedNidFilter, std::uint32_t unusedFilterLevel);

    RelinkResult Relink(const std::vector<std::uint8_t>& sourceElf) override;

private:
    std::shared_ptr<IElfReader> _elfReader;
    std::shared_ptr<ISyscallScanner> _syscallScanner;
    std::shared_ptr<ICallSiteResolver> _callSiteResolver;
    std::shared_ptr<IValidationPolicy> _validationPolicy;
    std::shared_ptr<ISysVDynamicSectionBuilder> _dynamicSectionBuilder;
    std::shared_ptr<IUnusedNidFilter> _unusedNidFilter;
    std::uint32_t unusedFilterLevel;

    static std::string _relocationTypeName(std::uint32_t type);
};

}

#endif

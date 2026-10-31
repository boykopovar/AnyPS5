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
#include <optional>
#include <vector>
#include <domain/IProgress.hpp>

namespace Relinker {

class RelinkerPipeline : public IRelinkerPipeline {
public:
    RelinkerPipeline(std::shared_ptr<IElfReader> elfReader, std::shared_ptr<ISyscallScanner> syscallScanner, std::shared_ptr<ICallSiteResolver> callSiteResolver, std::shared_ptr<IValidationPolicy> validationPolicy, std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder, std::shared_ptr<IUnusedNidFilter> unusedNidFilter, std::uint32_t unusedFilterLevel, Domain::IProgress& progress, std::optional<std::vector<CodeRange>> dataTables = std::nullopt);

    RelinkResult Relink() override;

private:
    std::shared_ptr<IElfReader> elfReader;
    std::shared_ptr<ISyscallScanner> syscallScanner;
    std::shared_ptr<ICallSiteResolver> callSiteResolver;
    std::shared_ptr<IValidationPolicy> validationPolicy;
    std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder;
    std::shared_ptr<IUnusedNidFilter> unusedNidFilter;
    std::uint32_t unusedFilterLevel;
    Domain::IProgress& progress;
    std::optional<std::vector<CodeRange>> knownDataTables;

    static std::string relocationTypeName(std::uint32_t type);
};

}

#endif

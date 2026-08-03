#ifndef RELINKER_PIPELINE_RELINKERPIPELINE_HPP
#define RELINKER_PIPELINE_RELINKERPIPELINE_HPP

#include <relinker/domain/IRelinkerPipeline.hpp>
#include <relinker/domain/IElfReader.hpp>
#include <relinker/domain/ISceDynlibParser.hpp>
#include <relinker/domain/ISdkRevisionProfile.hpp>
#include <relinker/domain/ISyscallScanner.hpp>
#include <relinker/domain/ICallSiteResolver.hpp>
#include <relinker/domain/IValidationPolicy.hpp>
#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>
#include <memory>

namespace Relinker {

class RelinkerPipeline : public IRelinkerPipeline {
public:
    RelinkerPipeline(
        std::shared_ptr<IElfReader> elfReader,
        std::shared_ptr<ISceDynlibParser> dynlibParser,
        std::shared_ptr<ISyscallScanner> syscallScanner,
        std::shared_ptr<ICallSiteResolver> callSiteResolver,
        std::shared_ptr<IValidationPolicy> validationPolicy,
        std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder
    );

    RelinkResult Relink(const std::vector<std::uint8_t>& sourceElf) override;

private:
    std::shared_ptr<IElfReader> _elfReader;
    std::shared_ptr<ISceDynlibParser> _dynlibParser;
    std::shared_ptr<ISyscallScanner> _syscallScanner;
    std::shared_ptr<ICallSiteResolver> _callSiteResolver;
    std::shared_ptr<IValidationPolicy> _validationPolicy;
    std::shared_ptr<ISysVDynamicSectionBuilder> _dynamicSectionBuilder;

    static constexpr std::uint32_t PT_LOAD = 1;
    static constexpr std::uint32_t PT_DYNAMIC = 2;
    static constexpr std::uint32_t PT_SCE_DYNLIBDATA = 0x61000000;
    static constexpr std::uint32_t PF_X = 0x1;

    static constexpr std::int64_t DT_PLTGOT = 0x00000003;
    static constexpr std::int64_t DT_OS_PLTGOT = 0x61000027;
    static constexpr std::int64_t DT_PLTRELSZ = 0x00000002;
    static constexpr std::int64_t DT_OS_PLTRELSZ = 0x6100002d;
    static constexpr std::int64_t DT_NEEDED = 0x00000001;
    static constexpr std::int64_t DT_STRTAB = 0x00000005;
    static constexpr std::int64_t DT_STRSZ = 0x0000000a;
    static constexpr std::int64_t DT_RELA = 0x00000007;
    static constexpr std::int64_t DT_RELASZ = 0x00000008;
    static constexpr std::int64_t DT_JMPREL = 0x61000029;
    static constexpr std::int64_t DT_SYSV_JMPREL = 0x00000017;
    static constexpr std::int64_t DT_PLTRELSZ_SYSV = 0x00000002;
    static constexpr std::int64_t DT_SYMTAB = 0x00000006;

    static std::string _relocationTypeName(std::uint32_t type);
};

}

#endif

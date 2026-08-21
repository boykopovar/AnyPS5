#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include <domain/Types.hpp>

namespace Elfpatcher::Windows {

WindowsPePatcher::WindowsPePatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter))
{
}

std::vector<std::uint8_t> WindowsPePatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynamicSection,
    std::uint64_t originalPltGotVaddr,
    const std::string& runPath)
{
    throw Domain::RelinkerException("Not implemented");
}

}

#include <compiler/ElfBinaryParser.hpp>
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Error.h>
#include <optional>
#include <stdexcept>

namespace Aot::Compiler {

template<typename TValue>
static TValue Require(llvm::Expected<TValue> result) {
    if (!result) {
        throw std::runtime_error(llvm::toString(result.takeError()));
    }
    return std::move(*result);
}

ElfFunction ElfBinaryParser::ReadFunction(const std::filesystem::path& path, std::string_view name) {
    if (name.empty()) {
        throw std::runtime_error("Select a named ELF function");
    }
    auto binary = Require(llvm::object::ObjectFile::createObjectFile(path.string()));
    auto* elf64 = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary.getBinary());
    const llvm::object::ELFObjectFileBase* elf = elf64;
    if (!elf || elf->getEMachine() != llvm::ELF::EM_X86_64 ||
        (elf->getEType() != llvm::ELF::ET_REL && elf->getEType() != llvm::ELF::ET_EXEC &&
         elf->getEType() != llvm::ELF::ET_DYN)) {
        throw std::runtime_error("Input must be a little-endian x86-64 ELF object or executable");
    }
    const auto& header = elf64->getELFFile().getHeader();
    if (header.e_ident[llvm::ELF::EI_VERSION] != llvm::ELF::EV_CURRENT ||
        header.e_version != llvm::ELF::EV_CURRENT || header.e_ehsize != sizeof(header)) {
        throw std::runtime_error("Invalid ELF header version or size");
    }
    for (const auto& section : elf->sections()) {
        if (section.relocation_begin() != section.relocation_end()) {
            throw std::runtime_error("ELF relocations are not supported");
        }
    }
    std::optional<ElfFunction> result;
    for (const auto& symbol : elf->symbols()) {
        if (Require(symbol.getName()) != llvm::StringRef(name)) {
            continue;
        }
        if (result) {
            throw std::runtime_error("Selected function has multiple symbol definitions");
        }
        if (llvm::object::ELFSymbolRef(symbol).getELFType() != llvm::ELF::STT_FUNC) {
            throw std::runtime_error("Selected symbol is not a function");
        }
        auto section = Require(symbol.getSection());
        if (section == elf->section_end() || !section->isText()) {
            throw std::runtime_error("Selected function is not defined in executable code");
        }
        auto contents = Require(section->getContents());
        const uint64_t value = Require(symbol.getValue());
        const uint64_t base = elf->getEType() == llvm::ELF::ET_REL ? 0 : section->getAddress();
        const uint64_t size = llvm::object::ELFSymbolRef(symbol).getSize();
        if (!size || value < base || value - base > contents.size() ||
            size > contents.size() - (value - base)) {
            throw std::runtime_error("Selected function has an empty or invalid symbol extent");
        }
        auto bytes = contents.substr(value - base, size);
        result = ElfFunction{std::string(name), std::vector<uint8_t>(bytes.begin(), bytes.end())};
    }
    if (!result) {
        throw std::runtime_error("Selected function was not found in the ELF symbol table");
    }
    return std::move(*result);
}

} // namespace Aot::Compiler

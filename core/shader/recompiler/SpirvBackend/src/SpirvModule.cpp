#include "SpirvBackend/SpirvModule.hpp"

#include <algorithm>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>

namespace ShaderRecompiler {
namespace {

constexpr std::size_t InitialSpirvSectionReserve = 4096;
constexpr std::size_t InitialSpirvFunctionSectionReserve = 32768;

void AppendInstructionWords(std::vector<std::uint32_t>& section, std::span<const std::uint32_t> words) {
    if (words.empty()) {
        return;
    }
    const auto opcode = words[0];
    const auto wordCount = static_cast<std::uint32_t>(words.size());
    section.push_back((wordCount << spv::WordCountShift) | opcode);
    section.insert(section.end(), words.begin() + 1, words.end());
}

}

SpirvModule::SpirvModule(std::uint32_t version) : version(version) {
    debug.reserve(InitialSpirvSectionReserve);
    annotations.reserve(InitialSpirvSectionReserve);
    declarations.reserve(InitialSpirvSectionReserve);
    functionInstructions.reserve(InitialSpirvFunctionSectionReserve);
}

std::uint32_t SpirvModule::AllocateId() {
    return nextId++;
}

void SpirvModule::EmitCapability(std::uint32_t capability) {
    if (requiredCapabilities.insert(capability).second) {
        appendInstruction(capabilities, spv::OpCapability, capability);
    }
}

void SpirvModule::EmitExtension(const std::string& extensionName) {
    if (requiredExtensions.insert(extensionName).second) {
        std::vector<std::uint32_t> operands;
        appendString(operands, extensionName);
        appendInstruction(extensions, spv::OpExtension, operands);
    }
}

void SpirvModule::EmitEntryPoint(std::uint32_t executionModel, std::uint32_t entryPointId, const std::string& entryPointName, const std::vector<std::uint32_t>& interfaceIds) {
    std::vector<std::uint32_t> operands;
    appendOperands(operands, executionModel, entryPointId);
    appendString(operands, entryPointName);
    operands.insert(operands.end(), interfaceIds.begin(), interfaceIds.end());
    if (version >= 0x00010400u) {
        const auto scanForVariables = [&](const std::vector<std::uint32_t>& section) {
            for (std::size_t offset = 0; offset < section.size();) {
                const auto count = section[offset] >> spv::WordCountShift;
                if ((section[offset] & spv::OpCodeMask) == spv::OpVariable) {
                    const auto id = section[offset + 2u];
                    if (std::find(interfaceIds.begin(), interfaceIds.end(), id) == interfaceIds.end()) {
                        operands.push_back(id);
                    }
                }
                offset += count;
            }
        };
        scanForVariables(declarations);
        scanForVariables(globalVariables);
    }
    appendInstruction(entryPoints, spv::OpEntryPoint, operands);
}

void SpirvModule::EmitTypeDeclaration(std::vector<std::uint32_t> words) {
    AppendInstructionWords(typeDeclarations, words);
}

void SpirvModule::EmitGlobalVariable(std::vector<std::uint32_t> words) {
    AppendInstructionWords(globalVariables, words);
}

void SpirvModule::EmitFunctionInstruction(std::vector<std::uint32_t> words) {
    AppendInstructionWords(functionInstructions, words);
}

std::vector<std::uint32_t> SpirvModule::Finalize() const {
    if (unpatchedPhiIncomings != 0) {
        throw std::runtime_error("SpirvModule::Finalize called with unpatched OpPhi incoming pairs");
    }
    if (inHelperFunction) throw std::runtime_error("SpirvModule::Finalize called inside a helper function");
    std::vector<std::uint32_t> module;
    module.reserve(5u + capabilities.size() + extensions.size() + extInstImports.size() + memoryModel.size() + entryPoints.size() + executionModes.size() + debug.size() + annotations.size() + typeDeclarations.size() + declarations.size() + globalVariables.size() + functionInstructions.size() + helperFunctionInstructions.size());
    module.push_back(spv::MagicNumber);
    module.push_back(version);
    module.push_back(0u);
    module.push_back(nextId);
    module.push_back(0u);
    module.insert(module.end(), capabilities.begin(), capabilities.end());
    module.insert(module.end(), extensions.begin(), extensions.end());
    module.insert(module.end(), extInstImports.begin(), extInstImports.end());
    module.insert(module.end(), memoryModel.begin(), memoryModel.end());
    module.insert(module.end(), entryPoints.begin(), entryPoints.end());
    module.insert(module.end(), executionModes.begin(), executionModes.end());
    module.insert(module.end(), debug.begin(), debug.end());
    module.insert(module.end(), annotations.begin(), annotations.end());
    module.insert(module.end(), typeDeclarations.begin(), typeDeclarations.end());
    module.insert(module.end(), declarations.begin(), declarations.end());
    module.insert(module.end(), globalVariables.begin(), globalVariables.end());
    module.insert(module.end(), functionInstructions.begin(), functionInstructions.end());
    module.insert(module.end(), helperFunctionInstructions.begin(), helperFunctionInstructions.end());
    return module;
}

void SpirvModule::BeginHelperFunction() {
    if (inHelperFunction) throw std::runtime_error("SpirvModule helper functions cannot nest");
    std::swap(functionInstructions, helperFunctionInstructions);
    inHelperFunction = true;
}

void SpirvModule::EndHelperFunction() {
    if (!inHelperFunction) throw std::runtime_error("SpirvModule::EndHelperFunction without a helper function");
    std::swap(functionInstructions, helperFunctionInstructions);
    inHelperFunction = false;
}

void SpirvModule::RequireVersion(std::uint32_t version) {
    this->version = std::max(this->version, version);
}

std::uint32_t SpirvModule::Import(const std::string& name) {
    if (const auto it = importIds.find(name); it != importIds.end()) {
        return it->second;
    }
    const auto id = AllocateId();
    importIds.emplace(name, id);
    std::vector<std::uint32_t> operands = {id};
    appendString(operands, name);
    appendInstruction(extInstImports, spv::OpExtInstImport, operands);
    return id;
}

std::uint32_t SpirvModule::declareType(std::uint32_t opcode, std::vector<std::uint32_t> key) {
    if (const auto it = declarationIds.find(key); it != declarationIds.end()) {
        return it->second;
    }
    const auto id = AllocateId();
    appendInstruction(declarations, opcode, id, std::span<const std::uint32_t>(key).subspan(2));
    declarationIds.emplace(std::move(key), id);
    return id;
}

std::uint32_t SpirvModule::declareDecoratedType(std::uint32_t opcode, std::vector<std::uint32_t> key, std::initializer_list<SpirvTypeAnnotation> annotationList) {
    if (annotationList.size() == 0) {
        return declareType(opcode, std::move(key));
    }
    const auto operandCount = key[1];
    key.push_back(static_cast<std::uint32_t>(annotationList.size()));
    for (const auto& annotation : annotationList) {
        appendOperand(key, annotation.opcode);
        key.push_back(static_cast<std::uint32_t>(annotation.operands.size()));
        key.insert(key.end(), annotation.operands.begin(), annotation.operands.end());
    }
    if (const auto it = declarationIds.find(key); it != declarationIds.end()) {
        return it->second;
    }
    const auto id = AllocateId();
    appendInstruction(declarations, opcode, id, std::span<const std::uint32_t>(key).subspan(2, operandCount));
    declarationIds.emplace(std::move(key), id);
    for (const auto& annotation : annotationList) {
        appendInstruction(annotations, annotation.opcode, id, annotation.operands);
    }
    return id;
}

std::uint32_t SpirvModule::SpecializationConstant(std::uint32_t type, std::uint32_t constantId, std::uint32_t defaultValue) {
    if (const auto found = specializationIds.find(constantId); found != specializationIds.end()) return found->second;
    const auto id = AllocateId();
    appendInstruction(declarations, spv::OpSpecConstant, type, id, defaultValue);
    AddAnnotation(spv::OpDecorate, id, spv::DecorationSpecId, constantId);
    specializationIds.emplace(constantId, id);
    return id;
}

std::uint32_t SpirvModule::declareConstant(std::uint32_t opcode, std::vector<std::uint32_t> key) {
    if (const auto it = declarationIds.find(key); it != declarationIds.end()) {
        return it->second;
    }
    const auto id = AllocateId();
    appendInstruction(declarations, opcode, key[1], id, std::span<const std::uint32_t>(key).subspan(2));
    declarationIds.emplace(std::move(key), id);
    return id;
}

std::uint32_t SpirvModule::DefineGlobalVariable(std::uint32_t pointerType, std::uint32_t storageClass) {
    const auto id = AllocateId();
    DefineGlobalVariable(id, pointerType, storageClass);
    return id;
}

void SpirvModule::DefineGlobalVariable(std::uint32_t id, std::uint32_t pointerType, std::uint32_t storageClass) {
    appendInstruction(declarations, spv::OpVariable, pointerType, id, storageClass);
}

std::uint32_t SpirvModule::DefineInitializedGlobalVariable(std::uint32_t pointerType, std::uint32_t storageClass, std::uint32_t initializer) {
    const auto id = AllocateId();
    appendInstruction(declarations, spv::OpVariable, pointerType, id, storageClass, initializer);
    return id;
}

void SpirvModule::AddMemoryModel(std::uint32_t addressingModel, std::uint32_t memoryModel) {
    appendInstruction(this->memoryModel, spv::OpMemoryModel, addressingModel, memoryModel);
}

void SpirvModule::AddName(std::uint32_t target, const std::string& name) {
    std::vector<std::uint32_t> operands = {target};
    appendString(operands, name);
    appendInstruction(debug, spv::OpName, operands);
}

void SpirvModule::AddFunction(std::span<const std::uint32_t> words) {
    AppendInstructionWords(functionInstructions, words);
}

void SpirvModule::appendString(std::vector<std::uint32_t>& words, const std::string& text) {
    const auto length = text.size();
    const auto wordCount = (length + 1u + 3u) / 4u;
    for (std::size_t i = 0; i < wordCount; i++) {
        std::uint32_t word = 0;
        for (std::size_t byte = 0; byte < 4; byte++) {
            const auto index = i * 4u + byte;
            if (index < length) {
                word |= static_cast<std::uint32_t>(static_cast<unsigned char>(text[index])) << (byte * 8u);
            }
        }
        words.push_back(word);
    }
}

SpirvDeferredPhi SpirvModule::AddDeferredPhi(std::uint32_t type, std::uint32_t result, std::size_t incomingCount) {
    std::vector<std::uint32_t> words = {spv::OpPhi, type, result};
    words.resize(words.size() + incomingCount * 2u);
    const SpirvDeferredPhi phi {functionInstructions.size()};
    AddFunction(std::span<const std::uint32_t>(words));
    unpatchedPhiIncomings += incomingCount;
    return phi;
}

void SpirvModule::PatchDeferredPhi(SpirvDeferredPhi phi, std::size_t incoming, std::uint32_t value, std::uint32_t parent) {
    const auto incomingCount = ((functionInstructions.at(phi.wordOffset) >> spv::WordCountShift) - 3u) / 2u;
    if (incoming >= incomingCount || value == 0u || parent == 0u) {
        throw std::runtime_error("SpirvModule::PatchDeferredPhi received an invalid incoming index or a zero id");
    }
    const auto valueWord = phi.wordOffset + 3u + incoming * 2u;
    const auto parentWord = valueWord + 1u;
    if (functionInstructions.at(valueWord) != 0u || functionInstructions.at(parentWord) != 0u) {
        throw std::runtime_error("SpirvModule::PatchDeferredPhi target incoming pair is already patched");
    }
    functionInstructions[valueWord] = value;
    functionInstructions[parentWord] = parent;
    unpatchedPhiIncomings--;
}

}

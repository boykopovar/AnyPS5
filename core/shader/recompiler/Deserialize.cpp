#include "Recompiler.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Optimization/DeadCodeEliminator.hpp"
#include "Optimization/ReadLaneEliminator.hpp"
#include "Optimization/ResourceTracker.hpp"
#include "Optimization/ShaderInfoCollector.hpp"
#include "Optimization/SrtWalker.hpp"
#include "Optimization/SsaBuilder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"

#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

static std::string ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("failed to open file: ") + path);
    }

    std::ostringstream stream;
    stream << file.rdbuf();

    if (!file.good() && !file.eof()) {
        throw std::runtime_error(std::string("failed to read file: ") + path);
    }

    return stream.str();
}

static ShaderStageKind ToShaderStageKind(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Compute:
        return ShaderStageKind::Compute;
    case ShaderStage::Vertex:
        return ShaderStageKind::Vertex;
    case ShaderStage::TessellationControl:
        return ShaderStageKind::TessellationControl;
    case ShaderStage::TessellationEvaluation:
        return ShaderStageKind::TessellationEvaluation;
    case ShaderStage::Fragment:
        return ShaderStageKind::Pixel;
    case ShaderStage::Local:
        return ShaderStageKind::Local;
    case ShaderStage::Mesh:
        return ShaderStageKind::Mesh;
    case ShaderStage::Geometry:
        throw std::runtime_error("unsupported geometry shader stage");
    }

    throw std::runtime_error("invalid shader stage");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: Deserialize <payload.b64>\n";
        return 1;
    }

    try {
        std::string b64 = ReadFile(argv[1]);

        while (!b64.empty() && (b64.back() == '\n' || b64.back() == '\r' || b64.back() == ' ' || b64.back() == '\t')) {
            b64.pop_back();
        }

        if (b64.empty()) {
            throw std::runtime_error("payload is empty");
        }

        RequestSerializer serializer;
        const DeserializedRequest req = serializer.Deserialize(b64);
        const auto& request = req.request;

        std::cout << "shader code words: " << request.shader.code.size() << "\n";
        std::cout << "shader stage: " << static_cast<int>(request.shader.stage) << "\n";
        std::cout << "codeAddress: 0x" << std::hex << request.shader.codeAddress << std::dec << "\n";
        std::cout << "header bytes: " << request.shader.header.size() << "\n";
        std::cout << "userData count: " << request.context.userData.size() << "\n";
        std::cout << "waveSize: " << request.context.waveSize << "\n";
        std::cout << "userDataBaseRegister: " << request.context.userDataBaseRegister << "\n";
        std::cout << "memory regions: " << request.context.memory.size() << "\n";

        for (const auto& memory : request.context.memory) {
            std::cout << "region addr=0x" << std::hex << memory.guestAddress << std::dec << " size=" << memory.bytes.size() << "\n";
        }

        for (std::uint32_t i = 0; i < request.context.userData.size(); ++i) {
            std::cout << "userData[" << i << "]=0x" << std::hex << request.context.userData[i] << std::dec << "\n";
        }

        if (request.context.vertex) {
            const auto& vertex = *request.context.vertex;

            std::cout << "vertex resourcesNum=" << vertex.resourcesNum << " fetchAttribReg=" << vertex.fetchAttribReg << " fetchBufferReg=" << vertex.fetchBufferReg << " fetchEmbedded=" << vertex.fetchEmbedded << "\n";

            if (vertex.resources.size() < vertex.resourcesNum) {
                throw std::runtime_error("vertex resources size is smaller than resourcesNum");
            }

            if (vertex.resourcesDst.size() < vertex.resourcesNum) {
                throw std::runtime_error("vertex resourcesDst size is smaller than resourcesNum");
            }

            for (std::uint32_t i = 0; i < vertex.resourcesNum; ++i) {
                const auto& resource = vertex.resources[i];
                const auto& dst = vertex.resourcesDst[i];

                std::cout << "resource[" << i << "] fields=";
                for (const auto field : resource.fields) {
                    std::cout << std::hex << field << " ";
                }

                std::cout << std::dec << "dst registerStart=" << dst.registerStart << " registersNum=" << dst.registersNum << " attrId=" << dst.attrId << " fetchIndex=" << dst.fetchIndex << "\n";
            }
        }

        if (request.context.compute) {
            const auto& compute = *request.context.compute;
            std::cout << "compute numThreads=" << compute.numThreads[0] << "," << compute.numThreads[1] << "," << compute.numThreads[2] << " ldsSizeDwords=" << compute.ldsSizeDwords << "\n";
        }

        RdnaInstructionDecoder decoder;
        const auto decoded = decoder.Decode(request.shader.code);
        const std::string disasm = RdnaProgramToString(decoded);

        {
            std::ofstream output("disasm.txt", std::ios::binary);
            if (!output) {
                throw std::runtime_error("failed to open disasm.txt");
            }

            output << disasm;
            if (!output) {
                throw std::runtime_error("failed to write disasm.txt");
            }
        }

        std::cout << "Disassembly written to disasm.txt (" << disasm.size() << " bytes)\n";

        const ShaderStageKind stageKind = ToShaderStageKind(request.shader.stage);
        const auto inputInfo = BuildShaderStageInputInfo(stageKind, request.context, request.target.subgroupSize);

        GraphBuilder graphBuilder;
        auto cfg = graphBuilder.Build(decoded);

        Structurizer structurizer;
        structurizer.Structurize(cfg);

        TranslateOptions translateOptions{};
        translateOptions.stage = stageKind;
        translateOptions.waveSize = request.context.waveSize;
        translateOptions.userDataBaseRegister = request.context.userDataBaseRegister;
        translateOptions.userDataCount = static_cast<std::uint32_t>(request.context.userData.size());
        translateOptions.inputInfo = inputInfo;

        EmbeddedFetchPlan embeddedFetch;

        if ((stageKind == ShaderStageKind::Vertex || stageKind == ShaderStageKind::Local) && inputInfo.vertex != nullptr && inputInfo.vertex->fetchEmbedded) {
            EmbeddedVertexFetchAnalyzer embeddedFetchAnalyzer;
            embeddedFetch = embeddedFetchAnalyzer.Analyze(decoded, inputInfo.vertex->fetchAttribReg, inputInfo.vertex->fetchBufferReg, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize);
        }

        translateOptions.embeddedFetch = embeddedFetch.loads.empty() ? nullptr : &embeddedFetch;

        std::cout << "embedded fetch loads: " << embeddedFetch.loads.size() << "\n";

        InstructionTranslator translator;
        auto program = translator.Translate(decoded, cfg, translateOptions);

        std::cout << "Translate OK, blocks=" << program.Blocks().size() << "\n";

        SsaBuilder ssaBuilder;
        ssaBuilder.Rewrite(program);
        std::cout << "SSA OK\n";

        ConstantFolder constantFolder;
        DeadCodeEliminator deadCodeEliminator;

        constantFolder.Fold(program);
        ResolveControlFlowIdentities(program);
        deadCodeEliminator.RemoveIdentities(program);
        deadCodeEliminator.Eliminate(program);
        std::cout << "Fold/DCE pass 1 OK\n";

        ReadLaneEliminator readLaneEliminator;
        const auto readLaneStats = readLaneEliminator.Eliminate(program, translateOptions.waveSize);

        std::cout << "ReadLaneEliminator rewrote " << readLaneStats.rewrittenReads << "\n";

        if (readLaneStats.rewrittenReads != 0u) {
            constantFolder.Fold(program);
            ResolveControlFlowIdentities(program);
            deadCodeEliminator.RemoveIdentities(program);
            deadCodeEliminator.Eliminate(program);
        }

        SrtWalker srtWalker;
        srtWalker.BuildPlan(program);
        std::cout << "SrtWalker::BuildPlan OK\n";

        deadCodeEliminator.Eliminate(program);
        std::cout << "DCE after SRT OK\n";

        ResourceTracker resourceTracker;
        resourceTracker.Track(program);
        std::cout << "ResourceTracker::Track OK\n";

        deadCodeEliminator.Eliminate(program);
        std::cout << "DCE after ResourceTracker OK\n";

        ShaderInfoCollector shaderInfoCollector;
        shaderInfoCollector.Collect(program);
        std::cout << "ShaderInfoCollector::Collect OK\n";

        BindingAllocator bindingAllocator;
        const auto bindings = bindingAllocator.Allocate(program, request.layout);

        std::cout << "BindingAllocator::Allocate OK, bindings=" << bindings.bindings.size() << "\n";
        std::cout << "ALL STAGES OK\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "FAILED: unknown exception\n";
        return 2;
    }
}

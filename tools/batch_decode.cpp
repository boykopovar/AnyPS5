#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ShaderRecompiler;

static std::string HexWord(std::uint32_t word) {
    char b[16];
    snprintf(b, sizeof(b), "%08x", word);
    return b;
}

static bool EndsFallthrough(RdnaOpcode op) {
    return op == RdnaOpcode::SBranch || op == RdnaOpcode::SEndpgm || op == RdnaOpcode::SCodeEnd;
}

static bool TargetsCode(const RdnaInstruction& instruction) {
    return IsDirectBranchOpcode(instruction.op) || instruction.op == RdnaOpcode::SCallB64;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: batch_decode <shader_extract_dir> [limit]\n";
        return 1;
    }
    const fs::path root(argv[1]);
    const std::size_t limit = argc > 2 ? std::stoull(argv[2]) : ~0ULL;

    std::size_t files = 0, clean = 0, gapFiles = 0, structuralFails = 0;
    std::size_t instructions = 0, flaggedUnsupported = 0, deadSkipped = 0, reachableGaps = 0, targetedGaps = 0;
    std::map<std::string, std::uint64_t> gapForms;
    std::map<std::string, std::uint64_t> reasons;
    std::map<std::string, std::uint64_t> structuralReasons;
    std::map<std::string, std::string> examples;

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (files >= limit) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".code") continue;
        std::ifstream in(entry.path(), std::ios::binary);
        std::vector<char> raw((std::istreambuf_iterator<char>(in)), {});
        const std::size_t words = raw.size() / 4;
        std::vector<std::uint32_t> code(words);
        std::memcpy(code.data(), raw.data(), words * 4);
        ++files;

        std::vector<RdnaInstruction> program;
        program.reserve(words);
        std::vector<std::pair<std::uint32_t, std::exception_ptr>> skipped;
        std::vector<std::pair<std::uint32_t, std::exception_ptr>> gaps;
        std::uint32_t furthestBranchTarget = 0;
        bool fallthroughReachable = true;
        bool boundaryFail = false;
        bool fileHasGap = false;

        std::uint32_t wordIndex = 0;
        while (wordIndex < code.size()) {
            const std::uint32_t pc = wordIndex * 4;
            RdnaInstruction instruction;
            std::exception_ptr error;
            try {
                instruction = DecodeRdnaEncodedInstruction(pc, code, wordIndex);
            } catch (...) {
                error = std::current_exception();
            }
            if (error) {
                if (fallthroughReachable) {
                    ++reachableGaps;
                    fileHasGap = true;
                    gaps.push_back({pc, error});
                    const std::uint32_t word = code[wordIndex];
                    std::string what;
                    try { std::rethrow_exception(error); } catch (const std::exception& e) { what = e.what(); }
                    const std::string key = what.substr(0, what.find(" at program counter")) + " word=0x" + HexWord(word);
                    ++gapForms[key];
                    const std::string shortKey = what.substr(0, what.find(" at program counter"));
                    if (!examples.count(shortKey)) examples[shortKey] = entry.path().filename().string() + " pc=0x" + HexWord(pc);
                } else {
                    ++deadSkipped;
                    skipped.push_back({pc, error});
                }
                ++wordIndex;
                continue;
            }
            program.push_back(instruction);
            wordIndex += instruction.wordCount;
            fallthroughReachable = !EndsFallthrough(instruction.op);
            if (TargetsCode(instruction)) {
                const std::uint32_t targetIndex = instruction.branchTarget / 4;
                if (targetIndex >= code.size()) {
                    boundaryFail = true;
                    ++structuralReasons["branch target out of bounds"];
                    break;
                }
                furthestBranchTarget = std::max(furthestBranchTarget, targetIndex);
            }
            if ((instruction.op == RdnaOpcode::SEndpgm || instruction.op == RdnaOpcode::SCodeEnd) && furthestBranchTarget < wordIndex) {
                break;
            }
        }
        if (!boundaryFail && wordIndex >= code.size() && !program.empty() && !EndsFallthrough(program.back().op)) {
            boundaryFail = true;
            ++structuralReasons["reached code boundary before s_endpgm"];
        }
        if (!boundaryFail) {
            for (const auto& instruction : program) {
                if (!TargetsCode(instruction)) continue;
                for (const auto& [pc, error] : skipped) {
                    if (instruction.branchTarget == pc) {
                        ++targetedGaps;
                        fileHasGap = true;
                        std::string what;
                        try { std::rethrow_exception(error); } catch (const std::exception& e) { what = e.what(); }
                        ++gapForms["(branch-targeted) " + what.substr(0, what.find(" at program counter"))];
                    }
                }
            }
        }
        if (boundaryFail) {
            ++structuralFails;
            continue;
        }
        if (fileHasGap) ++gapFiles; else ++clean;
        for (const auto& instruction : program) {
            ++instructions;
            if (!instruction.unsupportedReason.empty()) {
                ++flaggedUnsupported;
                ++reasons[std::string(instruction.unsupportedReason)];
            }
        }
    }

    std::cout << "files: " << files << "  clean: " << clean << "  with-gaps: " << gapFiles << "  structural-fail: " << structuralFails << "\n";
    std::cout << "instructions: " << instructions << "  flagged-unsupported: " << flaggedUnsupported << "\n";
    std::cout << "reachable gaps: " << reachableGaps << "  branch-targeted gaps: " << targetedGaps << "  dead-code skips: " << deadSkipped << "\n\n";
    for (const auto& [gap, where] : examples) std::cout << "EXAMPLE " << gap << " in " << where << "\n";
    std::vector<std::pair<std::string, std::uint64_t>> sg(gapForms.begin(), gapForms.end());
    std::sort(sg.begin(), sg.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "== reachable decode gaps ==\n";
    for (const auto& [form, count] : sg) std::cout << count << "  " << form << "\n";
    std::vector<std::pair<std::string, std::uint64_t>> sr(reasons.begin(), reasons.end());
    std::sort(sr.begin(), sr.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "== flagged-unsupported reasons ==\n";
    for (const auto& [reason, count] : sr) std::cout << count << "  " << reason << "\n";
    std::vector<std::pair<std::string, std::uint64_t>> ss(structuralReasons.begin(), structuralReasons.end());
    std::sort(ss.begin(), ss.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "== structural failures ==\n";
    for (const auto& [reason, count] : ss) std::cout << count << "  " << reason << "\n";
    return 0;
}

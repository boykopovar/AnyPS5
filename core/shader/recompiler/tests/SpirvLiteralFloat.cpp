#include <spirv-tools/libspirv.hpp>
#include <spirv/unified1/spirv.hpp11>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct ParseState {
    unsigned callbacks = 0;
    bool correct = true;
};

spv_result_t ReadInstruction(void* data, const spv_parsed_instruction_t* instruction) {
    auto& state = *static_cast<ParseState*>(data);
    ++state.callbacks;
    if (instruction->opcode != static_cast<std::uint16_t>(spv::Op::OpDecorate) || instruction->num_operands != 3) {
        state.correct = false;
        return SPV_SUCCESS;
    }
    const auto& operand = instruction->operands[2];
    if (operand.type != SPV_OPERAND_TYPE_LITERAL_FLOAT || operand.number_kind != SPV_NUMBER_FLOATING ||
        operand.number_bit_width != 32 || operand.num_words != 1 || operand.fp_encoding != SPV_FP_ENCODING_UNKNOWN) {
        std::fprintf(stderr, "literal-float metadata: type=%d kind=%d width=%u words=%u encoding=%d\n",
                     operand.type, operand.number_kind, operand.number_bit_width, operand.num_words, operand.fp_encoding);
        state.correct = false;
    }
    return SPV_SUCCESS;
}

std::uint32_t Swap(std::uint32_t word) {
    return (word >> 24) | ((word >> 8) & 0x0000ff00u) | ((word << 8) & 0x00ff0000u) | (word << 24);
}

}

int main() {
    constexpr std::array<std::uint32_t, 8> literals{0, 0x80000000u, 0x3f800000u, 0xbf800000u,
                                                 0x20202020u, 0x7f7fffffu, 1, 0x80800000u};
    spvtools::Context context(SPV_ENV_UNIVERSAL_1_6);
    spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_6);
    unsigned failed = 0;
    unsigned roundTrips = 0;
    unsigned controls = 0;
    std::vector<std::uint32_t> control;
    std::string controlText;
    if (tools.Assemble("%1 = OpTypeFloat 32\n%2 = OpConstant %1 1\n", &control, SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS) &&
        tools.Disassemble(control, &controlText, SPV_BINARY_TO_TEXT_OPTION_NO_HEADER)) ++controls;
    else ++failed;
    for (const auto literal : literals) {
        const std::array<std::uint32_t, 9> words{spv::MagicNumber, 0x00010600u, 0, 2, 0,
            (4u << 16) | static_cast<std::uint32_t>(spv::Op::OpDecorate), 1,
            static_cast<std::uint32_t>(spv::Decoration::FPMaxErrorDecorationINTEL), literal};
        for (const bool swapped : {false, true}) {
            auto input = words;
            if (swapped) for (auto& word : input) word = Swap(word);
            if (literal == 0 && !swapped) {
                ParseState truncated;
                if (spvBinaryParse(context.CContext(), &truncated, input.data(), input.size() - 1,
                                   nullptr, ReadInstruction, nullptr) == SPV_ERROR_INVALID_BINARY && truncated.callbacks == 0) ++controls;
                else ++failed;
            }
            ParseState state;
            const auto result = spvBinaryParse(context.CContext(), &state, input.data(), input.size(),
                                               nullptr, ReadInstruction, nullptr);
            if (result != SPV_SUCCESS || state.callbacks != 1 || !state.correct) ++failed;
            std::string text;
            std::vector<std::uint32_t> output;
            if (!tools.Disassemble(input.data(), input.size(), &text, SPV_BINARY_TO_TEXT_OPTION_NO_HEADER) ||
                !tools.Assemble(text, &output, SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS) ||
                output.size() != words.size() || !std::equal(words.begin() + 5, words.end(), output.begin() + 5)) {
                std::fprintf(stderr, "literal %08x swapped=%d failed round trip: %s\n", literal, swapped, text.c_str());
                ++failed;
            } else ++roundTrips;
        }
    }
    std::printf("SPIRV literal float: 16 parser checks, %u exact round trips, %u controls, %u failures\n", roundTrips, controls, failed);
    return failed == 0 ? 0 : 1;
}

#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

enum class Outcome {
    Accepted,
    Invalid,
    Unsupported,
};

struct Result {
    Outcome outcome;
    std::string message;
};

bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

std::string WithoutLocation(std::string message) {
    for (const char* marker : {" of opcode 0x", " at pc 0x"}) {
        for (auto start = message.find(marker); start != std::string::npos; start = message.find(marker)) {
            auto end = start + std::strlen(marker);
            while (end < message.size() && std::isxdigit(static_cast<unsigned char>(message[end])) != 0) ++end;
            message.erase(start, end - start);
        }
    }
    return message;
}

Result Decode(const std::vector<std::uint32_t>& words) {
    try {
        DecodeRdnaInstruction(0u, words, 0u);
        return {Outcome::Accepted, {}};
    } catch (const std::exception& error) {
        const std::string message = WithoutLocation(error.what());
        const bool unsupported = Contains(message, "not implemented") || Contains(message, "not supported");
        const bool invalid = Contains(message, "invalid") || Contains(message, "reserved") || Contains(message, "truncated");
        return {invalid && !unsupported ? Outcome::Invalid : Outcome::Unsupported, message};
    }
}

constexpr std::uint32_t Source0 = 257u;
constexpr std::uint32_t Source1 = 258u;
constexpr std::uint32_t Source2 = 259u;
constexpr std::uint32_t Destination = 2u;
constexpr std::uint32_t SdwaForm = 0xf9u;
constexpr std::uint32_t DppForm = 0xfau;
constexpr std::uint32_t SdwaNeutral = 1u | (6u << 8u) | (6u << 16u) | (6u << 24u);
constexpr std::uint32_t DestinationSelectMask = 7u << 8u;
constexpr std::uint32_t DestinationUnusedMask = 3u << 11u;
constexpr std::uint32_t SourceZeroSelectMask = 7u << 16u;
constexpr std::uint32_t SourceOneSelectMask = 7u << 24u;
constexpr std::uint32_t VopcSdwaDestinationBits = 0xff00u;
constexpr std::uint32_t DppNeutral = 1u | (0xe4u << 8u) | (0xfu << 24u) | (0xfu << 28u);

using Builder = std::vector<std::uint32_t> (*)(std::uint32_t opcode, std::uint32_t first, std::uint32_t second, std::uint32_t clear);

std::vector<std::uint32_t> Vop3(std::uint32_t opcode, std::uint32_t first, std::uint32_t second, std::uint32_t) {
    return {0xd4000000u | (opcode << 16u) | Destination | first, Source0 | (Source1 << 9u) | (Source2 << 18u) | second};
}

std::vector<std::uint32_t> Vop3p(std::uint32_t opcode, std::uint32_t first, std::uint32_t second, std::uint32_t) {
    return {0xcc000000u | (opcode << 16u) | Destination | first, Source0 | (Source1 << 9u) | (Source2 << 18u) | second};
}

std::vector<std::uint32_t> Vop1Sdwa(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t clear) {
    return {0x7e000000u | (Destination << 17u) | (opcode << 9u) | SdwaForm, (SdwaNeutral & ~clear) | second};
}

std::vector<std::uint32_t> Vop2Sdwa(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t clear) {
    return {(opcode << 25u) | (Destination << 17u) | (3u << 9u) | SdwaForm, (SdwaNeutral & ~clear) | second};
}

std::vector<std::uint32_t> VopcSdwa(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t clear) {
    return {0x7c000000u | (opcode << 17u) | (3u << 9u) | SdwaForm, (SdwaNeutral & ~clear) | second};
}

std::vector<std::uint32_t> Vop1Dpp(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t) {
    return {0x7e000000u | (Destination << 17u) | (opcode << 9u) | DppForm, DppNeutral | second};
}

std::vector<std::uint32_t> Vop2Dpp(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t) {
    return {(opcode << 25u) | (Destination << 17u) | (3u << 9u) | DppForm, DppNeutral | second};
}

std::vector<std::uint32_t> VopcDpp(std::uint32_t opcode, std::uint32_t, std::uint32_t second, std::uint32_t) {
    return {0x7c000000u | (opcode << 17u) | (3u << 9u) | DppForm, DppNeutral | second};
}

struct Variant {
    const char* encoding;
    const char* format;
    std::uint32_t opcodeCount;
    Builder build;
};

constexpr std::array<Variant, 8> Variants{{
    {"VOP3", "VOP3", 1024u, Vop3},
    {"VOP3P", "VOP3P", 128u, Vop3p},
    {"SDWA", "VOP1", 256u, Vop1Sdwa},
    {"SDWA", "VOP2", 62u, Vop2Sdwa},
    {"SDWA", "VOPC", 256u, VopcSdwa},
    {"DPP", "VOP1", 256u, Vop1Dpp},
    {"DPP", "VOP2", 62u, Vop2Dpp},
    {"DPP", "VOPC", 256u, VopcDpp},
}};

struct Case {
    const char* encoding;
    const char* modifier;
    std::string combination;
    std::uint32_t first;
    std::uint32_t second;
    std::uint32_t clear;
    bool needsSecondSource;
};

void Add(std::vector<Case>& cases, const char* encoding, const char* modifier, const std::string& combination, std::uint32_t first, std::uint32_t second, std::uint32_t clear = 0u, bool needsSecondSource = false) {
    cases.push_back({encoding, modifier, combination, first, second, clear, needsSecondSource});
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    for (std::uint32_t source = 0; source < 3u; ++source) {
        Add(cases, "VOP3", "abs", "single", 1u << (8u + source), 0u);
        Add(cases, "VOP3", "neg", "single", 0u, 1u << (29u + source));
    }
    Add(cases, "VOP3", "clamp", "single", 1u << 15u, 0u);
    for (std::uint32_t omod = 1; omod < 4u; ++omod) Add(cases, "VOP3", "omod", "single", 0u, omod << 27u);
    for (std::uint32_t bit = 0; bit < 4u; ++bit) Add(cases, "VOP3", "op_sel", "single", 1u << (11u + bit), 0u);
    Add(cases, "VOP3", "abs", "pair:abs+neg", 1u << 8u, 1u << 29u);
    Add(cases, "VOP3", "abs", "pair:abs+abs", (1u << 8u) | (1u << 9u), 0u);
    Add(cases, "VOP3", "clamp", "pair:clamp+omod", 1u << 15u, 1u << 27u);
    Add(cases, "VOP3", "op_sel", "pair:op_sel+clamp", (1u << 11u) | (1u << 15u), 0u);
    Add(cases, "VOP3", "neg", "pair:neg+omod", 0u, (1u << 29u) | (1u << 27u));
    for (std::uint32_t source = 0; source < 3u; ++source) {
        Add(cases, "VOP3P", "neg_lo", "single", 0u, 1u << (29u + source));
        Add(cases, "VOP3P", "neg_hi", "single", 1u << (8u + source), 0u);
        Add(cases, "VOP3P", "op_sel", "single", 1u << (11u + source), 0u);
    }
    Add(cases, "VOP3P", "op_sel_hi", "single", 0u, 1u << 27u);
    Add(cases, "VOP3P", "op_sel_hi", "single", 0u, 1u << 28u);
    Add(cases, "VOP3P", "op_sel_hi", "single", 1u << 14u, 0u);
    Add(cases, "VOP3P", "clamp", "single", 1u << 15u, 0u);
    Add(cases, "VOP3P", "neg_lo", "pair:neg_lo+neg_hi", 1u << 8u, 1u << 29u);
    Add(cases, "VOP3P", "op_sel", "pair:op_sel+op_sel_hi", 1u << 11u, 1u << 27u);
    Add(cases, "VOP3P", "clamp", "pair:clamp+neg_lo", 1u << 15u, 1u << 29u);
    for (std::uint32_t select = 0; select < 6u; ++select) {
        Add(cases, "SDWA", "dst_sel", "single", 0u, select << 8u, DestinationSelectMask);
        Add(cases, "SDWA", "src0_sel", "single", 0u, select << 16u, SourceZeroSelectMask);
        Add(cases, "SDWA", "src1_sel", "single", 0u, select << 24u, SourceOneSelectMask, true);
    }
    for (std::uint32_t unused = 1; unused < 3u; ++unused) Add(cases, "SDWA", "dst_unused", "single", 0u, unused << 11u);
    Add(cases, "SDWA", "src0_sext", "single", 0u, 1u << 19u);
    Add(cases, "SDWA", "src1_sext", "single", 0u, 1u << 27u, 0u, true);
    Add(cases, "SDWA", "abs", "single", 0u, 1u << 21u);
    Add(cases, "SDWA", "abs", "single", 0u, 1u << 29u, 0u, true);
    Add(cases, "SDWA", "neg", "single", 0u, 1u << 20u);
    Add(cases, "SDWA", "neg", "single", 0u, 1u << 28u, 0u, true);
    Add(cases, "SDWA", "clamp", "single", 0u, 1u << 13u);
    for (std::uint32_t omod = 1; omod < 4u; ++omod) Add(cases, "SDWA", "omod", "single", 0u, omod << 14u);
    for (std::uint32_t select = 0; select < 7u; ++select) {
        for (std::uint32_t unused = 0; unused < 3u; ++unused) {
            if (select == 6u && unused == 0u) continue;
            Add(cases, "SDWA", "dst_sel", "pair:dst_sel+dst_unused", 0u, (select << 8u) | (unused << 11u), DestinationSelectMask | DestinationUnusedMask);
        }
    }
    Add(cases, "SDWA", "clamp", "pair:clamp+src0_sel", 0u, (1u << 13u) | (1u << 16u), SourceZeroSelectMask);
    Add(cases, "SDWA", "clamp", "pair:clamp+dst_sel", 0u, (1u << 13u) | (1u << 8u), DestinationSelectMask);
    Add(cases, "SDWA", "clamp", "pair:clamp+omod", 0u, (1u << 13u) | (1u << 14u));
    Add(cases, "SDWA", "abs", "pair:abs+neg", 0u, (1u << 21u) | (1u << 20u));
    Add(cases, "DPP", "abs", "single", 0u, 1u << 21u);
    Add(cases, "DPP", "neg", "single", 0u, 1u << 20u);
    Add(cases, "DPP", "abs", "single", 0u, 1u << 23u, 0u, true);
    Add(cases, "DPP", "neg", "single", 0u, 1u << 22u, 0u, true);
    Add(cases, "DPP", "abs", "pair:abs+neg", 0u, (1u << 21u) | (1u << 20u));
    return cases;
}

struct Family {
    std::uint32_t total = 0;
    std::uint32_t accepted = 0;
    std::map<std::string, std::set<std::string>> rejected;
    std::map<std::string, std::uint32_t> rejectedCount;
    std::map<std::string, const char*> rejectedKind;
};

struct Baseline {
    std::uint32_t opcodes = 0;
    std::uint32_t accepted = 0;
};

std::string Hex(std::uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%02x", value);
    return buffer;
}

std::string Quote(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

bool SelectorWordsAreBuiltAndAccepted() {
    bool ok = true;
    for (std::uint32_t source = 0; source < 7u; ++source) {
        for (std::uint32_t destination = 0; destination < 7u; ++destination) {
            for (std::uint32_t unused = 0; unused < 3u; ++unused) {
                const auto words = Vop1Sdwa(1u, 0u, (source << 16u) | (destination << 8u) | (unused << 11u),
                    SourceZeroSelectMask | DestinationSelectMask | DestinationUnusedMask);
                if (((words[1] >> 16u) & 7u) != source || ((words[1] >> 8u) & 7u) != destination || ((words[1] >> 11u) & 3u) != unused) {
                    std::fprintf(stderr, "src0_sel %u dst_sel %u dst_unused %u: the word builder wrote other field values\n", source, destination, unused);
                    ok = false;
                } else if (Decode(words).outcome != Outcome::Accepted) {
                    std::fprintf(stderr, "src0_sel %u dst_sel %u dst_unused %u: the decoder refuses a v_mov_b32 SDWA move it documents as supported\n", source, destination, unused);
                    ok = false;
                }
            }
        }
    }
    return ok;
}

}

int main(int argc, char** argv) {
    int failures = SelectorWordsAreBuiltAndAccepted() ? 0 : 1;
    std::map<std::string, Baseline> baselines;
    std::map<std::string, std::vector<std::uint32_t>> accepted;
    for (const auto& variant : Variants) {
        const std::string key = std::string(variant.encoding) + ":" + variant.format;
        auto& baseline = baselines[key];
        baseline.opcodes = variant.opcodeCount;
        for (std::uint32_t opcode = 0; opcode < variant.opcodeCount; ++opcode) {
            if (Decode(variant.build(opcode, 0u, 0u, 0u)).outcome == Outcome::Accepted) {
                ++baseline.accepted;
                accepted[key].push_back(opcode);
            }
        }
    }

    const auto cases = Cases();
    std::map<std::string, Family> families;
    for (const auto& item : cases) {
        for (const auto& variant : Variants) {
            if (std::string(variant.encoding) != item.encoding) continue;
            const std::string key = std::string(variant.encoding) + ":" + variant.format;
            if (item.needsSecondSource && std::string(variant.format) == "VOP1") continue;
            if (std::string(variant.encoding) == "SDWA" && std::string(variant.format) == "VOPC" && ((item.second | item.clear) & VopcSdwaDestinationBits) != 0u) continue;
            auto& family = families[std::string(item.encoding) + "|" + item.modifier + "|" + item.combination];
            for (const auto opcode : accepted[key]) {
                const auto result = Decode(variant.build(opcode, item.first, item.second, item.clear));
                ++family.total;
                if (result.outcome == Outcome::Accepted) {
                    ++family.accepted;
                } else {
                    family.rejectedKind[result.message] = result.outcome == Outcome::Invalid ? "invalid" : "unsupported";
                    family.rejected[result.message].insert(std::string(variant.format) + ":" + Hex(opcode));
                    ++family.rejectedCount[result.message];
                }
            }
        }
    }

    std::uint32_t overallAccepted = 0;
    for (const auto& [name, family] : families) {
        std::uint32_t refused = 0;
        for (const auto& [message, count] : family.rejectedCount) refused += count;
        if (family.accepted + refused != family.total) {
            std::fprintf(stderr, "%s: %u accepted + %u refused != %u total\n", name.c_str(), family.accepted, refused, family.total);
            ++failures;
        }
        overallAccepted += family.accepted;
    }
    for (const auto& [key, baseline] : baselines) {
        if (baseline.accepted == 0u) {
            std::fprintf(stderr, "%s: the decoder accepts no plain word\n", key.c_str());
            ++failures;
        }
    }
    if (overallAccepted == 0u) {
        std::fprintf(stderr, "no modifier word was accepted\n");
        ++failures;
    }

    std::string json = "{\"format\":\"anyps5-decoder-modifiers\",\"version\":1,\"denominator\":\"opcodes whose modifier-free word the decoder accepts; a modifier counts as accepted when the decoder returns for the word with the field set; refused words are grouped by decoder message, with kind invalid when the message says invalid, reserved or truncated and unsupported otherwise\",\"baseline\":[";
    bool first = true;
    for (const auto& [key, baseline] : baselines) {
        const auto split = key.find(':');
        json += std::string(first ? "" : ",") + "{\"encoding\":" + Quote(key.substr(0, split)) + ",\"format\":" + Quote(key.substr(split + 1)) +
            ",\"opcodes\":" + std::to_string(baseline.opcodes) + ",\"accepted\":" + std::to_string(baseline.accepted) + "}";
        first = false;
    }
    json += "],\"families\":[";
    first = true;
    for (const auto& [name, family] : families) {
        const auto firstBar = name.find('|');
        const auto secondBar = name.find('|', firstBar + 1u);
        json += std::string(first ? "" : ",") + "{\"encoding\":" + Quote(name.substr(0, firstBar)) + ",\"modifier\":" + Quote(name.substr(firstBar + 1u, secondBar - firstBar - 1u)) +
            ",\"combination\":" + Quote(name.substr(secondBar + 1u)) + ",\"total\":" + std::to_string(family.total) + ",\"accepted\":" + std::to_string(family.accepted) +
            ",\"refused\":" + std::to_string(family.total - family.accepted) + ",\"translated\":null,\"rejected\":[";
        bool firstMessage = true;
        for (const auto& [message, opcodes] : family.rejected) {
            json += std::string(firstMessage ? "" : ",") + "{\"message\":" + Quote(message) + ",\"kind\":" + Quote(family.rejectedKind.at(message)) + ",\"count\":" + std::to_string(family.rejectedCount.at(message)) + ",\"opcodes\":[";
            bool firstOpcode = true;
            for (const auto& opcode : opcodes) {
                json += std::string(firstOpcode ? "" : ",") + Quote(opcode);
                firstOpcode = false;
            }
            json += "]}";
            firstMessage = false;
        }
        json += "]}";
        first = false;
    }
    json += "]}\n";
    std::fwrite(json.data(), 1, json.size(), stdout);
    if (argc > 1) {
        std::FILE* file = std::fopen(argv[1], "wb");
        if (file == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", argv[1]);
            return 1;
        }
        std::fwrite(json.data(), 1, json.size(), file);
        std::fclose(file);
    }
    return failures == 0 ? 0 : 1;
}

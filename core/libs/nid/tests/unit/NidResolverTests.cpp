#include <Testing/Test.hpp>
#include <nid/NidCompute.hpp>
#include <nid/NidResolver.hpp>

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Testing::Case;
using Testing::RequireEqual;

const std::string printfNid = "hcuQgD53UxM";

const Case oneNameNoPatchCut{"ResolveOneName_NoPatchCutSuffix_StripsSuffixOnly", [] {
    RequireEqual(Nid::ResolveOneName("memcpy_nid_no_patch_cut"), std::string("memcpy"), "resolved name");
}};

const Case oneNameNoPatch{"ResolveOneName_NoPatchSuffix_KeepsName", [] {
    RequireEqual(Nid::ResolveOneName("helper_nid_no_patch"), std::string("helper_nid_no_patch"), "resolved name");
}};

const Case oneNameSdl{"ResolveOneName_SdlPrefix_KeepsName", [] {
    RequireEqual(Nid::ResolveOneName("SDL_Init"), std::string("SDL_Init"), "resolved name");
}};

const Case oneNamePostfix{"ResolveOneName_NidPostfix_HashesNameWithoutPostfix", [] {
    RequireEqual(Nid::ResolveOneName("printf_nid_postfix"), printfNid, "resolved name");
}};

const Case oneNamePlain{"ResolveOneName_PlainName_HashesName", [] {
    RequireEqual(Nid::ResolveOneName("printf"), printfNid, "resolved name");
}};

const Case oneNameDisambiguation{"ResolveOneName_NumericDisambiguationMarker_IsStripped", [] {
    RequireEqual(Nid::ResolveOneName("printf_nid_disambig2"), printfNid, "marker without postfix");
    RequireEqual(Nid::ResolveOneName("printf_nid_disambig17_nid_postfix"), printfNid, "marker before postfix");
}};

const Case oneNameMarkerWithoutDigits{"ResolveOneName_DisambiguationMarkerWithoutDigits_IsHashedVerbatim", [] {
    RequireEqual(Nid::ResolveOneName("printf_nid_disambig"), Nid::ComputeNid("printf_nid_disambig", ""), "marker with no suffix");
    RequireEqual(Nid::ResolveOneName("printf_nid_disambigA1"), Nid::ComputeNid("printf_nid_disambigA1", ""), "marker with letters");
}};

const Case nidsForMixedNames{"ResolveNids_MixedNames_MapsEachNameByItsRule", [] {
    const auto result = Nid::ResolveNids({"printf_nid_postfix", "memcpy_nid_no_patch_cut", "helper_nid_no_patch", "sceKernelUsleep"}, "libTest", {});
    RequireEqual(result.size(), std::size_t{4}, "entry count");
    RequireEqual(result.at("printf_nid_postfix"), printfNid, "postfix name");
    RequireEqual(result.at("memcpy_nid_no_patch_cut"), std::string("memcpy"), "cut name");
    RequireEqual(result.at("helper_nid_no_patch"), std::string("helper_nid_no_patch"), "no-patch name");
    RequireEqual(result.at("sceKernelUsleep"), std::string("1jfXLRVzisc"), "plain name");
}};

const Case nidsKeepPlainNameWithPostfixTwin{"ResolveNids_PlainNameWithPostfixTwin_KeepsPlainName", [] {
    const auto result = Nid::ResolveNids({"printf", "printf_nid_postfix"}, "libTest", {});
    RequireEqual(result.at("printf"), std::string("printf"), "plain twin");
    RequireEqual(result.at("printf_nid_postfix"), printfNid, "postfix twin");
}};

const Case nidsExcludedExport{"ResolveNids_ExcludedExport_KeepsOriginalName", [] {
    const std::unordered_set<std::string> excluded{"printf"};
    const auto result = Nid::ResolveNids({"printf_nid_postfix", "malloc"}, "libTest", excluded);
    RequireEqual(result.at("printf_nid_postfix"), std::string("printf_nid_postfix"), "excluded through normalized name");
    RequireEqual(result.at("malloc"), std::string("gQX+4GDQjpM"), "not excluded");
}};

const Case nidsDuplicate{"ResolveNids_DuplicateExport_ThrowsWithSymbolAndLibrary", [] {
    Testing::RequireThrowsWithMessage<std::runtime_error>(
        [] { Nid::ResolveNids({"a", "b", "a"}, "libTest", {}); },
        "duplicate exported symbol \"a\" in library \"libTest\"", "duplicate export");
}};

const Case nidsEmpty{"ResolveNids_NoExports_ReturnsEmptyMap", [] {
    Testing::Require(Nid::ResolveNids({}, "libTest", {}).empty(), "empty input gives empty map");
}};

} // namespace

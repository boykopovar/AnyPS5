#include <Testing/Test.hpp>
#include <nid/NidCompute.hpp>

#include <stdexcept>
#include <string>

namespace {

using Testing::Case;
using Testing::RequireEqual;

const Case knownLibcNids{"ComputeNid_KnownLibcSymbols_MatchPublishedNids", [] {
    RequireEqual(Nid::ComputeNid("printf", ""), std::string("hcuQgD53UxM"), "printf");
    RequireEqual(Nid::ComputeNid("malloc", ""), std::string("gQX+4GDQjpM"), "malloc");
}};

const Case knownKernelNids{"ComputeNid_KnownKernelSymbols_MatchPublishedNids", [] {
    RequireEqual(Nid::ComputeNid("sceKernelUsleep", ""), std::string("1jfXLRVzisc"), "sceKernelUsleep");
    RequireEqual(Nid::ComputeNid("sceKernelGetProcParam", ""), std::string("959qrazPIrg"), "sceKernelGetProcParam");
}};

const Case singleCharacter{"ComputeNid_SingleCharacterSymbol_ProducesElevenCharacters", [] {
    RequireEqual(Nid::ComputeNid("a", ""), std::string("RZk3ozzWYBg"), "nid of a");
}};

const Case libraryNameIgnored{"ComputeNid_DifferentLibraryNames_ProduceSameNid", [] {
    RequireEqual(Nid::ComputeNid("printf", "libSceLibcInternal"), Nid::ComputeNid("printf", ""), "library name does not salt the nid");
}};

const Case emptySymbol{"ComputeNid_EmptySymbol_ThrowsInvalidArgument", [] {
    Testing::RequireThrowsWithMessage<std::invalid_argument>(
        [] { Nid::ComputeNid("", "libc"); }, "symbolName is empty", "empty symbol");
}};

} // namespace

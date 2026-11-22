#include <nid/IBinaryPatcher.hpp>
#include <nid/NidCompute.hpp>
#include <nid/NidResolver.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using Nid::PatchOptions;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TException, typename TAction>
void RequireThrows(TAction action, const std::string& message) {
    try {
        action();
    } catch (const TException&) {
        return;
    } catch (...) {
        throw std::runtime_error(message + " threw an unexpected exception");
    }
    throw std::runtime_error(message);
}

template<typename TException, typename TAction>
void RequireThrowsContaining(TAction action, const std::string& fragment, const std::string& message) {
    try {
        action();
    } catch (const TException& error) {
        Require(std::string(error.what()).find(fragment) != std::string::npos, message);
        return;
    } catch (...) {
        throw std::runtime_error(message + " threw an unexpected exception");
    }
    throw std::runtime_error(message);
}

}

int main() {
    try {
        using namespace Nid;

        Require(IsSceImport("sce"), "the shortest SCE name must be recognized");
        Require(IsSceImport("SCE_Kernel"), "SCE detection must ignore case");
        Require(IsSceImport("sCeKernel"), "SCE detection must ignore case per character");
        Require(IsSceImport("kernelFunction_nid_postfix"), "a NID postfix must mark an SCE import");
        Require(!IsSceImport("kernelFunction"), "an ordinary name must not be an SCE import");
        Require(!IsSceImport("sc"), "two leading SCE characters are not enough");
        Require(!IsSceImport(""), "an empty name must not be an SCE import");
        Require(!IsSceImport("xsce"), "SCE must be a prefix");
        Require(!IsSceImport("_nid_postfi"), "a partial postfix must not mark an SCE import");

        Require(IsMangledSceImport("_ZN3sce"), "the shortest mangled SCE name must be recognized");
        Require(IsMangledSceImport("_ZN3SCEFoo"), "mangled SCE detection must ignore case");
        Require(IsMangledSceImport("_ZNK3sce4TestEv"), "a const qualifier must be skipped");
        Require(IsMangledSceImport("_ZNV3sce"), "a volatile qualifier must be skipped");
        Require(IsMangledSceImport("_ZNr3sce"), "a restrict qualifier must be skipped");
        Require(IsMangledSceImport("_ZNR3sce"), "an R qualifier must be skipped");
        Require(IsMangledSceImport("_ZNO3sce"), "an O qualifier must be skipped");
        Require(IsMangledSceImport("_ZNrVK3sce"), "several qualifiers must be skipped");
        Require(IsMangledSceImport("_ZNKR3sce"), "const and R qualifiers must be skipped");
        Require(IsMangledSceImport("_ZNrO3sce"), "restrict and O qualifiers must be skipped");
        Require(!IsMangledSceImport("_ZN3foo"), "a mangled non-SCE name must be rejected");
        Require(!IsMangledSceImport("_ZN2sce"), "a mangled name needs the SCE length");
        Require(!IsMangledSceImport("_ZN3sc"), "a truncated mangled SCE name must be rejected");
        Require(!IsMangledSceImport("_ZN"), "the bare mangled prefix must be rejected");
        Require(!IsMangledSceImport(""), "an empty mangled name must be rejected");
        Require(!IsMangledSceImport("__ZN3sce"), "the mangled prefix must start the name");
        Require(!IsMangledSceImport("_ZNRO3sce"), "R and O cannot both be qualifiers");
        Require(!IsMangledSceImport("_ZNr3foo"), "qualifiers must precede an SCE name");

        Require(ResolveOneName("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "a known name must resolve to its NID");
        Require(ResolveOneName("sceKernelGetProcessTime_nid_postfix") == "4J2sUJmuHZQ", "the NID postfix must be stripped before computing");
        Require(ResolveOneName("sceKernelGetProcessTime_nid_disambig7") == "4J2sUJmuHZQ", "a numeric disambiguation suffix must be stripped");
        Require(ResolveOneName("sceHelper_nid_no_patch") == "sceHelper_nid_no_patch", "a no-patch name must stay unchanged");
        Require(ResolveOneName("sceHelper_nid_no_patch_cut") == "sceHelper", "a cut suffix must be removed");
        Require(ResolveOneName("SDL_Init") == "SDL_Init", "an SDL name must stay unchanged");
        Require(ResolveOneName("unknownFunction") == "wqZ6gt+Xu48", "an unknown name must still get a NID");
        RequireThrows<std::invalid_argument>([] { ResolveOneName(""); }, "an empty name must throw");

        Require(ResolveNids({}, "libkernel", {}).empty(), "no exports must produce an empty map");
        const auto resolved = ResolveNids({"sceKernelGetProcessTime", "unknownFunction", "sceHelper_nid_no_patch",
            "sceHelper_nid_no_patch_cut", "SDL_Init", "sceKernelGetProcessTime_nid_disambig7", "SDL_"}, "libkernel", {});
        Require(resolved.size() == 7u, "every export must produce one entry");
        Require(resolved.at("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "a known export must resolve to its NID");
        Require(resolved.at("unknownFunction") == "wqZ6gt+Xu48", "an unknown export must resolve to its NID");
        Require(resolved.at("sceHelper_nid_no_patch") == "sceHelper_nid_no_patch", "a no-patch export must stay unchanged");
        Require(resolved.at("sceHelper_nid_no_patch_cut") == "sceHelper", "a cut export must lose its suffix");
        Require(resolved.at("SDL_Init") == "SDL_Init", "an SDL export must stay unchanged");
        Require(resolved.at("SDL_") == "SDL_", "the shortest SDL prefix must stay unchanged");
        Require(resolved.at("sceKernelGetProcessTime_nid_disambig7") == "4J2sUJmuHZQ", "a disambiguated export must resolve to its base NID");
        Require(ResolveNids({"plainImport"}, "libkernel", {}) == ResolveNids({"plainImport"}, "anotherlib", {}), "the library name must not change the computed NID");

        const auto postfixedFirst = ResolveNids({"sceKernelGetProcessTime_nid_postfix", "sceKernelGetProcessTime"}, "libkernel", {});
        Require(postfixedFirst.at("sceKernelGetProcessTime") == "sceKernelGetProcessTime", "a plain alias must stay unchanged when its postfixed form exists");
        Require(postfixedFirst.at("sceKernelGetProcessTime_nid_postfix") == "4J2sUJmuHZQ", "the postfixed form must resolve to its base NID");
        const auto plainFirst = ResolveNids({"sceKernelGetProcessTime", "sceKernelGetProcessTime_nid_postfix"}, "libkernel", {});
        Require(plainFirst == postfixedFirst, "the alias rule must not depend on export order");

        const auto excluded = ResolveNids({"sceKernelGetProcessTime", "unknownFunction", "sceKernelGetProcessTime_nid_postfix"},
            "libkernel", {"unknownFunction", "sceKernelGetProcessTime"});
        Require(excluded.at("unknownFunction") == "unknownFunction", "an excluded export must stay unchanged");
        Require(excluded.at("sceKernelGetProcessTime_nid_postfix") == "sceKernelGetProcessTime_nid_postfix", "exclusions must use the normalized name");
        Require(excluded.at("sceKernelGetProcessTime") == "sceKernelGetProcessTime", "an excluded plain export must stay unchanged");
        Require(ResolveNids({"_nid_no_patch_cut"}, "libkernel", {}).at("_nid_no_patch_cut").empty(), "a cut suffix may leave an empty name");
        RequireThrows<std::invalid_argument>([] { ResolveNids({""}, "libkernel", {}); }, "an empty export name must throw");
        RequireThrowsContaining<std::runtime_error>([] { ResolveNids({"dup", "dup"}, "libdup", {}); },
            "dup", "a duplicate export must report the name");
        RequireThrowsContaining<std::runtime_error>([] { ResolveNids({"dup", "dup"}, "libdup", {}); },
            "libdup", "a duplicate export must report the library");
        RequireThrows<std::runtime_error>([] { ResolveNids({"dup", "dup"}, "libdup", {"dup"}); },
            "an excluded duplicate must still throw");

        const PatchOptions libraryMode;
        const PatchOptions libraryOnlySceMode{false, true};
        const PatchOptions executableMode{true, false};
        const PatchOptions executableOnlySceMode{true, true};
        const std::vector<std::string> imports{"sceKernelGetProcessTime", "sceHelper_nid_no_patch",
            "sceHelper_nid_no_patch_cut", "_ZN3sce4TestEv", "unknownFunction", "plainImport"};

        Require(ResolveImportNids({}, {}, libraryMode).empty(), "no imports must produce an empty map");
        const auto libraryImports = ResolveImportNids(imports, {}, libraryMode);
        Require(libraryImports.size() == imports.size(), "every import must produce one entry");
        Require(libraryImports.at("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "an SCE import must be resolved in library mode");
        Require(libraryImports.at("sceHelper_nid_no_patch") == "sceHelper_nid_no_patch", "a no-patch import must stay unchanged in library mode");
        Require(libraryImports.at("sceHelper_nid_no_patch_cut") == "sceHelper", "a cut import must lose its suffix in library mode");
        Require(libraryImports.at("_ZN3sce4TestEv") == "0j5tsHSDvBI", "a mangled SCE import must be resolved in library mode");
        Require(libraryImports.at("unknownFunction") == "unknownFunction", "a non-SCE import must stay unchanged in library mode");
        Require(libraryImports.at("plainImport") == "plainImport", "a plain import must stay unchanged in library mode");
        Require(ResolveImportNids(imports, {}, libraryOnlySceMode) == libraryImports, "onlySce must not change library mode");

        const auto executableImports = ResolveImportNids(imports, {}, executableMode);
        Require(executableImports.size() == imports.size(), "every import must produce one executable entry");
        Require(executableImports.at("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "an SCE import must be resolved in executable mode");
        Require(executableImports.at("sceHelper_nid_no_patch") == "sceHelper_nid_no_patch", "a no-patch import must stay unchanged in executable mode");
        Require(executableImports.at("sceHelper_nid_no_patch_cut") == "sceHelper", "a cut import must lose its suffix in executable mode");
        Require(executableImports.at("_ZN3sce4TestEv") == "0j5tsHSDvBI", "a mangled import must be resolved in executable mode");
        Require(executableImports.at("unknownFunction") == "wqZ6gt+Xu48", "a non-SCE import must be resolved in executable mode");
        Require(executableImports.at("plainImport") == "lKCBf8q8wVs", "a plain import must be resolved in executable mode");

        const auto executableOnlySceImports = ResolveImportNids(imports, {}, executableOnlySceMode);
        Require(executableOnlySceImports.at("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "an SCE import must be resolved with onlySce");
        Require(executableOnlySceImports.at("sceHelper_nid_no_patch") == "sceHelper_nid_no_patch", "a no-patch import must stay unchanged with onlySce");
        Require(executableOnlySceImports.at("sceHelper_nid_no_patch_cut") == "sceHelper", "a cut import must lose its suffix with onlySce");
        Require(executableOnlySceImports.at("_ZN3sce4TestEv") == "_ZN3sce4TestEv", "a mangled import must stay unchanged with onlySce");
        Require(executableOnlySceImports.at("unknownFunction") == "unknownFunction", "a non-SCE import must stay unchanged with onlySce");
        Require(executableOnlySceImports.at("plainImport") == "plainImport", "a plain import must stay unchanged with onlySce");

        const auto deduplicated = ResolveImportNids({"sceKernelGetProcessTime", "sceKernelGetProcessTime"}, {}, libraryMode);
        Require(deduplicated.size() == 1u, "duplicate imports must be deduplicated");
        Require(deduplicated.at("sceKernelGetProcessTime") == "4J2sUJmuHZQ", "a deduplicated import must keep its NID");
        RequireThrows<std::invalid_argument>([&] { ResolveImportNids({""}, {}, libraryMode); }, "an empty import must throw");

        const auto libraryExcluded = ResolveImportNids({"sceExcluded", "plainImport"},
            {"sceExcluded", "plainImport"}, libraryMode);
        Require(libraryExcluded.at("sceExcluded") == "m0YN7B60gps", "an excluded SCE import is still resolved in library mode");
        Require(libraryExcluded.at("plainImport") == "plainImport", "an excluded non-SCE import must stay unchanged in library mode");
        const auto executableExcluded = ResolveImportNids({"unknownFunction"}, {"unknownFunction"}, executableMode);
        Require(executableExcluded.at("unknownFunction") == "unknownFunction", "an excluded import must stay unchanged in executable mode");
        const auto executableOnlySceExcluded = ResolveImportNids({"sceExcluded", "plainImport"},
            {"sceExcluded", "plainImport"}, executableOnlySceMode);
        Require(executableOnlySceExcluded.at("sceExcluded") == "sceExcluded", "an excluded SCE import must stay unchanged with onlySce");
        Require(executableOnlySceExcluded.at("plainImport") == "plainImport", "an excluded non-SCE import must stay unchanged with onlySce");

        std::cout << "NID resolver tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include <cstddef>
#include <cstdint>
#include <ios>
#include <locale>
#include <mutex>

namespace {

    std::mutex g_localeInitMutex;
    bool g_localeInitialized = false;

    constexpr std::size_t LocinfoStorageSize = 64;

    struct LocinfoStorage {
        alignas(std::max_align_t) unsigned char bytes[LocinfoStorageSize];
    };

}

extern "C" {

    std::streamoff _ZSt7_BADOFF_nid_postfix = -1;
    std::fpos_t _ZSt4_Fpz_nid_postfix {};
    long _ZNSt6locale2id7_Id_cntE_nid_postfix = 0;

}

namespace {

    const std::locale& ClassicLocaleInstance() {
        static const std::locale instance = std::locale::classic();
        return instance;
    }

}

extern "C" {

    const std::locale* _ZSt21_sceLibcClassicLocale_nid_postfix = &ClassicLocaleInstance();

    void _ZNSt6locale5_InitEv_nid_postfix() {
        std::lock_guard<std::mutex> lock(g_localeInitMutex);
        if (!g_localeInitialized) {
            ClassicLocaleInstance();
            g_localeInitialized = true;
        }
    }

    void _ZNSt6locale5facet9_RegisterEv_nid_postfix() {
    }

    const std::locale* _ZNSt6locale16_GetgloballocaleEv_nid_postfix() {
        return &ClassicLocaleInstance();
    }

    void _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(
        const std::locale::facet** targetFacet, const std::locale* sourceLocale
    ) {
        const std::locale& effectiveLocale = sourceLocale != nullptr ? *sourceLocale : ClassicLocaleInstance();
        *targetFacet = &std::use_facet<std::collate<wchar_t>>(effectiveLocale);
    }

    void _ZNSt8_LocinfoC1EPKc_nid_postfix(LocinfoStorage* self, const char* localeName) {
        (void)localeName;
        new (self) LocinfoStorage {};
    }

    void _ZNSt8_LocinfoD1Ev_nid_postfix(LocinfoStorage* self) {
        (void)self;
    }

}

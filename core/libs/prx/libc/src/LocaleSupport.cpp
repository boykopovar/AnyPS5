#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
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

const std::locale& ClassicLocaleInstance() {
    static const std::locale instance = std::locale::classic();
    return instance;
}

}

extern "C" {

std::streamoff _ZSt7_BADOFF_nid_postfix = -1;
std::fpos_t _ZSt4_Fpz_nid_postfix {};
long _ZNSt6locale2id7_Id_cntE_nid_postfix = 0;

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

wchar_t* _Mbtowcx_nid_postfix(wchar_t* dst, const char* src, std::size_t count, mbstate_t* st) {
    while (count > 0) {
        std::size_t result = std::mbrtowc(dst, src, count, st);
        if (result == static_cast<std::size_t>(-1) || result == static_cast<std::size_t>(-2))
            return nullptr;
        if (result == 0)
            return dst;
        src += result;
        count -= result;
        ++dst;
    }
    return dst;
}

char* _Wctombx_nid_postfix(char* dst, wchar_t src, mbstate_t* st) {
    std::size_t result = std::wcrtomb(dst, src, st);
    if (result == static_cast<std::size_t>(-1))
        return nullptr;
    return dst + result;
}

const short* _Getpctype_nid_postfix() {
    const auto& facet = std::use_facet<std::ctype<char>>(ClassicLocaleInstance());
    return reinterpret_cast<const short*>(facet.table());
}

const short* _Getptolower_nid_postfix() {
    static short table[std::ctype<char>::table_size];
    static std::once_flag flag;
    std::call_once(flag, []() {
        const auto& facet = std::use_facet<std::ctype<char>>(ClassicLocaleInstance());
        for (int i = 0; i < std::ctype<char>::table_size; ++i)
            table[i] = static_cast<short>(facet.tolower(static_cast<char>(i)));
    });
    return table;
}

const short* _Getptoupper_nid_postfix() {
    static short table[std::ctype<char>::table_size];
    static std::once_flag flag;
    std::call_once(flag, []() {
        const auto& facet = std::use_facet<std::ctype<char>>(ClassicLocaleInstance());
        for (int i = 0; i < std::ctype<char>::table_size; ++i)
            table[i] = static_cast<short>(facet.toupper(static_cast<char>(i)));
    });
    return table;
}

mbstate_t* _Getpmbstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

mbstate_t* _Getpwcstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

wint_t _Towctrans_nid_postfix(wint_t c, wctrans_t desc) {
    return std::towctrans(c, desc);
}

void _init_env_nid_postfix() {
}

}

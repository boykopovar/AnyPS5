#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

const uint8_t* sceCesRefersUcsProfileCp1252(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceCesSbcToUtf8(const uint8_t* profile, uint8_t sbc, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len) {
 (void)profile;
 (void)sbc;
 (void)utf8;
 (void)utf8max;
 (void)utf8_len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceCesUtf8ToSbc(const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len, const uint8_t* profile, uint8_t* sbc) {
 (void)utf8;
 (void)utf8max;
 (void)utf8_len;
 (void)profile;
 (void)sbc;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

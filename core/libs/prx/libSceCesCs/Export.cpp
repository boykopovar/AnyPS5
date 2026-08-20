#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

const uint8_t* sceCesRefersUcsProfileCp1252(void) {
 return nullptr;
}

int sceCesSbcToUtf8(const uint8_t* profile, uint8_t sbc, uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len) {
 (void)profile;
 (void)sbc;
 (void)utf8;
 (void)utf8max;
 (void)utf8_len;
 return 0;
}

int sceCesUtf8ToSbc(const uint8_t* utf8, uint32_t utf8max, uint32_t* utf8_len, const uint8_t* profile, uint8_t* sbc) {
 (void)utf8;
 (void)utf8max;
 (void)utf8_len;
 (void)profile;
 (void)sbc;
 return 0;
}

}

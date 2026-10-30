#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

void Require(bool value) { if (!value) std::abort(); }

const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

int Encode(void* context, const char* utf8, std::uint8_t* iso, std::uint32_t isoMax, std::uint32_t& used, std::uint32_t& produced) {
    return sceCesUtf8StrToIso2022Str(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, iso, isoMax, &produced);
}

struct Case {
    int profile;
    bool encode;
    const char* input;
    std::uint32_t size;
    int result;
    std::uint32_t used;
    std::uint32_t produced;
    const char* output;
};

const Case CASES[] = {
    {0, true, "\x41", 1, static_cast<int>(0x00000000), 1, 1, "\x41"},
    {0, true, "", 0, static_cast<int>(0x00000000), 0, 0, ""},
    {0, true, "\xE3\x81\x82", 3, static_cast<int>(0x00000000), 3, 8, "\x1B\x24\x42\x24\x22\x1B\x28\x42"},
    {0, true, "\x41\xE3\x81\x82\xC2\xA5\x42\x0A", 8, static_cast<int>(0x00000000), 8, 12, "\x41\x1B\x24\x42\x24\x22\x1B\x28\x42\x5C\x42\x0A"},
    {0, true, "\xE2\x80\xBE\x7E", 4, static_cast<int>(0x805C0020), 3, 1, "\x7E"},
    {0, true, "\xE3\x81\x82\xE3\x81\x84", 6, static_cast<int>(0x00000000), 6, 10, "\x1B\x24\x42\x24\x22\x24\x24\x1B\x28\x42"},
    {0, true, "\xEF\xBD\xB6", 3, static_cast<int>(0x805C0024), 0, 0, ""},
    {0, true, "\xEF\xBD\xB6\xE3\x81\x82", 6, static_cast<int>(0x805C0024), 0, 0, ""},
    {0, true, "\xEF\xBD\xB6\xE3\x81\x82\xEF\xBD\xB6\x41", 10, static_cast<int>(0x805C0024), 0, 0, ""},
    {0, true, "\x5C", 1, static_cast<int>(0x00000000), 1, 8, "\x1B\x24\x42\x21\x40\x1B\x28\x42"},
    {0, true, "\xE3\x81\x82\x0A\xE3\x81\x84", 7, static_cast<int>(0x00000000), 7, 17, "\x1B\x24\x42\x24\x22\x1B\x28\x42\x0A\x1B\x24\x42\x24\x24\x1B\x28\x42"},
    {0, true, "\x1B", 1, static_cast<int>(0x805C0011), 0, 0, ""},
    {0, true, "\x0E", 1, static_cast<int>(0x805C0024), 0, 0, ""},
    {0, true, "\x41\x1B\x42", 3, static_cast<int>(0x805C0015), 1, 1, "\x41"},
    {0, true, "\xF0\x9F\x98\x80", 4, static_cast<int>(0x805C0020), 0, 0, ""},
    {0, true, "\xEF\xBB\xBF", 3, static_cast<int>(0x00000000), 3, 0, ""},
    {0, true, "\xC2\xA2", 2, static_cast<int>(0x00000000), 2, 8, "\x1B\x24\x42\x21\x71\x1B\x28\x42"},
    {0, true, "\xEF\xBD\xB6\x41", 4, static_cast<int>(0x805C0024), 0, 0, ""},
    {0, true, "\xC2\xA5\xE3\x81\x82\xC2\xA5", 7, static_cast<int>(0x00000000), 7, 10, "\x5C\x1B\x24\x42\x24\x22\x1B\x28\x42\x5C"},
    {0, true, "\xEF\xBC\xBC", 3, static_cast<int>(0x805C0020), 0, 0, ""},
    {0, true, "\x41\xC2\x80\x42", 4, static_cast<int>(0x805C0024), 1, 1, "\x41"},
    {0, false, "\x1B\x24\x40\x24\x22\x1B\x28\x42", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {0, false, "\x1B\x24\x42\x24\x22", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x81\x82"},
    {0, false, "\x1B\x24\x42\x24\x22\x1B\x28\x4A", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {0, false, "\x1B\x28\x4A\x41", 4, static_cast<int>(0x00000000), 4, 1, "\x41"},
    {0, false, "\x1B\x28\x4A\x41\x1B\x28\x42", 7, static_cast<int>(0x805C0011), 4, 1, "\x41"},
    {0, false, "\x1B\x28\x4A\x5C\x7E\x41\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 6, "\xC2\xA5\xE2\x80\xBE\x41"},
    {0, false, "\x1B\x24\x42\x24\x22\x0A\x24\x24\x1B\x28\x42", 11, static_cast<int>(0x805C0011), 8, 7, "\xE3\x81\x82\x0A\xE3\x81\x84"},
    {0, false, "\x1B\x28\x49\x36", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x0E\x36\x0F", 3, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x0E\x36\x0F\x41", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x0E\x36", 2, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x1B\x24\x28\x44\x30\x21", 6, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x1B\x24", 2, static_cast<int>(0x805C0011), 0, 0, ""},
    {0, false, "\x1B\x24\x42\x24", 4, static_cast<int>(0x805C0011), 0, 0, ""},
    {0, false, "\x1B\x28\x5A", 3, static_cast<int>(0x805C0011), 0, 0, ""},
    {0, false, "\xB6", 1, static_cast<int>(0x805C0014), 0, 0, ""},
    {0, false, "\x1B\x24\x42\x29\x21", 5, static_cast<int>(0x805C0020), 0, 0, ""},
    {0, false, "\x1B\x24\x42\x24\x7F", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x82\xA1"},
    {0, false, "\x1B\x28\x49\x36\x1B\x28\x42", 7, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x1B\x28\x49\x60", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {0, false, "\x5C\x7E", 2, static_cast<int>(0x00000000), 2, 5, "\xC2\xA5\xE2\x80\xBE"},
    {0, false, "\x41\x1B\x28\x42", 4, static_cast<int>(0x805C0011), 1, 1, "\x41"},
    {0, false, "\x41\x1B\x24\x42\x24\x22\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 4, "\x41\xE3\x81\x82"},
    {0, false, "\x41\x1B\x24\x42\x24\x22\x24\x24", 8, static_cast<int>(0x00000000), 8, 7, "\x41\xE3\x81\x82\xE3\x81\x84"},
    {0, false, "\x1B\x24\x42\x24\x22\x41", 6, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {1, true, "\x41", 1, static_cast<int>(0x00000000), 1, 1, "\x41"},
    {1, true, "", 0, static_cast<int>(0x00000000), 0, 0, ""},
    {1, true, "\xE3\x81\x82", 3, static_cast<int>(0x00000000), 3, 8, "\x1B\x24\x42\x24\x22\x1B\x28\x42"},
    {1, true, "\x41\xE3\x81\x82\xC2\xA5\x42\x0A", 8, static_cast<int>(0x00000000), 8, 13, "\x41\x1B\x24\x42\x24\x22\x21\x6F\x1B\x28\x42\x42\x0A"},
    {1, true, "\xE2\x80\xBE\x7E", 4, static_cast<int>(0x00000000), 4, 9, "\x1B\x24\x42\x21\x31\x1B\x28\x42\x7E"},
    {1, true, "\xE3\x81\x82\xE3\x81\x84", 6, static_cast<int>(0x00000000), 6, 10, "\x1B\x24\x42\x24\x22\x24\x24\x1B\x28\x42"},
    {1, true, "\xEF\xBD\xB6", 3, static_cast<int>(0x00000000), 3, 7, "\x1B\x28\x49\x36\x1B\x28\x42"},
    {1, true, "\xEF\xBD\xB6\xE3\x81\x82", 6, static_cast<int>(0x00000000), 6, 12, "\x1B\x28\x49\x36\x1B\x24\x42\x24\x22\x1B\x28\x42"},
    {1, true, "\xEF\xBD\xB6\xE3\x81\x82\xEF\xBD\xB6\x41", 10, static_cast<int>(0x00000000), 10, 17, "\x1B\x28\x49\x36\x1B\x24\x42\x24\x22\x1B\x28\x49\x36\x1B\x28\x42\x41"},
    {1, true, "\x5C", 1, static_cast<int>(0x00000000), 1, 1, "\x5C"},
    {1, true, "\xE3\x81\x82\x0A\xE3\x81\x84", 7, static_cast<int>(0x00000000), 7, 17, "\x1B\x24\x42\x24\x22\x1B\x28\x42\x0A\x1B\x24\x42\x24\x24\x1B\x28\x42"},
    {1, true, "\x1B", 1, static_cast<int>(0x805C0011), 0, 0, ""},
    {1, true, "\x0E", 1, static_cast<int>(0x805C0024), 0, 0, ""},
    {1, true, "\x41\x1B\x42", 3, static_cast<int>(0x805C0015), 1, 1, "\x41"},
    {1, true, "\xF0\x9F\x98\x80", 4, static_cast<int>(0x805C0020), 0, 0, ""},
    {1, true, "\xEF\xBB\xBF", 3, static_cast<int>(0x00000000), 3, 0, ""},
    {1, true, "\xC2\xA2", 2, static_cast<int>(0x00000000), 2, 8, "\x1B\x24\x42\x21\x71\x1B\x28\x42"},
    {1, true, "\xEF\xBD\xB6\x41", 4, static_cast<int>(0x00000000), 4, 8, "\x1B\x28\x49\x36\x1B\x28\x42\x41"},
    {1, true, "\xC2\xA5\xE3\x81\x82\xC2\xA5", 7, static_cast<int>(0x00000000), 7, 12, "\x1B\x24\x42\x21\x6F\x24\x22\x21\x6F\x1B\x28\x42"},
    {1, true, "\xEF\xBC\xBC", 3, static_cast<int>(0x00000000), 3, 8, "\x1B\x24\x42\x21\x40\x1B\x28\x42"},
    {1, true, "\x41\xC2\x80\x42", 4, static_cast<int>(0x805C0024), 1, 1, "\x41"},
    {1, false, "\x1B\x24\x40\x24\x22\x1B\x28\x42", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {1, false, "\x1B\x24\x42\x24\x22", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x81\x82"},
    {1, false, "\x1B\x24\x42\x24\x22\x1B\x28\x4A", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {1, false, "\x1B\x28\x4A\x41", 4, static_cast<int>(0x00000000), 4, 1, "\x41"},
    {1, false, "\x1B\x28\x4A\x41\x1B\x28\x42", 7, static_cast<int>(0x805C0011), 4, 1, "\x41"},
    {1, false, "\x1B\x28\x4A\x5C\x7E\x41\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 3, "\x5C\x7E\x41"},
    {1, false, "\x1B\x24\x42\x24\x22\x0A\x24\x24\x1B\x28\x42", 11, static_cast<int>(0x805C0011), 8, 7, "\xE3\x81\x82\x0A\xE3\x81\x84"},
    {1, false, "\x1B\x28\x49\x36", 4, static_cast<int>(0x00000000), 4, 3, "\xEF\xBD\xB6"},
    {1, false, "\x0E\x36\x0F", 3, static_cast<int>(0x805C0015), 0, 0, ""},
    {1, false, "\x0E\x36\x0F\x41", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {1, false, "\x0E\x36", 2, static_cast<int>(0x805C0015), 0, 0, ""},
    {1, false, "\x1B\x24\x28\x44\x30\x21", 6, static_cast<int>(0x805C0015), 0, 0, ""},
    {1, false, "\x1B\x24", 2, static_cast<int>(0x805C0011), 0, 0, ""},
    {1, false, "\x1B\x24\x42\x24", 4, static_cast<int>(0x805C0011), 0, 0, ""},
    {1, false, "\x1B\x28\x5A", 3, static_cast<int>(0x805C0011), 0, 0, ""},
    {1, false, "\xB6", 1, static_cast<int>(0x805C0014), 0, 0, ""},
    {1, false, "\x1B\x24\x42\x29\x21", 5, static_cast<int>(0x805C0020), 0, 0, ""},
    {1, false, "\x1B\x24\x42\x24\x7F", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x82\xA1"},
    {1, false, "\x1B\x28\x49\x36\x1B\x28\x42", 7, static_cast<int>(0x805C0011), 4, 3, "\xEF\xBD\xB6"},
    {1, false, "\x1B\x28\x49\x60", 4, static_cast<int>(0x805C0020), 0, 0, ""},
    {1, false, "\x5C\x7E", 2, static_cast<int>(0x00000000), 2, 2, "\x5C\x7E"},
    {1, false, "\x41\x1B\x28\x42", 4, static_cast<int>(0x805C0011), 1, 1, "\x41"},
    {1, false, "\x41\x1B\x24\x42\x24\x22\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 4, "\x41\xE3\x81\x82"},
    {1, false, "\x41\x1B\x24\x42\x24\x22\x24\x24", 8, static_cast<int>(0x00000000), 8, 7, "\x41\xE3\x81\x82\xE3\x81\x84"},
    {1, false, "\x1B\x24\x42\x24\x22\x41", 6, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {2, true, "\x41", 1, static_cast<int>(0x00000000), 1, 1, "\x41"},
    {2, true, "", 0, static_cast<int>(0x00000000), 0, 0, ""},
    {2, true, "\xE3\x81\x82", 3, static_cast<int>(0x00000000), 3, 8, "\x1B\x24\x42\x24\x22\x1B\x28\x42"},
    {2, true, "\x41\xE3\x81\x82\xC2\xA5\x42\x0A", 8, static_cast<int>(0x00000000), 8, 13, "\x41\x1B\x24\x42\x24\x22\x21\x6F\x1B\x28\x42\x42\x0A"},
    {2, true, "\xE2\x80\xBE\x7E", 4, static_cast<int>(0x00000000), 4, 9, "\x1B\x24\x42\x21\x31\x1B\x28\x42\x7E"},
    {2, true, "\xE3\x81\x82\xE3\x81\x84", 6, static_cast<int>(0x00000000), 6, 10, "\x1B\x24\x42\x24\x22\x24\x24\x1B\x28\x42"},
    {2, true, "\xEF\xBD\xB6", 3, static_cast<int>(0x00000000), 3, 6, "\x0E\x36\x0F\x1B\x28\x42"},
    {2, true, "\xEF\xBD\xB6\xE3\x81\x82", 6, static_cast<int>(0x00000000), 6, 11, "\x0E\x36\x0F\x1B\x24\x42\x24\x22\x1B\x28\x42"},
    {2, true, "\xEF\xBD\xB6\xE3\x81\x82\xEF\xBD\xB6\x41", 10, static_cast<int>(0x00000000), 10, 15, "\x0E\x36\x0F\x1B\x24\x42\x24\x22\x0E\x36\x0F\x1B\x28\x42\x41"},
    {2, true, "\x5C", 1, static_cast<int>(0x00000000), 1, 1, "\x5C"},
    {2, true, "\xE3\x81\x82\x0A\xE3\x81\x84", 7, static_cast<int>(0x00000000), 7, 17, "\x1B\x24\x42\x24\x22\x1B\x28\x42\x0A\x1B\x24\x42\x24\x24\x1B\x28\x42"},
    {2, true, "\x1B", 1, static_cast<int>(0x805C0011), 0, 0, ""},
    {2, true, "\x0E", 1, static_cast<int>(0x805C0024), 0, 0, ""},
    {2, true, "\x41\x1B\x42", 3, static_cast<int>(0x805C0015), 1, 1, "\x41"},
    {2, true, "\xF0\x9F\x98\x80", 4, static_cast<int>(0x805C0020), 0, 0, ""},
    {2, true, "\xEF\xBB\xBF", 3, static_cast<int>(0x00000000), 3, 0, ""},
    {2, true, "\xC2\xA2", 2, static_cast<int>(0x00000000), 2, 8, "\x1B\x24\x42\x21\x71\x1B\x28\x42"},
    {2, true, "\xEF\xBD\xB6\x41", 4, static_cast<int>(0x00000000), 4, 7, "\x0E\x36\x0F\x1B\x28\x42\x41"},
    {2, true, "\xC2\xA5\xE3\x81\x82\xC2\xA5", 7, static_cast<int>(0x00000000), 7, 12, "\x1B\x24\x42\x21\x6F\x24\x22\x21\x6F\x1B\x28\x42"},
    {2, true, "\xEF\xBC\xBC", 3, static_cast<int>(0x00000000), 3, 8, "\x1B\x24\x42\x21\x40\x1B\x28\x42"},
    {2, true, "\x41\xC2\x80\x42", 4, static_cast<int>(0x805C0024), 1, 1, "\x41"},
    {2, false, "\x1B\x24\x40\x24\x22\x1B\x28\x42", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {2, false, "\x1B\x24\x42\x24\x22", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x81\x82"},
    {2, false, "\x1B\x24\x42\x24\x22\x1B\x28\x4A", 8, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
    {2, false, "\x1B\x28\x4A\x41", 4, static_cast<int>(0x00000000), 4, 1, "\x41"},
    {2, false, "\x1B\x28\x4A\x41\x1B\x28\x42", 7, static_cast<int>(0x805C0011), 4, 1, "\x41"},
    {2, false, "\x1B\x28\x4A\x5C\x7E\x41\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 3, "\x5C\x7E\x41"},
    {2, false, "\x1B\x24\x42\x24\x22\x0A\x24\x24\x1B\x28\x42", 11, static_cast<int>(0x805C0011), 8, 7, "\xE3\x81\x82\x0A\xE3\x81\x84"},
    {2, false, "\x1B\x28\x49\x36", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {2, false, "\x0E\x36\x0F", 3, static_cast<int>(0x805C0011), 2, 3, "\xEF\xBD\xB6"},
    {2, false, "\x0E\x36\x0F\x41", 4, static_cast<int>(0x00000000), 4, 4, "\xEF\xBD\xB6\x41"},
    {2, false, "\x0E\x36", 2, static_cast<int>(0x00000000), 2, 3, "\xEF\xBD\xB6"},
    {2, false, "\x1B\x24\x28\x44\x30\x21", 6, static_cast<int>(0x805C0015), 0, 0, ""},
    {2, false, "\x1B\x24", 2, static_cast<int>(0x805C0011), 0, 0, ""},
    {2, false, "\x1B\x24\x42\x24", 4, static_cast<int>(0x805C0011), 0, 0, ""},
    {2, false, "\x1B\x28\x5A", 3, static_cast<int>(0x805C0011), 0, 0, ""},
    {2, false, "\xB6", 1, static_cast<int>(0x805C0014), 0, 0, ""},
    {2, false, "\x1B\x24\x42\x29\x21", 5, static_cast<int>(0x805C0020), 0, 0, ""},
    {2, false, "\x1B\x24\x42\x24\x7F", 5, static_cast<int>(0x00000000), 5, 3, "\xE3\x82\xA1"},
    {2, false, "\x1B\x28\x49\x36\x1B\x28\x42", 7, static_cast<int>(0x805C0015), 0, 0, ""},
    {2, false, "\x1B\x28\x49\x60", 4, static_cast<int>(0x805C0015), 0, 0, ""},
    {2, false, "\x5C\x7E", 2, static_cast<int>(0x00000000), 2, 2, "\x5C\x7E"},
    {2, false, "\x41\x1B\x28\x42", 4, static_cast<int>(0x805C0011), 1, 1, "\x41"},
    {2, false, "\x41\x1B\x24\x42\x24\x22\x1B\x28\x42", 9, static_cast<int>(0x805C0011), 6, 4, "\x41\xE3\x81\x82"},
    {2, false, "\x41\x1B\x24\x42\x24\x22\x24\x24", 8, static_cast<int>(0x00000000), 8, 7, "\x41\xE3\x81\x82\xE3\x81\x84"},
    {2, false, "\x1B\x24\x42\x24\x22\x41", 6, static_cast<int>(0x805C0011), 5, 3, "\xE3\x81\x82"},
};

void* InitProfile(int profile, std::uint8_t* sheet) {
    if (profile == 0) return sceCesUcsProfileInitIso2022JpRfc1468(sheet);
    if (profile == 1) return sceCesUcsProfileInitIso2022JpCp50221(sheet);
    return sceCesUcsProfileInitIso2022JpCp50222(sheet);
}

void CheckCases() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    for (const Case& entry : CASES) {
        Require(InitProfile(entry.profile, sheet) == sheet && sceCesIso2022UcsContextInit(context, sheet) == 0);
        std::uint8_t out[64];
        std::memset(out, 0xCC, sizeof(out));
        std::uint32_t used = 0x77777777;
        std::uint32_t produced = 0x66666666;
        const int result = entry.encode ? sceCesUtf8StrToIso2022Str(context, Bytes(entry.input), entry.size, &used, out, sizeof(out), &produced)
                                        : sceCesIso2022StrToUtf8Str(context, Bytes(entry.input), entry.size, &used, out, sizeof(out), &produced);
        Require(result == entry.result && used == entry.used && produced == entry.produced && std::memcmp(out, entry.output, produced) == 0);
    }
}

void Fnv(std::uint32_t& hash, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) hash = (hash ^ ((value >> (8 * i)) & 0xFF)) * 0x01000193u;
}

const char* const PREFIXES[] = {"", "\x1B(B", "\x1B(J", "\x1B$@", "\x1B$B", "\x1B(I", "\x1B$(D", "\x1B$A", "\x1B$(Q"};

void CheckSweeps(int profile, std::uint32_t encodeHash, std::uint32_t decodeHash) {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(InitProfile(profile, sheet) == sheet);
    std::uint32_t hash = 0x811C9DC5u;
    for (std::uint32_t code = 1; code < 0x10000; ++code) {
        Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
        std::uint8_t iso[16];
        std::memset(iso, 0xCC, sizeof(iso));
        std::uint32_t used = 0x77777777;
        std::uint32_t written = 0x66666666;
        Fnv(hash, static_cast<std::uint32_t>(sceCesUtf32StrToIso2022Str(context, &code, 1, &used, iso, sizeof(iso), &written)));
        Fnv(hash, used);
        Fnv(hash, written);
        for (std::uint32_t i = 0; i < written && i < 16; ++i) hash = (hash ^ iso[i]) * 0x01000193u;
    }
    Require(hash == encodeHash);
    hash = 0x811C9DC5u;
    for (const char* prefix : PREFIXES) {
        const std::uint32_t size = static_cast<std::uint32_t>(std::strlen(prefix));
        for (std::uint32_t bytes = 1; bytes <= 2; ++bytes) {
            for (std::uint32_t i = 0; i < (bytes == 1 ? 256u : 94u * 94u); ++i) {
                std::uint8_t iso[8] = {};
                std::memcpy(iso, prefix, size);
                if (bytes == 1) iso[size] = static_cast<std::uint8_t>(i);
                else {
                    iso[size] = static_cast<std::uint8_t>(0x21 + i / 94);
                    iso[size + 1] = static_cast<std::uint8_t>(0x21 + i % 94);
                }
                Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
                std::uint32_t out[4] = {};
                std::uint32_t used = 0x77777777;
                std::uint32_t produced = 0x66666666;
                Fnv(hash, static_cast<std::uint32_t>(sceCesIso2022StrToUtf32Str(context, iso, size + bytes, &used, out, 4, &produced)));
                Fnv(hash, used);
                Fnv(hash, produced);
                if (produced > 0) Fnv(hash, out[0]);
                if (produced > 1) Fnv(hash, out[1]);
            }
        }
    }
    Require(hash == decodeHash);
}

void CheckArguments() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitIso2022JpRfc1468(sheet) == sheet);
    Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    std::uint8_t out[4] = {};
    Require(Encode(context, "\xE3\x81\x82", out, sizeof(out), used, produced) == dstBufferEnd);
    Require(Encode(context, "A", nullptr, 0, used, produced) == invalidDstBuffer);
    Require(sceCesUtf8StrGetIso2022Len(context, Bytes("A\xE3\x81\x82"), 4, &used, &produced) == 0 && used == 4 && produced == 9);
    Require(sceCesIso2022StrGetUtf8Len(context, Bytes("A\x1B$B\x24\x22"), 6, &used, &produced) == 0 && used == 6 && produced == 4);
    Require(sceCesUtf8StrToIso2022Str(context, nullptr, 1, &used, out, sizeof(out), &produced) == invalidSrcBuffer);
    Require(sceCesIso2022StrToUtf8Str(context, nullptr, 1, &used, out, sizeof(out), &produced) == invalidSrcBuffer);
    Require(sceCesIso2022StrToUtf8Str(context, Bytes("A"), 1, &used, nullptr, 0, &produced) == invalidDstBuffer);
    Require(sceCesUtf8StrToIso2022Str(nullptr, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    Require(sceCesIso2022UcsContextInit(nullptr, sheet) == invalidParameter);
    alignas(8) std::uint8_t cleared[0x400] = {};
    std::memset(cleared, 0xA5, sizeof(cleared));
    Require(sceCesIso2022UcsContextInit(cleared, nullptr) == invalidParameter);
    for (std::size_t i = 0; i < sizeof(cleared); ++i) Require(cleared[i] == (i < 0x100 ? 0 : 0xA5));
    Require(sceCesIso2022StrToUtf8Str(cleared, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    alignas(8) std::uint8_t copy[64] = {};
    Require(sceCesIso2022UcsContextInitCopy(copy, context) == 0);
    std::uint8_t iso[16] = {};
    Require(Encode(copy, "\xE3\x81\x82", iso, sizeof(iso), used, produced) == 0 && produced == 8 && std::memcmp(iso, "\x1B$B\x24\x22\x1B(B", 8) == 0);
    Require(sceCesIso2022UcsContextInitCopy(copy, nullptr) == invalidParameter);
    Require(sceCesMbcsStrToUtf8Str(context, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    alignas(8) std::uint8_t sjis[64] = {};
    alignas(8) std::uint8_t other[64] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjis) == sjis);
    Require(sceCesMbcsUcsContextInit(other, sjis) == 0);
    Require(sceCesUtf8StrToIso2022Str(other, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    Require(sceCesIso2022StrToUtf8Str(other, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
}

void CheckWideUnits() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitIso2022JpRfc1468(sheet) == sheet);
    Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
    const std::uint16_t ucs2[] = {0x41, 0x3042, 0};
    const std::uint32_t utf32[] = {0x41, 0x3042, 0};
    std::uint8_t iso[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 2, &used, iso, sizeof(iso), &produced) == 0 && used == 2 && produced == 9);
    Require(std::memcmp(iso, "A\x1B$B\x24\x22\x1B(B", 9) == 0);
    Require(sceCesUcs2StrGetIso2022Len(context, ucs2, 2, &used, &produced) == 0 && produced == 9);
    Require(sceCesUtf16StrToIso2022Str(context, ucs2, 2, &used, iso, sizeof(iso), &produced) == 0 && produced == 9);
    Require(sceCesUtf16StrGetIso2022Len(context, ucs2, 2, &used, &produced) == 0 && produced == 9);
    Require(sceCesUtf32StrToIso2022Str(context, utf32, 2, &used, iso, sizeof(iso), &produced) == 0 && produced == 9);
    Require(sceCesUtf32StrGetIso2022Len(context, utf32, 2, &used, &produced) == 0 && produced == 9);
    std::uint16_t units[8] = {};
    std::uint32_t wide[8] = {};
    Require(sceCesIso2022StrToUcs2Str(context, iso, 10, &used, units, 8, &produced) == 0 && used == 6 && produced == 2 && units[0] == 0x41 && units[1] == 0x3042);
    Require(sceCesIso2022StrGetUcs2Len(context, iso, 10, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUtf16Str(context, iso, 10, &used, units, 8, &produced) == 0 && produced == 2 && units[1] == 0x3042);
    Require(sceCesIso2022StrGetUtf16Len(context, iso, 10, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUtf32Str(context, iso, 10, &used, wide, 8, &produced) == 0 && produced == 2 && wide[1] == 0x3042);
    Require(sceCesIso2022StrGetUtf32Len(context, iso, 10, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUcs2Str(context, iso, 10, &used, units, 1, &produced) == dstBufferEnd);
    Require(sceCesIso2022StrToUcs2Str(context, iso, 10, &used, nullptr, 0, &produced) == invalidDstBuffer);
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 2, &used, nullptr, 0, &produced) == invalidDstBuffer);
}

}

int main() {
    CheckCases();
    CheckSweeps(0, 0x84AEE5BBu, 0xC3973AD3u);
    CheckSweeps(1, 0x2419A587u, 0x7714F53Eu);
    CheckSweeps(2, 0xA1B4C62Du, 0xF29A5516u);
    CheckArguments();
    CheckWideUnits();
    return 0;
}

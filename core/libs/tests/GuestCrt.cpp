#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <cfenv>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <limits>

extern "C" {
SonyLconv* APS5_VABI sceAmprLocaleconv(void);
double APS5_VABI sceAmprSinh(double x, double y);
char* APS5_VABI sceAmprStrtokS(char* str, const char* delim, char** context);
int APS5_VABI sceAmprCtimeS(char* buffer, std::size_t size, const std::time_t* time);
int APS5_VABI sceAmprWcscpyS(std::uint16_t* dest, std::size_t size, const std::uint16_t* src);
void* APS5_VABI sceAmprSetConstraintHandlerS(void* handler);
int APS5_VABI sceAmprFltrounds(void);
void APS5_VABI sceAmprIgnoreHandlerS(const char* message, void* pointer, int error);
std::size_t APS5_VABI sceAmprMbstowcs(std::uint16_t* dest, const char* src, std::size_t size);
int APS5_VABI sceAmprAsctimeS(char* buffer, std::size_t size, const std::tm* time);
int APS5_VABI sceAmprFpclassifyd(double value);
}

static void Require(bool value) { if (!value) std::abort(); }

static int g_handlerCalls = 0;
static void CountingHandler(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
    ++g_handlerCalls;
}

int main() {
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::quiet_NaN()) == 2);
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::infinity()) == 1);
    Require(sceAmprFpclassifyd(0.0) == 0);
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::denorm_min()) == -2);
    Require(sceAmprFpclassifyd(1.5) == -1);

    Require(sceAmprFltrounds() == 1);
    Require(std::fesetround(FE_DOWNWARD) == 0);
    Require(sceAmprFltrounds() == 3);
    Require(std::fesetround(FE_TONEAREST) == 0);
    Require(sceAmprFltrounds() == 1);

    Require(sceAmprSinh(1.0, 2.0) == 2.0 * std::sinh(1.0));
    Require(sceAmprSinh(0.0, 5.0) == 0.0);

    SonyLconv* locale = sceAmprLocaleconv();
    Require(reinterpret_cast<std::uintptr_t>(&locale->decimal_point) - reinterpret_cast<std::uintptr_t>(locale) == 0x48);
    Require(std::strcmp(locale->decimal_point, ".") == 0);

    Require(SonyUdivti3(100, 3) == 33);
    Require(SonyUdivti3(0, 5) == 0);
    Require(SonyUdivti3((unsigned __int128)1 << 100, 3) == (((unsigned __int128)1 << 100) / 3));
    bool trapped = false;
    try {
        SonyUdivti3(1, 0);
    } catch (const std::exception&) {
        trapped = true;
    }
    Require(trapped);

    void* previous = sceAmprSetConstraintHandlerS(reinterpret_cast<void*>(&CountingHandler));
    Require(previous != nullptr);
    Require(previous != reinterpret_cast<void*>(&CountingHandler));
    const std::uint16_t hello[] = {'h', 'i', 0};
    std::uint16_t buffer[8] = {};
    Require(sceAmprWcscpyS(buffer, 8, hello) == 0);
    Require(buffer[0] == 'h' && buffer[1] == 'i' && buffer[2] == 0);
    const std::uint16_t longText[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 0};
    g_handlerCalls = 0;
    Require(sceAmprWcscpyS(buffer, 4, longText) == ERANGE);
    Require(g_handlerCalls == 1);
    Require(sceAmprWcscpyS(nullptr, 4, longText) == EINVAL);
    previous = sceAmprSetConstraintHandlerS(nullptr);
    Require(previous == reinterpret_cast<void*>(&CountingHandler));
    previous = sceAmprSetConstraintHandlerS(reinterpret_cast<void*>(&CountingHandler));
    Require(previous != nullptr && previous != reinterpret_cast<void*>(&CountingHandler));
    sceAmprIgnoreHandlerS("msg", nullptr, 0);

    char text[64];
    std::tm known{};
    known.tm_year = 70;
    known.tm_mon = 0;
    known.tm_mday = 1;
    known.tm_wday = 4;
    Require(sceAmprAsctimeS(text, sizeof(text), &known) == 0);
    Require(std::strcmp(text, "Thu Jan  1 00:00:00 1970\n") == 0);
    Require(sceAmprAsctimeS(nullptr, sizeof(text), &known) == EINVAL);
    Require(sceAmprAsctimeS(text, 25, &known) == EINVAL);
    const std::time_t epoch = 0;
    Require(sceAmprCtimeS(text, sizeof(text), &epoch) == 0);
    Require(std::strlen(text) == 25 && text[24] == '\n');
    Require(sceAmprCtimeS(text, sizeof(text), nullptr) == EINVAL);

    char line[] = "a,b,,c";
    char* context = nullptr;
    Require(std::strcmp(sceAmprStrtokS(line, ",", &context), "a") == 0);
    Require(std::strcmp(sceAmprStrtokS(nullptr, ",", &context), "b") == 0);
    Require(std::strcmp(sceAmprStrtokS(nullptr, ",", &context), "c") == 0);
    Require(sceAmprStrtokS(nullptr, ",", &context) == nullptr);
    Require(sceAmprStrtokS(line, nullptr, &context) == nullptr);

    std::uint16_t wide[8] = {};
    Require(sceAmprMbstowcs(wide, "ABC", 8) == 3);
    Require(wide[0] == 65 && wide[1] == 66 && wide[2] == 67 && wide[3] == 0);
    Require(sceAmprMbstowcs(nullptr, "ABC", 8) == 3);
    Require(sceAmprMbstowcs(wide, "ABC", 2) == 2);
}

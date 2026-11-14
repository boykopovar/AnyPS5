#include <cstdio>
#include <string_view>

void CheckIcuCalendar();
void CheckIcuCollator();
void CheckIcuDateFormat();
void CheckIcuDateTimePatternGenerator();
void CheckIcuDateIntervalFormat();
void CheckIcuFieldPositionIterator();
void CheckIcuFormattedValue();
void CheckIcuIdna();
void CheckIcuIdnaUts46();
void CheckIcuIdnaNative();
void CheckIcuIdnaConformance(int operation);
void CheckIcuIdnaMemory();
#ifdef __linux__
void CheckIcuIdnaInputAllocation();
#endif
void CheckIcuLocaleDisplayNames();
void CheckIcuListFormatter();
void CheckIcuNumberFormat();
void CheckIcuNumberFormatter();
void CheckIcuNumberingSystem();
void CheckIcuPluralRules();
void CheckIcuRelativeDateTimeFormatter();
void CheckIcuResourceBundle();
void CheckIcuStringSearch();
void CheckIcuUText();

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--idna-uts46") {
        CheckIcuIdnaUts46();
        std::puts("ICU UTS46 IDNA tests passed");
        return 0;
    }
    if (argc == 2) {
        const std::string_view mode(argv[1]);
        if (mode == "--idna-native") { CheckIcuIdnaNative(); return 0; }
        if (mode == "--idna-memory") { CheckIcuIdnaMemory(); return 0; }
#ifdef __linux__
        if (mode == "--idna-input-allocation") { CheckIcuIdnaInputAllocation(); return 0; }
#endif
        if (mode == "--idna-conformance-unicode") { CheckIcuIdnaConformance(0); return 0; }
        if (mode == "--idna-conformance-ascii-n") { CheckIcuIdnaConformance(1); return 0; }
        if (mode == "--idna-conformance-ascii-t") { CheckIcuIdnaConformance(2); return 0; }
    }
    CheckIcuCalendar();
    CheckIcuCollator();
    CheckIcuDateFormat();
    CheckIcuDateTimePatternGenerator();
    CheckIcuDateIntervalFormat();
    CheckIcuFieldPositionIterator();
    CheckIcuFormattedValue();
    CheckIcuIdna();
    CheckIcuLocaleDisplayNames();
    CheckIcuListFormatter();
    CheckIcuNumberFormat();
    CheckIcuNumberFormatter();
    CheckIcuNumberingSystem();
    CheckIcuPluralRules();
    CheckIcuRelativeDateTimeFormatter();
    CheckIcuResourceBundle();
    CheckIcuStringSearch();
    CheckIcuUText();
    std::puts("ICU recovered signature tests passed");
}

#include <cstdio>

void CheckIcuCalendar();
void CheckIcuCollator();
void CheckIcuDateFormat();
void CheckIcuDateTimePatternGenerator();
void CheckIcuDateIntervalFormat();
void CheckIcuFieldPositionIterator();
void CheckIcuFormattedValue();
void CheckIcuIdna();
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

int main() {
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

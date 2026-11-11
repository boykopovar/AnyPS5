[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | **Polski**

# O projekcie

Narzędzie do automatycznego portowania plików wykonywalnych na systemy Linux i Windows.

Wersja z samym relinkerem działa na macOS, także na procesorach Apple silicon.

[**Konwerter w przeglądarce**](https://boykopovar.github.io/AnyPS5/) - pliki pozostają na Twoim urządzeniu.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Zawiera [relinker](../../core/relinker), który konwertuje plik wykonywalny do natywnego formatu systemu docelowego, oraz implementacje [systemowych bibliotek prx](../../core/libs/prx) przystosowane do dynamicznego linkowania. Bez emulacji i bez osobnego procesu środowiska uruchomieniowego.

[**Discord**](https://discord.gg/x9naWVzC6A), [Użycie](USAGE.md), [Instrukcja kompilacji](../dev/BUILD.md), [Architektura](../dev/ARCHITECTURE.md), [Dług techniczny projektu](../dev/TechnicalDebt.md), [konwencje stylu kodu](../dev/CONVENTIONS.md), [współtworzenie projektu](../../CONTRIBUTING.md)

## Stan prac

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historia postępów](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Biblioteki systemowe: odsetek funkcji znanych dotąd projektowi (zadeklarowanych w [core/libs/prx](../../core/libs/prx)), a nie wszystkich funkcji systemowych PS5. Łączna liczba rośnie w miarę deklarowania kolejnych funkcji.</sub>

W nieobsługiwanych lub nieoczekiwanych stanach rzucany jest wyjątek C++, zwykle `std::runtime_error`. Nieprzechwycony wyjątek wypisuje komunikat z `what()` na stderr i kończy proces.

[Rekompilator shaderów](../../core/shader/recompiler/Recompiler.cpp) z powodzeniem generuje kod SPIR-V (weryfikowany za pomocą [Spirv-Tools](../../3rdparty/SPIRV-Tools) przy kompilacji z `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Kompatybilność

Stan poszczególnych gier i raporty z testów znajdują się w dyskusjach [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), po jednym wątku na tytuł.

## Mapowanie sterowania

Obsługiwane są kontrolery do gier zmapowane przez SDL, łącznie z gałkami analogowymi i spustami. Sterowanie klawiaturą i myszą można skonfigurować w pliku `anyps5-input.ini`. Listę obsługiwanych urządzeń i format konfiguracji znajdziesz w dokumencie [mapowanie sterowania](INPUT_MAPPING.md).

## Zastrzeżenia

Ten projekt jest przeznaczony do celów interoperacyjności, badań, archiwizacji i kompatybilności. Nie zawiera, nie rozpowszechnia ani nie wymaga oprogramowania chronionego prawem autorskim, firmware'u, kluczy kryptograficznych ani bibliotek własnościowych. Użytkownicy odpowiadają za to, by każdy plik binarny, którego używają z tym projektem, został pozyskany i był wykorzystywany zgodnie z obowiązującym prawem oraz warunkami jego licencji.

AnyPS5 jest niezależnym projektem. Nie jest powiązany z firmą Sony Interactive Entertainment ani przez nią autoryzowany czy popierany. „PlayStation” i „PS5” są znakami towarowymi Sony Interactive Entertainment Inc.

## Licencja

Ten projekt jest udostępniany na licencji GNU General Public License wyłącznie w wersji 2.

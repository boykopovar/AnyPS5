[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | **Català** | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md)

# Sobre

Eina per portar executables automàticament a Linux i Windows.

La compilació que només inclou el relinker funciona a macOS, inclòs Apple silicon.

[**Convertidor web**](https://boykopovar.github.io/AnyPS5/) - els fitxers es queden al teu dispositiu.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Inclou un [relinker](../../core/relinker) que converteix l'executable al format natiu del sistema de destinació i implementacions de les [biblioteques prx del sistema](../../core/libs/prx) aptes per a l'enllaçament dinàmic. Sense emulació ni un procés a part en temps d'execució.

[**Discord**](https://discord.gg/BHFztBPUe), [Ús](USAGE.md), [Instruccions de compilació](../dev/BUILD.md), [Arquitectura](../dev/ARCHITECTURE.md), [Deute tècnic del projecte](../dev/TechnicalDebt.md), [convencions d'estil de codi](../dev/CONVENTIONS.md), [com contribuir](../../CONTRIBUTING.md)

## Estat

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historial de progrés](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Biblioteques del sistema: percentatge de les funcions que el projecte coneix fins ara (declarades a [core/libs/prx](../../core/libs/prx)), no de totes les funcions del sistema de la PS5. El total creix a mesura que es declaren més funcions.</sub>

Les situacions no suportades o inesperades llancen una excepció de C++, normalment `std::runtime_error`. Una excepció que no es captura imprimeix el seu `what()` a stderr i acaba el procés.

El [recompilador de shaders](../../core/shader/recompiler/Recompiler.cpp) genera SPIR-V correctament (validat amb [Spirv-Tools](../../3rdparty/SPIRV-Tools) quan es compila amb `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilitat

L'estat de cada joc i els informes de proves són a les discussions de [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), un fil per títol.

## Assignació de controls

S'admeten els comandaments assignats per SDL, inclosos els sticks analògics i els gatells. Els controls de teclat i ratolí es poden configurar amb un fitxer `anyps5-input.ini`. Consulta [assignació de controls](INPUT_MAPPING.md) per veure els dispositius admesos i el format de configuració.

## Avís legal

Aquest projecte té com a finalitat la interoperabilitat, la recerca, la preservació i la compatibilitat. No inclou, no distribueix ni requereix programari protegit per drets d'autor, firmware, claus criptogràfiques ni biblioteques propietàries. Els usuaris són responsables d'assegurar-se que qualsevol binari que facin servir amb aquest projecte s'obté i s'utilitza d'acord amb les lleis aplicables i els termes de llicència corresponents.

AnyPS5 és un projecte independent i no està afiliat, autoritzat ni avalat per Sony Interactive Entertainment. "PlayStation" i "PS5" són marques comercials de Sony Interactive Entertainment Inc.

## Llicència

Aquest projecte té llicència GNU General Public License versió 2 únicament.

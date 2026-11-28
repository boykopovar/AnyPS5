[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md) | [Türkçe](README.tr.md) | [עברית](README.he.md) | [Deutsch](README.de.md) | **Română**

# Despre

Instrument pentru portarea automată a executabilelor în Linux și Windows.

Versiunea care conține exclusiv relinker-ul rulează pe macOS, inclusiv pe Apple silicon.

[**Convertor web**](https://boykopovar.github.io/AnyPS5/) - fișierele rămân pe dispozitivul dvs.

Include un [relinker](../../core/relinker) care convertește executabilul în formatul nativ al sistemului țintă și implementări ale [bibliotecilor PRX de sistem](../../core/libs/prx) adecvate pentru legarea dinamică. Fără emulare și fără un proces separat de rulare.

[**Discord**](https://discord.gg/x9naWVzC6A), [Utilizare](USAGE.md), [Instrucțiuni de compilare](../dev/BUILD.md), [Arhitectură](../dev/ARCHITECTURE.md), [Datoria tehnică a proiectului](../dev/TechnicalDebt.md), [convenții de stil pentru cod](../dev/CONVENTIONS.md), [cum să contribuiți](../../CONTRIBUTING.md)

## Stare

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Istoricul progresului](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Biblioteci de sistem: procentajul funcțiilor cunoscute proiectului până în prezent (declarate în [core/libs/prx](../../core/libs/prx)), nu al tuturor funcțiilor de sistem PS5. Totalul crește pe măsură ce sunt declarate mai multe funcții.</sub>

Stările neacceptate sau neașteptate generează o excepție C++, în mod obișnuit `std::runtime_error`. O excepție pe care nimeni nu o tratează afișează `what()` pe stderr și încheie procesul.

[Recompilatorul de shadere](../../core/shader/recompiler/Recompiler.cpp) generează cu succes SPIR-V (validat prin [Spirv-Tools](../../3rdparty/SPIRV-Tools) atunci când este compilat cu `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilitate

Starea fiecărui joc și rapoartele de testare sunt disponibile în discuțiile [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), câte un fir pentru fiecare titlu.

## Maparea comenzilor

Controlerele compatibile cu SDL sunt acceptate, inclusiv stick-urile analogice și triggerele. Controalele de tastatură și mouse pot fi configurate printr-un fișier `anyps5-input.ini`. Consultați [maparea comenzilor](INPUT_MAPPING.md) pentru dispozitivele acceptate și formatul de configurare.

## Renunțare la răspundere

Acest proiect este destinat interoperabilității, cercetării, conservării și compatibilității. Nu include, nu distribuie și nu necesită software protejat prin drepturi de autor, firmware, chei criptografice sau biblioteci proprietare. Utilizatorii sunt responsabili să se asigure că toate fișierele binare utilizate cu acest proiect sunt obținute și utilizate în conformitate cu legile aplicabile și cu termenii de licență respectivi.

AnyPS5 este un proiect independent și nu este afiliat, autorizat sau aprobat de Sony Interactive Entertainment. „PlayStation” și „PS5” sunt mărcile înregistrate ale Sony Interactive Entertainment Inc.

## Licență

Acest proiect este licențiat exclusiv sub versiunea 2 a GNU General Public License.

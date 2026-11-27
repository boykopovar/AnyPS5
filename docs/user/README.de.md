[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | [العربية](README.ar.md) | [Ελληνικά](README.el.md) | [Türkçe](README.tr.md) | [עברית](README.he.md) | **Deutsch** | [Română](README.ro.md)

# Über das Projekt

Werkzeug zur automatischen Portierung ausführbarer Dateien auf Linux und Windows.

Der Build, der nur den Relinker enthält, läuft auch unter macOS, einschließlich Apple Silicon.

[**Web-Konverter**](https://boykopovar.github.io/AnyPS5/) - die Dateien bleiben auf deinem Gerät.

Enthält einen [Relinker](../../core/relinker), der die ausführbare Datei in das native Format des Zielsystems umwandelt, sowie Implementierungen der [System-prx-Bibliotheken](../../core/libs/prx), die sich für dynamisches Linken eignen. Keine Emulation und kein separater Laufzeitprozess.

[**Discord**](https://discord.gg/x9naWVzC6A), [Verwendung](USAGE.md), [Build-Anleitung](../dev/BUILD.md), [Architektur](../dev/ARCHITECTURE.md), [Technische Schulden des Projekts](../dev/TechnicalDebt.md), [Code-Stil-Konventionen](../dev/CONVENTIONS.md), [Mitwirken](../../CONTRIBUTING.md)

## Stand

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Fortschrittsverlauf](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Systembibliotheken: Anteil der Funktionen, die dem Projekt bisher bekannt sind (deklariert in [core/libs/prx](../../core/libs/prx)), nicht aller PS5-Systemfunktionen. Die Gesamtzahl wächst, je mehr Funktionen deklariert werden.</sub>

Nicht unterstützte oder unerwartete Zustände lösen eine C++-Ausnahme aus, meist `std::runtime_error`. Eine Ausnahme, die nicht abgefangen wird, gibt ihr `what()` auf stderr aus und beendet den Prozess.

Der [Shader-Recompiler](../../core/shader/recompiler/Recompiler.cpp) erzeugt erfolgreich SPIR-V (validiert mit [Spirv-Tools](../../3rdparty/SPIRV-Tools), wenn mit `ANYPS5_ENABLE_SPIRV_TOOLS` gebaut wird).

## Kompatibilität

Status und Testberichte einzelner Spiele finden sich in den Diskussionen unter [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), ein Thread pro Titel.

## Eingabezuordnung

Von SDL zugeordnete Gamecontroller werden unterstützt, einschließlich Analogsticks und Trigger. Tastatur- und Maussteuerung lassen sich über eine Datei `anyps5-input.ini` konfigurieren. Die unterstützten Geräte und das Konfigurationsformat sind unter [Eingabezuordnung](INPUT_MAPPING.md) beschrieben.

## Haftungsausschluss

Dieses Projekt dient der Interoperabilität, Forschung, Archivierung und Kompatibilität. Es enthält, verbreitet oder benötigt keine urheberrechtlich geschützte Software, Firmware, kryptografischen Schlüssel oder proprietären Bibliotheken. Die Nutzer sind dafür verantwortlich, dass alle Binärdateien, die sie mit diesem Projekt verwenden, im Einklang mit geltendem Recht und den jeweiligen Lizenzbedingungen beschafft und genutzt werden.

AnyPS5 ist ein unabhängiges Projekt und steht in keiner Verbindung zu Sony Interactive Entertainment, das es weder autorisiert noch unterstützt. „PlayStation“ und „PS5“ sind Marken der Sony Interactive Entertainment Inc.

## Lizenz

Dieses Projekt steht ausschließlich unter der GNU General Public License Version 2.

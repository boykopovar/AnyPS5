[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | [日本語](README.ja.md) | **Deutsch**

# Über das Projekt

Werkzeug zur automatischen Portierung von ausführbaren Dateien auf Linux und Windows.

Der reine Relinker-Build läuft unter macOS, einschließlich Apple Silicon.

[**Web-Konverter**](https://boykopovar.github.io/AnyPS5/) - die Dateien bleiben auf Ihrem Gerät.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Enthalten sind ein [Relinker](../../core/relinker), der die ausführbare Datei in das native Format des Zielsystems umwandelt, sowie Implementierungen der [System-prx-Bibliotheken](../../core/libs/prx), die sich für dynamisches Linken eignen. Es gibt keine Emulation und keinen separaten Laufzeitprozess.

[**Discord**](https://discord.gg/x9naWVzC6A), [Nutzung](USAGE.md), [Build-Anleitung](../dev/BUILD.md), [Architektur](../dev/ARCHITECTURE.md), [Technische Schulden des Projekts](../dev/TechnicalDebt.md), [Code-Style-Konventionen](../dev/CONVENTIONS.md), [Mitwirken](../../CONTRIBUTING.md)

## Status

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![RDNA2 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![gfx1013 reference shader forms decoded](https://boykopovar.github.io/AnyPS5/badge-shaders-gfx1013.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Fortschrittsverlauf](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Systembibliotheken: Anteil an den Funktionen, die dem Projekt bisher bekannt sind (deklariert in [core/libs/prx](../../core/libs/prx)), nicht an allen Systemfunktionen der PS5. Die Gesamtzahl wächst, sobald weitere Funktionen deklariert werden.</sub>

Nicht unterstützte oder unerwartete Zustände lösen eine C++-Ausnahme aus, meist `std::runtime_error`. Eine Ausnahme, die nirgends abgefangen wird, gibt ihr `what()` auf stderr aus und beendet den Prozess.

Der [Shader-Recompiler](../../core/shader/recompiler/Recompiler.cpp) erzeugt erfolgreich SPIR-V (bei einem Build mit `ANYPS5_ENABLE_SPIRV_TOOLS` mit [Spirv-Tools](../../3rdparty/SPIRV-Tools) validiert).

## Kompatibilität

Status und Testberichte je Spiel stehen in den [Game-bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up)-Diskussionen, ein Thread pro Titel.

## Eingabezuordnung

SDL-zugeordnete Gamecontroller werden unterstützt, einschließlich Analogsticks und Triggern. Tastatur- und Maussteuerung lassen sich über eine Datei `anyps5-input.ini` konfigurieren. Die unterstützten Geräte und das Konfigurationsformat stehen unter [Eingabezuordnung](INPUT_MAPPING.md).

## Haftungsausschluss

Dieses Projekt dient der Interoperabilität, der Forschung, der Bewahrung und der Kompatibilität. Es enthält, verbreitet oder benötigt keine urheberrechtlich geschützte Software, Firmware, kryptografischen Schlüssel oder proprietären Bibliotheken. Die Nutzer sind dafür verantwortlich, dass alle mit diesem Projekt verwendeten Binärdateien im Einklang mit dem geltenden Recht und den jeweiligen Lizenzbedingungen beschafft und verwendet werden.

AnyPS5 ist ein unabhängiges Projekt und steht in keiner Verbindung zu Sony Interactive Entertainment, wird von dieser weder autorisiert noch unterstützt. „PlayStation“ und „PS5“ sind Marken von Sony Interactive Entertainment Inc.

## Lizenz

Dieses Projekt steht ausschließlich unter der GNU General Public License Version 2.

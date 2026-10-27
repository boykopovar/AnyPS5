[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [简体中文](README.zh-CN.md) | **Русский**

# О проекте

Инструмент для автоматического портирования исполняемых файлов на Linux и Windows.

Сборка только relinker работает на macOS, включая Apple silicon.

[**Веб-конвертер**](https://boykopovar.github.io/AnyPS5/) - файлы остаются на вашем устройстве.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Включает [relinker](../../core/relinker), который преобразует исполняемый файл в нативный формат целевой системы, и реализации [системных prx-библиотек](../../core/libs/prx), подходящие для динамической линковки. Без эмуляции и отдельного процесса среды выполнения.

[**Discord**](https://discord.gg/BHFztBPUe), [Использование](USAGE.md), [Инструкции по сборке](../dev/BUILD.md), [Архитектура](../dev/ARCHITECTURE.md), [Технический долг проекта](../dev/TechnicalDebt.md), [соглашения о стиле кода](../dev/CONVENTIONS.md), [участие в разработке](../../CONTRIBUTING.md)

## Статус

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[История прогресса](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Системные библиотеки: процент функций, известных проекту на данный момент (объявленных в [core/libs/prx](../../core/libs/prx)), а не всех системных функций PS5. Общее число растёт по мере объявления новых функций.</sub>

Неподдерживаемые или неожиданные состояния строго выбрасывают `std::runtime_error`. `what()` выводится в stderr, и процесс завершается.

[Рекомпилятор шейдеров](../../core/shader/recompiler/Recompiler.cpp) успешно генерирует SPIR-V (проверяется через [Spirv-Tools](../../3rdparty/SPIRV-Tools) при сборке с `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Совместимость

Статус каждой игры и отчёты о тестировании находятся в обсуждениях [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), по одной теме на игру.

## Назначение управления

Поддерживаются игровые контроллеры с раскладкой SDL, включая аналоговые стики и триггеры. Управление с клавиатуры и мыши настраивается файлом `anyps5-input.ini`. Поддерживаемые устройства и формат конфигурации описаны в разделе [назначение управления](INPUT_MAPPING.md).

## Отказ от ответственности

Проект предназначен для целей совместимости, взаимодействия, исследований и сохранения. Он не содержит, не распространяет и не требует программного обеспечения, защищённого авторским правом, прошивок, криптографических ключей или проприетарных библиотек. Пользователи несут ответственность за то, чтобы любые бинарные файлы, используемые с этим проектом, были получены и использовались в соответствии с применимым законодательством и условиями соответствующих лицензий.

AnyPS5 — независимый проект, не связанный с Sony Interactive Entertainment, не уполномоченный и не одобренный ею. «PlayStation» и «PS5» являются товарными знаками Sony Interactive Entertainment Inc.

## Лицензия

Проект распространяется по лицензии GNU General Public License только версии 2.

[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | **Español** | [简体中文](README.zh-CN.md) | [Русский](README.ru.md)

# Acerca de

Herramienta para portar ejecutables automáticamente a Linux y Windows.

La compilación solo del relinker funciona en macOS, incluido Apple silicon.

[**Conversor web**](https://boykopovar.github.io/AnyPS5/) - los archivos permanecen en tu dispositivo.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Incluye un [relinker](../../core/relinker) que convierte el ejecutable al formato nativo del sistema de destino e implementaciones de las [bibliotecas prx del sistema](../../core/libs/prx) aptas para el enlazado dinámico. Sin emulación ni proceso de runtime separado.

[**Discord**](https://discord.gg/BHFztBPUe), [Uso](USAGE.md), [Instrucciones de compilación](../dev/BUILD.md), [Arquitectura](../dev/ARCHITECTURE.md), [Deuda técnica del proyecto](../dev/TechnicalDebt.md), [convenciones de estilo de código](../dev/CONVENTIONS.md), [cómo contribuir](../../CONTRIBUTING.md)

## Estado

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Historial de progreso](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Bibliotecas del sistema: porcentaje de las funciones conocidas por el proyecto hasta ahora (declaradas en [core/libs/prx](../../core/libs/prx)), no de todas las funciones del sistema de PS5. El total crece a medida que se declaran más funciones.</sub>

Los estados no soportados o inesperados lanzan una excepción de C++, normalmente `std::runtime_error`. Una excepción que nada captura imprime su `what()` en stderr y termina el proceso.

El [recompilador de shaders](../../core/shader/recompiler/Recompiler.cpp) produce SPIR-V correctamente (validado mediante [Spirv-Tools](../../3rdparty/SPIRV-Tools) cuando se compila con `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilidad

El estado de cada juego y los informes de prueba están en las discusiones de [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), un hilo por título.

## Mapeo de entrada

Se admiten mandos mapeados por SDL, incluidos los sticks analógicos y los gatillos. Los controles de teclado y ratón se pueden configurar con un archivo `anyps5-input.ini`. Consulta [mapeo de entrada](INPUT_MAPPING.md) para ver los dispositivos admitidos y el formato de configuración.

## Aviso legal

Este proyecto está destinado a fines de interoperabilidad, investigación, preservación y compatibilidad. No incluye, distribuye ni requiere software protegido por derechos de autor, firmware, claves criptográficas ni bibliotecas propietarias. Los usuarios son responsables de garantizar que cualquier binario utilizado con este proyecto se obtenga y utilice de acuerdo con las leyes aplicables y sus respectivos términos de licencia.

AnyPS5 es un proyecto independiente y no está afiliado, autorizado ni respaldado por Sony Interactive Entertainment. "PlayStation" y "PS5" son marcas registradas de Sony Interactive Entertainment Inc.

## Licencia

Este proyecto está licenciado bajo la GNU General Public License versión 2 únicamente.

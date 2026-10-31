[English](../../README.md) | **Português (Brasil)** | [Español](README.es.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md)

# Sobre

Ferramenta para portar executáveis automaticamente para Linux e Windows.

O build somente do relinker roda no macOS, incluindo Apple silicon.

[**Conversor web**](https://boykopovar.github.io/AnyPS5/) - os arquivos ficam no seu dispositivo.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Inclui um [relinker](../../core/relinker) que converte o executável para o formato nativo do sistema de destino e implementações das [bibliotecas prx do sistema](../../core/libs/prx) adequadas para linkagem dinâmica. Sem emulação nem processo de runtime separado.

[**Discord**](https://discord.gg/BHFztBPUe), [Uso](USAGE.md), [Instruções de build](../dev/BUILD.md), [Arquitetura](../dev/ARCHITECTURE.md), [Dívida técnica do projeto](../dev/TechnicalDebt.md), [convenções de estilo de código](../dev/CONVENTIONS.md), [como contribuir](../../CONTRIBUTING.md)

## Status

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Histórico de progresso](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Bibliotecas do sistema: verde é implementada, vermelho é um stub observado (declarada, e um uso medido mostra que ela é necessária) e cinza é um stub não observado (declarada, sem nenhum uso medido conhecido; isso não significa que nenhum título a use; nos shaders, cinza é uma instrução não implementada). A porcentagem principal é implementadas / (implementadas + stubs observados), então os cinzas não a reduzem; o valor declarado na página de progresso conta todas as funções declaradas. Só são contadas as funções declaradas em [core/libs/prx](../../core/libs/prx), não todas as funções de sistema do PS5. Mais detalhes em [docs/dev/PROGRESS.md](../dev/PROGRESS.md#observed-imports).</sub>

Estados não suportados ou inesperados lançam uma exceção C++, normalmente `std::runtime_error`. Uma exceção que nada captura imprime seu `what()` no stderr e encerra o processo.

O [recompilador de shaders](../../core/shader/recompiler/Recompiler.cpp) produz SPIR-V com sucesso (validado via [Spirv-Tools](../../3rdparty/SPIRV-Tools) quando compilado com `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Compatibilidade

O status de cada jogo e os relatórios de teste ficam nas discussões de [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), uma thread por título.

## Mapeamento de entrada

Controles mapeados pelo SDL são suportados, incluindo analógicos e gatilhos. Os controles de teclado e mouse podem ser configurados com um arquivo `anyps5-input.ini`. Veja [mapeamento de entrada](INPUT_MAPPING.md) para os dispositivos suportados e o formato de configuração.

## Aviso legal

Este projeto destina-se a fins de interoperabilidade, pesquisa, preservação e compatibilidade. Ele não inclui, distribui nem exige software protegido por direitos autorais, firmware, chaves criptográficas ou bibliotecas proprietárias. Os usuários são responsáveis por garantir que quaisquer binários usados com este projeto sejam obtidos e utilizados de acordo com as leis aplicáveis e os respectivos termos de licença.

O AnyPS5 é um projeto independente e não é afiliado, autorizado nem endossado pela Sony Interactive Entertainment. "PlayStation" e "PS5" são marcas registradas da Sony Interactive Entertainment Inc.

## Licença

Este projeto é licenciado sob a GNU General Public License versão 2 apenas.

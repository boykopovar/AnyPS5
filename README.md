# AnyPS5 - Camada de Tradução (Fork: PC_Station_5)

Este repositório é um fork do projeto original [AnyPS5](https://github.com/boykopovar/AnyPS5 "AnyPS5 GitHub"), focado em traduzir, documentar e adaptar o desenvolvimento para **Português (Brasil)**.

O projeto é uma ferramenta para portabilidade automática de executáveis nativos de PS5 para Linux e Windows.

Inclui um [relinker](core/relinker) que converte o executável para o formato nativo do sistema alvo e implementações de [bibliotecas de sistema prx](core/libs/prx) adequadas para ligação dinâmica. Não há emulação ou processo de runtime separado.

## Status do Projeto

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/)
[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/)

<sub>* Bibliotecas de sistema: porcentagem de funções conhecidas até agora (declaradas em [core/libs/prx](core/libs/prx)), não de cada função do sistema PS5. O total cresce à medida que mais funções são declaradas.</sub>

O [recompilador de shaders](core/shader/recompiler/Recompiler.cpp) produz com sucesso SPIR-V (validado via [Spirv-Tools](3rdparty/SPIRV-Tools) quando compilado com `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Guia de Uso Passo a Passo

Os caminhos são relativos ao executável gerado:

```text
app.elf (Linux) ou app.exe (Windows)
libs/
    *.prx
app0/
    <recursos do app>
    sce_module/
        <módulos convertidos>
```

Para usar, substitua `sce_module/` por `prx/` ou similar dependendo do diretório de entrada. Coloque os recursos em `app0/`. Copie as bibliotecas construídas de `build/core/libs/libs/*.prx` para `libs/`.

Linux:
```sh
chmod +x app.elf
./app.elf
```

Windows PowerShell:
```powershell
.\app.exe
```

## Instruções Detalhadas de Compilação

Para compilar apenas o relinker:

```sh
cmake -S . -B build-relinker -G Ninja -DCMAKE_BUILD_TYPE=Release -DANYPS5_RELINKER_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-relinker --parallel
ctest --test-dir build-relinker --output-on-failure
```

Para a compilação completa (requer inicialização de submódulos):

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --parallel
cmake --build build --target libs --parallel
```

Certifique-se de ter dependências como SDL2, Vulkan, e FFmpeg devidamente configuradas.

## Referências e Original

Este projeto é um trabalho derivado de código aberto. Todo o crédito da arquitetura original vai para o [AnyPS5](https://github.com/boykopovar/AnyPS5).

## Compatibilidade

Veja a [lista de compatibilidade de jogos](docs/user/COMPATIBILITY.md) (documento em inglês no repositório original) para jogos testados e problemas conhecidos.

## Aviso Legal

Este projeto destina-se a interoperabilidade, pesquisa, preservação e fins de compatibilidade. Ele não inclui, distribui ou requer software protegido por direitos autorais, firmware, chaves criptográficas ou bibliotecas proprietárias. Os usuários são responsáveis por garantir que quaisquer binários usados com este projeto sejam obtidos e usados de acordo com as leis aplicáveis e seus respectivos termos de licença.

## Licença

Este projeto é licenciado sob a GNU General Public License versão 2 apenas.

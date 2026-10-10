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

## Progresso e Mapeamento

Nós acompanhamos ativamente a quantidade de NIDs (Network IDs - assinaturas de funções do PS5) que já foram mapeados e traduzidos na nossa camada PC_Station_5. Assim como no projeto original, a métrica de progresso avança à medida em que funções do Kernel e opcodes de Shaders RDNA2 são declarados dentro de `core/libs/prx` e do recompilador de shaders.

*(Nota: Os gráficos SVG dinâmicos estão referenciados nas badges no topo do arquivo. Com a evolução dos nossos PRs internos, atualizaremos as porcentagens correspondentes ao volume de syscalls traduzidas em nosso framework de dispatch).*

## Créditos e Referências Técnicas

Este projeto é um trabalho colaborativo e derivado de código aberto focado em emulação, tradução e preservação do ecossistema PlayStation 5. Gostaríamos de creditar explicitamente as seguintes iniciativas e seus desenvolvedores originais:

*   **[AnyPS5](https://github.com/boykopovar/AnyPS5):** Pela arquitetura original completa de Relinker (conversão ELF para PE/Linux Native) e pelas bibliotecas estáticas (PRX reimplementadas).
*   **[KytyPS5](https://github.com/KytyPS5/KytyPS5):** Pela vasta pesquisa na tradução de Buffers de Comando Prospero / AGC (RDNA 2) e mapeamento da camada System V / POSIX do console.
*   **[SharpEmu](https://github.com/sharpemu/sharpemu):** Pelo detalhamento minucioso da estrutura do Kernel (como a alocação `sceKernelReserveVirtualRange`) e limites operacionais de memória do host (Windows/C# mappings).

## Compatibilidade

Veja a [lista de compatibilidade de jogos](docs/user/COMPATIBILITY.md) (documento em inglês no repositório original) para jogos testados e problemas conhecidos.

## Aviso Legal

Este projeto destina-se a interoperabilidade, pesquisa, preservação e fins de compatibilidade. Ele não inclui, distribui ou requer software protegido por direitos autorais, firmware, chaves criptográficas ou bibliotecas proprietárias. Os usuários são responsáveis por garantir que quaisquer binários usados com este projeto sejam obtidos e usados de acordo com as leis aplicáveis e seus respectivos termos de licença.

## Licença

Este projeto é licenciado sob a GNU General Public License versão 2 apenas.

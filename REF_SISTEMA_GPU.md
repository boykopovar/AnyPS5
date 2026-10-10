# Guia de Sistema e GPU (REF_SISTEMA_GPU.md)

Este documento atua como mapeamento das bibliotecas do sistema operacional do PS5, chamadas de sistema (syscalls) nativas e as equivalências para plataformas PC (Win32 e POSIX/Linux).

## 1. Bibliotecas do Sistema (Módulos `.prx`)

As bibliotecas estáticas e dinâmicas originais do PS5 (arquivos `.prx`) não podem ser simplesmente transferidas para o PC devido a diferenças de arquitetura de SO (FreeBSD modificado vs Windows/Linux). A camada de tradução reimplementa esses módulos no diretório `core/libs/prx` do projeto.

### 1.1 Libc (C Standard Library)
* **Objetivo:** Fornece as funções padrão de C.
* **Mapeamento:** O relinker vincula dinamicamente as chamadas do jogo para as versões Win32 ou glibc compatíveis na máquina host, implementadas via wrappers em `core/libs`.

### 1.2 Bibliotecas Gráficas (libSceAgc / libSceGnm)
* **Objetivo:** Controle direto e buffers de comandos para a GPU.
* **Comportamento Nativo:** O hardware RDNA 2 utiliza chamadas específicas de baixo nível (ex. `sceAgcCreateShader`).
* **Mapeamento para Vulkan 1.3:**
  * Chamadas proprietárias são encapsuladas e traduzidas para filas (queues) Vulkan.
  * O estado da memória de VRAM e mapeamentos diretos dependem das extensões do driver host.

## 2. Chamadas de Sistema (Syscalls)

O sistema do console expõe várias syscalls, muitas baseadas em FreeBSD mas altamente modificadas.

### 2.1 Alocação de Memória Direta (Direct Memory)
* **Função PS5:** `sceKernelAllocateDirectMemory`
* **Limites:** Títulos podem alocar até ~13.8 GB de memória para acesso direto (CPU/GPU).
* **Tradução Windows:** A memória é inteiramente alocada (committed) no ato da chamada usando `VirtualAlloc`. Isso exige que o tamanho do pagefile somado à RAM seja suficiente na máquina host, diferente do comportamento lazy do console.
* **Tradução Linux:** Feita importando buffers dma-buf (via `/dev/udmabuf`) caso o driver suporte compartilhamento de memória com Vulkan, ou via cópia convencional (fallback).

### 2.2 Threads e Sincronização
* **Função PS5:** `scePthread*` (Variantes do POSIX Threads)
* **Tradução:**
  * Windows: Utiliza as APIs `libwinpthread` ou implementações nativas via wrappers.
  * Linux: Mapeado diretamente para as syscalls glibc nativas (`pthread`).

## 3. Gestão da GPU RDNA 2 via Vulkan
* **Pipeline Graphics / Compute:** Requer suporte a Vulkan 1.1+ (de preferência 1.3) e apresentação de swapchain no host.
* O tradutor converte pacotes de buffer de comando do console (PM4 / comandos AGC) em estruturas de Vulkan que podem ser enviadas à GPU do PC nativamente.

*Nota: Esta referência será atualizada continuamente conforme a Engenharia Reversa do projeto AnyPS5 (e derivados) avançar.*
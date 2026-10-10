# Referência Arquitetural de GPUs Host (REF_VENDORS_GPU.md)

Este documento centraliza as peculiaridades de arquitetura de hardware das placas de vídeo de PC (NVIDIA e AMD Desktop) que impactam diretamente a otimização da camada de tradução do PS5, a fim de extrair o máximo de desempenho sem gargalos na tradução do SPIR-V.

## 1. Topologia de Execução (Wavefronts vs Warps)

A GPU do PS5 (AMD RDNA 2) agrupa as threads de processamento em blocos chamados **Wavefronts** (ou Waves). No RDNA 2, eles podem operar em dois modos: **Wave32** (32 threads) ou **Wave64** (64 threads).

### NVIDIA (Ampere, Ada Lovelace, etc)
*   **Terminologia:** Utiliza o termo **Warp**, que é **estritamente travado em 32 threads**.
*   **Problema de Tradução:** Se o shader original do PS5 exigir sincronização cruzada explícita (cross-lane operations) assumindo um Wave64, a placa NVIDIA não conseguirá executar isso nativamente em um único Warp.
*   **Otimização Exigida:** No recompilador (`SpirvBackend`), instruções de subgrupo (Subgroup) como `OpGroupNonUniform*` no SPIR-V devem ser emitidas com cuidado. Se o tamanho garantido de subgrupo na NVIDIA for 32, a simulação de Wave64 exigirá memória compartilhada (Shared Memory) extra, resultando em impacto de latência. Onde for possível, provar ao compilador que operações são independentes.

### AMD (Desktop RDNA 2 / RDNA 3)
*   **Vantagem Nativa:** A tradução para hardware AMD moderno no PC flui 1:1 na maioria das operações vetorizadas e escalares.
*   **Barreiras de Cache:** O driver Windows/Linux da AMD no PC pode tratar o cache L0/L1 de forma levemente diferente da APU do console, tornando instruções pesadas como `SWaitcnt` (inseridas pelo AnyPS5) críticas para evitar falhas visuais.

## 2. Abstração de Memória (UMA vs Memória Dedicada)

O PS5 possui **Unified Memory Architecture (UMA)**. CPU e GPU compartilham fisicamente os mesmos módulos GDDR6.

### Placas Dedicadas (NVIDIA PCIe / AMD PCIe)
*   No PC, há a memória do sistema (DDR4/DDR5) e a VRAM (GDDR6).
*   Se o jogo mapear buffers pesados no PS5 assumindo acesso imediato (Zero-Copy) e tentarmos isso no PC via PCIe (Resizable BAR ativado ou não), o barramento se tornará um gargalo massivo.
*   **Solução Arquitetural:** Sempre preferir memória Local à GPU (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`) para buffers estáticos. Memórias compartilhadas (`HOST_VISIBLE | HOST_COHERENT`) devem ser estritamente reservadas para command buffers temporários e ring buffers de streaming constante (como áudio ou geometria dinâmica leve).

## 3. Otimizações de Driver Específicas

*   **NVIDIA Thread Group Synchronization:** Em shaders de Compute (Direct Compute no PS5 traduzido para Vulkan Compute), a NVIDIA favorece barreiras de barreira globais otimizadas. Reduzir as dependências intra-warp acelera a alocação de registradores (Occupancy).
*   **Compilador de Shaders:** Drivers NVIDIA recompilam SPIR-V de forma extremamente agressiva. Loops desenrolados (unrolled) excessivamente no nosso recompilador do PS5 podem sobrecarregar o cache de instruções da NVIDIA, causando stutters (engasgos de compilação) de shader cache.
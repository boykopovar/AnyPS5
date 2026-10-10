# Referência de APIs Gráficas (REF_API_GRAFICA.md)

Este documento descreve como mapeamos a API nativa do PS5 (GNM/AGC - arquitetura baseada em command buffers diretos) para APIs de PC de alto nível/baixo nível, com foco primário em **Vulkan 1.3** e referencial secundário no **DirectX 12**.

## 1. Vulkan 1.3 (Pipeline Primário)

O projeto PC_Station_5 (baseado em AnyPS5 e Kyty) utiliza o Vulkan como a principal API de tradução gráfica cruzada (Windows/Linux) para manter o overhead em níveis mínimos.

### 1.1 Tradução de Shaders (SPIR-V)
A base do Vulkan. Como a AMD (PS5) gera bytecode GCN/RDNA bruto, precisamos recompilá-los:
*   O SPIR-V gerado deve ser focado no perfil Vulkan 1.1/1.3.
*   **Bindless Textures (Descriptor Indexing):** Jogos modernos de console utilizam arrays massivos e desestruturados de descritores de textura na memória. O Vulkan resolve isso maravilhosamente com a extensão `VK_EXT_descriptor_indexing`, essencial para mapear a flexibilidade do PS5 no PC. (Opcodes de GPU que dão fetch em descritores a partir de registradores de memória precisam disso ativo).

### 1.2 Gerenciamento de Memória Nativa (Linux / dma-buf)
No ecossistema Linux, a alocação `sceKernelAllocateDirectMemory` do PS5 pode ser espelhada na GPU eficientemente sem cópias usando o framework **dma-buf** (`/dev/udmabuf`).
*   Ele permite que o buffer alocado pelo Kernel do host Linux seja passado diretamente ao driver de vídeo Vulkan (extensão `VK_EXT_external_memory_dma_buf`), permitindo que a CPU grave buffers de comando e a GPU os consuma simultaneamente como se fosse um console.

### 1.3 Sincronização (Timeline Semaphores)
O PS5 envia buffers massivos e usa interrupts de GPU nativos para notificar a CPU de seu encerramento. No Vulkan 1.2+, nós traduzimos isso 1:1 usando **Timeline Semaphores**, reduzindo drasticamente os bloqueios em comparação aos semáforos binários legados do Vulkan 1.0.

## 2. DirectX 12 (Referencial de Otimização no Windows)

Embora o Vulkan seja a principal porta, o ambiente Windows possui especificidades rígidas no DX12 que inspiram técnicas de bypass e otimização.

*   **Root Signatures vs. AGC Registers:** A arquitetura AGC do PS5 envia dados (constantes/descritores) diretamente a registradores da GPU. Em DX12, o equivalente direto são os *Root Parameters*. Na camada de Vulkan, simulamos isso através de **Push Constants**, embora possuam limites de tamanho (geralmente 128 bytes/256 bytes na NVIDIA/AMD). Se o estado passar disso, migra-se para Uniform Buffer Objects (UBOs).
*   **VirtualAlloc Limitations:** Como abordado na base de tradução, ao replicar `MEM_RESERVE` e paginação explícita no Windows, o gerenciamento de páginas Tiled Resources (DX12) é conceitualmente similar à alocação de tabela de páginas crua do hardware original do console.
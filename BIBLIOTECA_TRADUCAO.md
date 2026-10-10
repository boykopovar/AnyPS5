# Biblioteca de Tradução Interna (BIBLIOTECA_TRADUCAO.md)

Este repositório interno descreve as instruções, opcodes e convenções de funções da camada de tradução do ecossistema PlayStation 5 para as equivalências de hardware de PC, com a finalidade de servir de consulta rápida e base autônoma.

## Módulo: Funções de Sistema (Syscalls e Memory)

### [NID_UNMAPPED] - sceKernelAllocateDirectMemory
- **Funcionamento Técnico:** Aloca memória contígua e diretamente endereçável pela CPU e GPU (Direct Memory), normalmente usando grandes blocos de paginação (huge pages) por questão de performance no RDNA2.
- **Mapeamento Equivalente (Win32):** Alocação completa usando `VirtualAlloc` com flags de commit imediato, que reserva RAM e Pagefile reais.
- **Mapeamento Equivalente (Linux):** Exportação dma-buf associada a drivers Vulkan, ou shm padrão/fallback com cópia de memória.
- **Exemplo Prático Comentado:**
  ```cpp
  // Na implementação de libSceKernel.prx do wrapper (simplificado):
  void* allocateDirectMemory(size_t size, int alignment) {
      #ifdef _WIN32
      // No Windows, toda memória deve ser comitada no momento da chamada.
      return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      #endif
  }
  ```

### [NID_UNMAPPED] - sceKernelLoadStartModule
- **Funcionamento Técnico:** Carrega uma biblioteca dinâmica do sistema do console (PRX/ELF) na memória do processo do título.
- **Mapeamento Equivalente:** No ecossistema do relinker, os módulos são escaneados estaticamente. Em tempo de execução, redirecionamos para os carregadores de biblioteca padrão do SO: `LoadLibrary` (Win32) ou `dlopen` (POSIX).

## Módulo: Shaders e Gráficos (libSceAgc)

### [NID_UNMAPPED] - sceAgcCreateShader
- **Funcionamento Técnico:** Cria e compila os objetos shader para a arquitetura gráfica (RDNA2).
- **Mapeamento Equivalente (SPIR-V / Vulkan):** A camada de tradução captura o bytecode do shader nativo da AMD e recompila utilizando `core/shader/recompiler` para produzir código intermediário SPIR-V, validado via SPIRV-Tools e passado ao driver Vulkan pelo `vkCreateShaderModule`.
- **Exemplo Prático Comentado:**
  ```cpp
  // O Recompiler recebe um ponteiro para os binários de GPU e os recompila.
  std::vector<uint32_t> spirv_code = recompiler.CompileToSpirv(ps5_shader_bytecode);
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = spirv_code.size() * sizeof(uint32_t);
  createInfo.pCode = spirv_code.data();
  // ... repassa ao vkCreateShaderModule
  ```

*Aviso: Este arquivo é progressivo e deverá ser populado com NIDs (Network IDs - Hashes de Funções) validados à medida que o parsing de executáveis do sistema (`core/libs/prx`) progredir na Fase 4.*
### [NID_UNMAPPED] - sceKernelReserveVirtualRange
- **Funcionamento Técnico:** Reserva intervalos virtuais gigantes no Address Space do jogo (podem passar de centenas de GiB) para mapeamentos futuros (Virtual Allocations).
- **Mapeamento Equivalente:** Comportamento conhecido em SharpEmu - Requer thresholds para lidar com esses blocos grandes (ex: `SparseReservationThreshold` em ~64 GiB). No Windows é difícil achar chunks contíguos dessa magnitude sem o flag correto (`MEM_RESERVE` puro). No Linux, requer atenção a overcommits.

### Módulo: Sincronização Posix do PS5 (libSceLibcInternal)
As chamadas de sistema abaixo usam a ABI SystemV (estilo POSIX) e foram mapeadas por projetos C/C# (Kyty/SharpEmu):
- **pthread_create_name_np**: Criação de thread com extensão de nomeamento.
- **PthreadAttrSetsolosched**: Atribuição de afinidade e agendamento solitário em threads.
- **KernelSyncOnAddress**: Primitiva fundamental de mutex baseada em Futex, usada para concorrência de GPU e CPU do PS5.


### Atualização Fase 4 (Mapeamento PRX/Kernel Base)
Após avaliação direta na árvore de código `core/libs/prx`, confirmamos as seguintes integrações (já incorporadas na branch upstream do original e portadas para a documentação de nosso agente Jules):

1. **Memória Flexível / Direct Memory**: As instruções `sceKernelReserveVirtualRange` estão ligadas sob o módulo de Export em `DirectMemory` com mitigadores de reserva alocativa (fallback limits).
2. **Sync / Threading**: Atributos complexos de agendamento POSIX (`scePthreadAttrSetsolosched`, etc) já constam mapeados e suportados nos diretórios PThread/SyncOnAddress na tradução C++.

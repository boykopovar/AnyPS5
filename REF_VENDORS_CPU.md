# Referência Arquitetural de CPUs Host (REF_VENDORS_CPU.md)

Para alcançar alta taxa de quadros (FPS) e consistência (1% Lows) na tradução de jogos nativos do PlayStation 5 para o PC, o escalonamento das threads do jogo requer uma interceptação inteligente. O PS5 utiliza uma arquitetura AMD Zen 2 unificada com 8 núcleos homogêneos. No PC, a realidade topológica é muito mais fragmentada.

## 1. Topologia de Processadores Intel (Arquitetura Híbrida)

A partir da 12ª geração (Alder Lake) em diante, a Intel adotou o modelo de *Big.LITTLE* para desktops (híbrido).
*   **P-Cores (Performance Cores):** Núcleos focados em instruções pesadas, clocks altos e IPC massivo. Suportam Hyper-Threading.
*   **E-Cores (Efficiency Cores):** Núcleos focados em processos de background. Clocks mais baixos, IPC reduzido, sem Hyper-Threading.
*   **Problema na Tradução:** Um jogo de PS5 frequentemente cria threads de renderização (Render Thread) pesadas. Se o Agendador do SO (OS Scheduler) do PC empurrar a *Render Thread* traduzida para um E-Core, o desempenho da GPU (independentemente de ser NVIDIA ou AMD) despencará pela falta de alimentação de buffers de comando (CPU bottleneck).
*   **Mitigação via Relinker:** A camada de compatibilidade do `Pthread` (ex: interceptações em `scePthreadAttrSetaffinity`) deve ser mapeada para o Windows (`SetThreadAffinityMask`) ou Linux (`sched_setaffinity`) de modo a **isolar** ou **priorizar** as máscaras de P-Cores disponíveis no processador host durante chamadas críticas.

## 2. Topologia de Processadores AMD (Zen 3, Zen 4 e Zen 5)

Processadores desktop da AMD utilizam o design de *Chiplets*.
*   **CCX/CCD (Core Complex / Core Complex Die):** Núcleos são agrupados em blocos (geralmente de 8).
*   **Cache L3 Unificado e Latência:** A latência de comunicação de uma thread no CCX-0 com outra thread no CCX-1 é substancialmente mais alta do que entre threads no mesmo CCX. No PS5, todas as threads compartilham a memória unificada e o mesmo die de maneira mais uniforme do que em CPUs enthusiast (como um Ryzen 9 7950X).
*   **Problema na Tradução:** Se as threads de física e colisão de um jogo precisarem sincronizar constantemente (via mutexes baseados em futex como `sceKernelSyncOnAddress`) e estiverem operando em CCDs diferentes no PC, a latência do barramento infinito (Infinity Fabric) destruirá a sincronização de frame do jogo.
*   **Mitigação via Relinker:** Agrupar threads logicamente conectadas (que no PS5 possuíam máscaras de afinidade adjacentes) dentro do mesmo *Core Complex* no ambiente Host.

## 3. Registradores x86_64 e Instruções SIMD Específicas

*   **AVX-512:** Algumas gerações de AMD (Zen 4) e Intel (limitado) suportam instruções massivas vetoriais AVX-512, enquanto o PS5 é limitado ao AVX2/AVX de 256 bits. O Relinker não deve estender artificialmente esses registradores, mas em rotinas internas do nosso próprio wrapper de biblioteca (`core/libs/prx/`), o uso de intrinsics nativos maiores pode acelerar a decodificação de texturas (swizzling) e desembalagem de arquivos de jogos.
*   **Context Switch (Thread Saves):** Quando o jogo original do console entra em modo *yield* ou invoca a syscall `scePthreadYield`, o custo no Linux via `sched_yield` e no Windows via `SwitchToThread` possui overheads diferentes nas instruções `xsave`/`xrstor`. No Windows, loops de spin-lock apertados podem precisar de instruções `PAUSE` explicitamente compiladas na camada de adaptação.
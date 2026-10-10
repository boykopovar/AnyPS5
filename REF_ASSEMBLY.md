# Dicionário de Assinaturas de Assembly (REF_ASSEMBLY.md)

Este dicionário provê a base para o matching de trechos de código assembly (x86_64 otimizados para Zen 2) mapeados nos binários do PS5, a fim de identificar e traduzir/hookar funções nativas de jogos nas camadas de tradução.

## 1. Convenções de Chamada e ABI (System V ABI Modificado)

A plataforma PS5 usa a arquitetura x86_64 e, no geral, baseia-se na **System V AMD64 ABI**, mas o compilador Clang e a engine de SO própria possuem alguns desvios nos registradores preservados por função.

### Registradores para Argumentos de Função:
- `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` (Mesmo padrão System V, com extensões para vetores XMM).

### Exemplo de Hook (Trampoline Assembly):
Ao fazer hooking de uma syscall do PS5 na memória de um jogo em tempo de execução para o nosso ambiente:
```nasm
; Hook genérico de JMP (Trampoline 64-bit)
; Salto absoluto para o wrapper no nosso host
mov rax, 0xAAAAAAAAAAAAAAAA ; Endereço absoluto 64 bits da nossa função wrapper
jmp rax
```

## 2. Padrões Encontrados (Signatures) em Binários RDNA2 / Zen 2

### Assinatura 1: Sincronização e Barreira de Memória Zen 2
Títulos otimizados para PS5 frequentemente dependem de instruções de barreiras pesadas (`mfence`, `sfence`) e leves otimizadas por cache line.
*   **Comportamento:** Limpeza agressiva de cache após submissão de buffers de GPU no modelo UMA (Unified Memory Architecture).
*   **Atenção de Tradução:** No PC, memória dedicada de GPU e UMA são arquitetonicamente diferentes.

### Assinatura 2: Context Switch Pthread
No re-link de binários de ELF do PS5 para Linux Host, a inicialização e save/restore do contexto de CPU para threads `scePthread` geralmente empilha e desempilha registradores XMM e de propósito geral via `xsave`.

*(Novas assinaturas em binários reais devem ser incluídas aqui quando o Relinker extrair padrões de bibliotecas PRX e a camada dinâmica estiver em debugging na Fase 4).*
# Memória Local e Persistência - Agente Jules

Este arquivo é um registro contínuo e persistente de longo prazo para as operações, decisões arquiteturais, bugs identificados, mapeamentos validados e próximos passos no desenvolvimento da camada de tradução.

## Estado Atual
- **Projeto Base:** AnyPS5 (Forkado como PC_Station_5)
- **Fase Atual:** Inicialização e pesquisa avançada
- **Ambiente de Operação:** Português (Brasil) obrigatório em comunicações, logs, commits e PRs.

## Decisões Arquiteturais
- **Documentação Local:** Toda documentação deve ser replicada ou resumida no próprio repositório para evitar consultas constantes ou dependência de internet.
- **Tradução de Shaders:** Uso obrigatório das bibliotecas spirv-tools para pipeline de shaders quando disponíveis.
- **Isolamento de Memória (Memória Direta):** Foi levantado via README que emulação para Windows compromete a memória virtual total na alocação de memória direta (até 13.8 GB por título). Considerar estratégias de mitigação no futuro.

## Bugs e Problemas Encontrados
- *Nenhum bug reportado no fork inicial.*

## Mapeamentos Validados
- A definir nas próximas etapas (Fase 4).

## Próximos Passos
- Gerar arquivos `REF_SISTEMA_GPU.md`, `BIBLIOTECA_TRADUCAO.md` e `REF_ASSEMBLY.md` para suportar a base de dados do Agente Jules e desenvolvimento sem necessidade de consultas online.
- Preparar integração com os processos originais e verificar o processo de mapeamento do executável nativo.
## Atualização de Fluxo (Fase 4 - Shaders RDNA2)
- **Descoberta:** O recompilador de shaders converte os opcodes nativos baseados na arquitetura RDNA da AMD (`core/shader/recompiler/RdnaDecoder`). Opcodes escalares controlam fluxo de execução e VGPRs processam os dados locais da thread.
- **Protocolo de Commit:** Todo novo avanço substancial deve ser enviado em uma nova branch (ex: `feature/mapeamento-shaders-rdna2`) para gerar Pull Requests distintos e limpos, permitindo melhor rastreabilidade de código.

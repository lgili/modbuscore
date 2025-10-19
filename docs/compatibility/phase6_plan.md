# Fase 6 – Compatibilidade & Migração

Objetivo: declarar a versão 1.0 (DI) como API oficial e remover artefatos
legados. Nenhum wrapper de compatibilidade será mantido.

## Entregáveis

1. **Comunicar breaking changes**: página curta explicando que a API 2.x foi
   retirada e que projetos devem adotar a arquitetura DI (não há compat layer).
2. **Limpeza de legados**: remoção da pasta `bak/` e quaisquer referências no
   código/documentação.
3. **Checklist de adoção**: lista de passos para quem está começando na 1.0
   (onde encontrar exemplos, CLI, etc.).

## Plano

1. Criar página `docs/compatibility/breaking_changes_1.0.md` com resumo das
   mudanças e apontar CLI/exemplos atuais.
2. Atualizar README/quick start/routing para remover qualquer menção a compat.
3. Remover diretório `bak/` (já feito) e garantir que históricos/lints não o
   referenciam.
4. Validar se o pipeline/testes continuam verdes após a remoção.

# Breaking Changes – ModbusCore 1.0

A versão 1.0 é uma reescrita completa baseada em injeção de dependências (DI).
Não há camada de compatibilidade com a API 2.x – projetos devem adotar a nova
arquitetura diretamente. Este documento resume as principais mudanças e aponta
materiais para começar.

## Principais Mudanças

- **API orientada a DI**: você instancia `mbc_runtime_t` com `mbc_runtime_builder`
  e injeta transporte (`mbc_transport_iface_t`) explicitamente.
- **Engine explícita**: `mbc_engine_t` gerencia ciclos de request/response; use
  `mbc_engine_submit_request`/`mbc_engine_step`/`mbc_engine_take_pdu`.
- **Transportes modulares**: drivers POSIX/Win32/Winsock ficam em
  `include/modbuscore/transport/`.
- **Eventos e testes**: API de eventos (`modbuscore/protocol/events.h`) e mocks
  facilitam cobertura.
- **CLI**: `./scripts/modbus` gera projetos iniciais (`new app`, `new profile`).

## Como começar

1. Leia o [Quick Start](../quick-start.md) – apresenta o fluxo completo com
   runtime/engine e PDUs.
2. Explore os exemplos em `examples/` – clientes/servidores TCP e RTU completos.
3. Use a CLI para criar um projeto de base:
   ```bash
   ./scripts/modbus new app my_modbus_app --transport tcp
   ```
4. Adapte seu código: substitua chamadas diretas como `mb_client_read_holding`
   pelo envio de PDUs através do engine.

## O que foi removido

- Arquivos da API 2.x (`modbus/*.h`) foram removidos juntamente com a pasta `bak/`.
- Não há macros/defines de compatibilidade (ex.: `#define MB_COMPAT`).
- Exemplos antigos foram substituídos pelos novos (`examples/`).

## Perguntas Frequentes

**Posso reutilizar meu código antigo?**

Sim, mas você precisará mapear as chamadas para a nova API. Por exemplo,
`mb_client_handle_t` agora é substituído por runtime + engine. Recomendamos
criar wrappers finos dentro do seu projeto se quiser manter assinaturas internas.

**Existirá compat layer no futuro?**

Não planejamos manter compatibilidade binária com 2.x – a recomendação é migrar
para a API DI.

**Como reportar dúvidas?**

Abra uma issue ou procure a equipe. Ao reportar, inclua trechos de código e
explique qual funcionalidade da 1.0 ainda não atende seu caso.


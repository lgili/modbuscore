# Modbus CLI – Fase 5 (DX & Tooling)

Este documento descreve o desenho inicial da ferramenta de linha de comando `modbus`
planejada para a Fase 5. O objetivo é oferecer uma experiência rápida de bootstrap
e diagnóstico para projetos que consumam a ModbusCore 3.0.

## Objetivos

- Fornecer um binário/entry-point único (`modbus`) acessível via `python -m` ou
  wrapper script.
- Comandos declarativos que geram esqueleto de projetos ou inspecionam
  configuração/ambiente.
- Suporte multiplataforma (macOS, Linux, Windows) sem dependências não-standard.

## Estrutura Geral

```
tools/
  modbus_cli/
    __init__.py
    cli.py          # ponto de entrada principal
    generator.py    # helpers para scaffolding
    profiles/
      __init__.py
      posix_client.py
      rtu_loop.py
    diagnostics.py
scripts/
  modbus (wrapper opcional para facilitar execução)
```

- A CLI será implementada em Python 3.8+ utilizando `argparse` (sem dependências
  externas).
- O wrapper `scripts/modbus` chamará `python -m modbus_cli.cli`.
- Perfis (e.g. `POSIX_CLIENT`) serão descritos como dicionários contendo:
  - Nome do projeto
  - Artefatos a gerar (CMakeLists, main.c, README)
  - Dependências necessárias (por exemplo, Stage 4 transportes)

## Comandos Planejados (Fase 5)

| Comando                  | Descrição                                                                                         |
|--------------------------|---------------------------------------------------------------------------------------------------|
| `modbus new app`         | Gera um projeto C/CMake simples com engine, runtime e transporte prontos.                         |
| `modbus new profile <p>` | Gera projeto baseado em perfis pré-definidos (ex.: `POSIX_CLIENT`, `RTU_LOOPBACK`).                |
| `modbus add transport`   | (post-fase 5) Adiciona arquivos de transporte a um projeto existente.                              |
| `modbus doctor`          | Executa checagens de ambiente (presença de `cmake`, compilador, utilitários opcionais).            |

### `modbus new app`

Primeira entrega implementável:

```
modbus new app my_modbus_app \
  --transport tcp \
  --unit 17 \
  --out ./my_modbus_app
```

Gera:

- `CMakeLists.txt` consumindo `ModbusCore::modbuscore`
- `src/main.c` com esqueleto de engine cliente + comentários
- `README.md` com instruções de build/run e links para exemplos
- Pastas opcionais (`include/`, `scripts/`) dependendo das flags

### Perfis Iniciais

- `posix-tcp-client`: liga via transport POSIX TCP e executa ciclo de leitura/escrita
- `posix-rtu-loop`: usa driver RTU com mocks (útil para desenvolvimento local)

Os perfis serão descritos em `tools/modbus_cli/profiles/*.py` com metadados
e templates Jinja-like (sem dependência externa; usaremos `str.format`).

## Integração com Repositório

- O diretório `examples/` servirá como fonte dos templates – os arquivos atuais
  serão reaproveitados/remixados pelo generator.
- `examples/README.md` será atualizado para apontar para a CLI.
- CI: adicionaremos job futuro executando `modbus new app` + build para garantir
  que o scaffolding continue válido.

## Roteiro de Implementação (Fase 5)

1. **Infraestrutura CLI** (este documento) – definir layout e entry-point.
2. **Implementação do comando `modbus new app`** com duas opções de transporte (`tcp`, `rtu`). ✅
3. **Perfis iniciais (`modbus new profile`) e `modbus doctor`.** ✅
4. **Integração em testes/CI** – rodar generation sanity via `ctest` (adicionado `modbus_cli_sanity`). ✅
5. **Extensões Futuras** – `modbus add transport`, perfis extras, automações adicionais (Fase 5+).

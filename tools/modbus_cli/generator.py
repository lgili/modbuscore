"""Project generation helpers for the Modbus CLI."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from textwrap import dedent, indent
from typing import Dict, Optional


class GenerationError(RuntimeError):
    """Raised when project scaffolding fails."""


@dataclass(frozen=True)
class AppConfig:
    name: str
    out_dir: Path
    transport: str
    unit: int
    overwrite: bool
    template_overrides: Optional[Dict[str, object]] = None
    extra_readme: Optional[str] = None
    autoheal: Optional["AutoHealOptions"] = None


@dataclass(frozen=True)
class AutoHealOptions:
    max_retries: int = 3
    initial_backoff_ms: int = 100
    max_backoff_ms: int = 1000
    cooldown_ms: int = 5000


TCP_MAIN_TEMPLATE = """\
#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>
#ifdef _WIN32
#include <modbuscore/transport/winsock_tcp.h>
#else
#include <modbuscore/transport/posix_tcp.h>
#endif

{autoheal_includes}
#include <stdio.h>

#define MODBUS_HOST "{host}"
#define MODBUS_PORT {port}
#define MODBUS_UNIT {unit}

int main(void)
{{
    mbc_status_t status;
    mbc_transport_iface_t transport;
#ifdef _WIN32
    mbc_winsock_tcp_ctx_t *ctx = NULL;
    mbc_winsock_tcp_config_t config = {{
        .host = MODBUS_HOST,
        .port = MODBUS_PORT,
        .connect_timeout_ms = 2000,
        .recv_timeout_ms = 2000,
    }};
    status = mbc_winsock_tcp_create(&config, &transport, &ctx);
#else
    mbc_posix_tcp_ctx_t *ctx = NULL;
    mbc_posix_tcp_config_t config = {{
        .host = MODBUS_HOST,
        .port = MODBUS_PORT,
        .connect_timeout_ms = 2000,
        .recv_timeout_ms = 2000,
    }};
    status = mbc_posix_tcp_create(&config, &transport, &ctx);
#endif
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to create TCP transport (status=%d)\\n", status);
        return 1;
    }}

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    status = mbc_runtime_builder_build(&builder, &runtime);
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to build runtime (status=%d)\\n", status);
#ifdef _WIN32
        mbc_winsock_tcp_destroy(ctx);
#else
        mbc_posix_tcp_destroy(ctx);
#endif
        return 1;
    }}

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {{
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .response_timeout_ms = 2000,
    }};
    status = mbc_engine_init(&engine, &engine_cfg);
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to initialise engine (status=%d)\\n", status);
        mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
        mbc_winsock_tcp_destroy(ctx);
#else
        mbc_posix_tcp_destroy(ctx);
#endif
        return 1;
    }}

{autoheal_section}
    printf("TODO: submit Modbus/TCP requests here (unit %u).\\n", (unsigned)MODBUS_UNIT);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
    mbc_winsock_tcp_destroy(ctx);
#else
    mbc_posix_tcp_destroy(ctx);
#endif
    return 0;
}}
"""


RTU_MAIN_TEMPLATE = """\
#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>
#ifdef _WIN32
#include <modbuscore/transport/win32_rtu.h>
#else
#include <modbuscore/transport/posix_rtu.h>
#endif

{autoheal_includes}
#include <stdio.h>

#define MODBUS_UNIT {unit}

#ifdef _WIN32
#define MODBUS_DEVICE "{win_device}"
#else
#define MODBUS_DEVICE "{posix_device}"
#endif

#define MODBUS_BAUD {baud}

int main(void)
{{
    mbc_status_t status;
    mbc_transport_iface_t transport;

#ifdef _WIN32
    mbc_win32_rtu_ctx_t *ctx = NULL;
    mbc_win32_rtu_config_t config = {{
        .port_name = MODBUS_DEVICE,
        .baud_rate = MODBUS_BAUD,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .guard_time_us = 0,
        .rx_buffer_capacity = 256,
    }};
    status = mbc_win32_rtu_create(&config, &transport, &ctx);
#else
    mbc_posix_rtu_ctx_t *ctx = NULL;
    mbc_posix_rtu_config_t config = {{
        .device_path = MODBUS_DEVICE,
        .baud_rate = MODBUS_BAUD,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .guard_time_us = 0,
        .rx_buffer_capacity = 256,
    }};
    status = mbc_posix_rtu_create(&config, &transport, &ctx);
#endif
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to open RTU interface (status=%d)\\n", status);
        return 1;
    }}

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    status = mbc_runtime_builder_build(&builder, &runtime);
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to build runtime (status=%d)\\n", status);
#ifdef _WIN32
        mbc_win32_rtu_destroy(ctx);
#else
        mbc_posix_rtu_destroy(ctx);
#endif
        return 1;
    }}

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {{
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 1000,
    }};
    status = mbc_engine_init(&engine, &engine_cfg);
    if (!mbc_status_is_ok(status)) {{
        fprintf(stderr, "Failed to initialise engine (status=%d)\\n", status);
        mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
        mbc_win32_rtu_destroy(ctx);
#else
        mbc_posix_rtu_destroy(ctx);
#endif
        return 1;
    }}

{autoheal_section}
    printf("TODO: submit Modbus/RTU requests here (unit %u).\\n", (unsigned)MODBUS_UNIT);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
    mbc_win32_rtu_destroy(ctx);
#else
    mbc_posix_rtu_destroy(ctx);
#endif
    return 0;
}}
"""


CMAKE_TEMPLATE = """\
cmake_minimum_required(VERSION 3.20)

project({project_name}
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "ModbusCore sample application"
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ModbusCore CONFIG REQUIRED)

add_executable({project_name}
  src/main.c
)

target_link_libraries({project_name} PRIVATE ModbusCore::modbuscore)
"""


README_TEMPLATE = """\
# {project_name}

Projeto bootstrap gerado pelo `modbus new app`.

## Compilação

```bash
cmake -S . -B build
cmake --build build
```

## Execução

Edite `src/main.c` para ajustar host/porta (TCP) ou dispositivo/bps (RTU) e inclua
o fluxo de envio de requisições. O código gerado inicializa `mbc_runtime_builder`
e `mbc_engine`, deixa preparado para você enviar PDUs com `mbc_engine_submit_request`
e processar respostas com `mbc_engine_step`.
{autoheal_note}
"""


TRANSPORT_TEMPLATES: Dict[str, str] = {
    "tcp": TCP_MAIN_TEMPLATE,
    "rtu": RTU_MAIN_TEMPLATE,
}


@dataclass(frozen=True)
class ProfileDefinition:
    default_name: str
    transport: str
    template_overrides: Dict[str, object]
    extra_readme: str
    unit: int = 17
    autoheal: Optional[AutoHealOptions] = None


PROFILE_DEFINITIONS: Dict[str, ProfileDefinition] = {
    "posix-tcp-client": ProfileDefinition(
        default_name="modbus_posix_tcp_client",
        transport="tcp",
        template_overrides={
            "host": "127.0.0.1",
            "port": 15020,
        },
        extra_readme=dedent(
            """
            ## Perfil: posix-tcp-client

            Projeto pronto para conectar em `127.0.0.1:15020`. Ajuste `MODBUS_HOST`
            e `MODBUS_PORT` em `src/main.c` de acordo com o endpoint do seu servidor
            Modbus e implemente o envio/recebimento de PDUs no espaço indicado.
            """
        ).strip(),
        unit=17,
        autoheal=AutoHealOptions(),
    ),
    "rtu-loopback": ProfileDefinition(
        default_name="modbus_rtu_loopback",
        transport="rtu",
        template_overrides={
            "posix_device": "/tmp/modbus_server",
            "win_device": "COM5",
            "baud": 9600,
        },
        extra_readme=dedent(
            """
            ## Perfil: rtu-loopback

            Utiliza um par virtual de portas seriais. Em macOS/Linux crie com:

            ```bash
            sudo socat -d -d \\
                pty,raw,echo=0,link=/tmp/modbus_server,perm=0666 \\
                pty,raw,echo=0,link=/tmp/modbus_client,perm=0666
            ```

            Execute o servidor no lado `modbus_server` e o cliente no lado
            `modbus_client`. Em Windows, utilize [com0com](https://sourceforge.net/projects/com0com/).
            """
        ).strip(),
        unit=17,
        autoheal=AutoHealOptions(
            max_retries=5,
            initial_backoff_ms=200,
            max_backoff_ms=2000,
            cooldown_ms=8000,
        ),
    ),
}


def _autoheal_snippets(options: Optional[AutoHealOptions]) -> Dict[str, str]:
    if not options:
        return {
            "autoheal_includes": "",
            "autoheal_section": "",
            "autoheal_note": "",
        }

    includes = "#include <modbuscore/runtime/autoheal.h>\n\n"

    section_body = dedent(
        """
        mbc_autoheal_config_t autoheal_cfg = {{
            .runtime = &runtime,
            .max_retries = {max_retries},
            .initial_backoff_ms = {initial_backoff_ms},
            .max_backoff_ms = {max_backoff_ms},
            .cooldown_ms = {cooldown_ms},
        }};

        mbc_autoheal_supervisor_t autoheal;
        status = mbc_autoheal_init(&autoheal, &autoheal_cfg, &engine);
        if (!mbc_status_is_ok(status)) {{
            fprintf(stderr, "Failed to initialise auto-heal (status=%d)\\n", status);
        }} else {{
            /* TODO: preparar frame e chamar mbc_autoheal_submit/step/take_pdu */
            mbc_autoheal_shutdown(&autoheal);
        }}
        """
    ).format(
        max_retries=options.max_retries,
        initial_backoff_ms=options.initial_backoff_ms,
        max_backoff_ms=options.max_backoff_ms,
        cooldown_ms=options.cooldown_ms,
    )
    section = indent(section_body.strip() + "\n", "    ")

    note_text = dedent(
        f"""
        ## Auto-heal

        Este projeto foi gerado com auto-heal habilitado (max_retries={options.max_retries},
        initial_backoff_ms={options.initial_backoff_ms} ms, max_backoff_ms={options.max_backoff_ms} ms,
        cooldown_ms={options.cooldown_ms} ms). Ajuste o bloco `mbc_autoheal_*` em `src/main.c`
        para enviar seus frames usando o supervisor e iterar com `mbc_autoheal_step`/`mbc_autoheal_take_pdu`.
        """
    ).strip()
    note = "\n" + note_text + "\n"

    return {
        "autoheal_includes": includes,
        "autoheal_section": section,
        "autoheal_note": note,
    }


def create_app_project(config: AppConfig) -> Path:
    if config.transport not in TRANSPORT_TEMPLATES:
        raise GenerationError(f"Unsupported transport '{config.transport}'")

    project_dir = config.out_dir if config.out_dir is not None else Path(config.name)
    project_dir = project_dir.resolve()
    src_dir = project_dir / "src"

    if project_dir.exists() and not config.overwrite:
        existing_files = list(project_dir.iterdir())
        if existing_files:
            raise GenerationError(
                f"Destination '{project_dir}' already exists. Use --force to overwrite."
            )

    os.makedirs(src_dir, exist_ok=True)

    substitutions = {
        "project_name": config.name,
        "unit": config.unit,
    }
    if config.transport == "tcp":
        substitutions.update({"host": "127.0.0.1", "port": 15020})
    else:
        substitutions.update(
            {
                "posix_device": "/dev/ttyUSB0",
                "win_device": "COM3",
                "baud": 9600,
            }
        )
    snippets = _autoheal_snippets(config.autoheal)
    substitutions.update(snippets)
    if config.template_overrides:
        substitutions.update(config.template_overrides)

    write_file(
        project_dir / "CMakeLists.txt",
        dedent(CMAKE_TEMPLATE).format(**substitutions),
        config.overwrite,
    )
    write_file(
        src_dir / "main.c",
        dedent(TRANSPORT_TEMPLATES[config.transport]).format(**substitutions),
        config.overwrite,
    )
    write_file(
        project_dir / "README.md",
        _compose_readme(substitutions, config.extra_readme),
        config.overwrite,
    )

    return project_dir


def write_file(path: Path, content: str, overwrite: bool) -> None:
    if path.exists() and not overwrite:
        raise GenerationError(f"File '{path}' already exists. Use --force to overwrite.")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(content.rstrip() + "\n")


def _compose_readme(substitutions: Dict[str, object], extra: Optional[str]) -> str:
    base = dedent(README_TEMPLATE).format(**substitutions).rstrip()
    if extra:
        return base + "\n\n" + dedent(extra).strip() + "\n"
    return base + "\n"


def available_profiles() -> Dict[str, ProfileDefinition]:
    return PROFILE_DEFINITIONS


def create_profile_project(
    profile_name: str,
    name: Optional[str],
    out_dir: Optional[Path],
    overwrite: bool,
    autoheal: Optional[AutoHealOptions] = None,
) -> Path:
    definition = PROFILE_DEFINITIONS.get(profile_name)
    if not definition:
        raise GenerationError(
            f"Perfil desconhecido '{profile_name}'. Use --list para ver opções."
        )

    project_name = name or definition.default_name
    destination = out_dir or Path(project_name)

    autoheal_cfg = autoheal if autoheal is not None else definition.autoheal

    return create_app_project(
        AppConfig(
            name=project_name,
            out_dir=destination,
            transport=definition.transport,
            unit=definition.unit,
            overwrite=overwrite,
            template_overrides=definition.template_overrides,
            extra_readme=definition.extra_readme,
            autoheal=autoheal_cfg,
        )
    )

"""Command-line entry point for the Modbus CLI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional

from .generator import (
    AppConfig,
    AutoHealOptions,
    GenerationError,
    available_profiles,
    create_app_project,
    create_profile_project,
)

AUTO_HEAL_DEFAULTS = {
    "max_retries": 3,
    "initial_backoff_ms": 100,
    "max_backoff_ms": 1000,
    "cooldown_ms": 5000,
}


def _add_autoheal_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--auto-heal",
        action="store_true",
        help="Habilita supervisor auto-heal com retries e circuit breaker",
    )
    parser.add_argument(
        "--heal-max-retries",
        type=int,
        default=AUTO_HEAL_DEFAULTS["max_retries"],
        metavar="N",
        help="Número máximo de retries antes de abrir circuito (default: %(default)s)",
    )
    parser.add_argument(
        "--heal-initial-backoff-ms",
        type=int,
        default=AUTO_HEAL_DEFAULTS["initial_backoff_ms"],
        metavar="MS",
        help="Delay inicial entre retries em ms (default: %(default)s)",
    )
    parser.add_argument(
        "--heal-max-backoff-ms",
        type=int,
        default=AUTO_HEAL_DEFAULTS["max_backoff_ms"],
        metavar="MS",
        help="Delay máximo entre retries em ms (default: %(default)s)",
    )
    parser.add_argument(
        "--heal-cooldown-ms",
        type=int,
        default=AUTO_HEAL_DEFAULTS["cooldown_ms"],
        metavar="MS",
        help="Tempo de cooldown do circuit breaker em ms (default: %(default)s)",
    )


def _autoheal_options_from_args(args: argparse.Namespace) -> Optional[AutoHealOptions]:
    heal_args = {
        "max_retries": getattr(args, "heal_max_retries", AUTO_HEAL_DEFAULTS["max_retries"]),
        "initial_backoff_ms": getattr(
            args, "heal_initial_backoff_ms", AUTO_HEAL_DEFAULTS["initial_backoff_ms"]
        ),
        "max_backoff_ms": getattr(args, "heal_max_backoff_ms", AUTO_HEAL_DEFAULTS["max_backoff_ms"]),
        "cooldown_ms": getattr(args, "heal_cooldown_ms", AUTO_HEAL_DEFAULTS["cooldown_ms"]),
    }

    enabled = bool(getattr(args, "auto_heal", False))
    custom_requested = any(
        heal_args[key] != AUTO_HEAL_DEFAULTS[key] for key in heal_args.keys()
    )

    if not enabled:
        if custom_requested:
            raise ValueError("Use --auto-heal para aplicar opções de auto-heal personalizadas")
        return None

    for key, value in heal_args.items():
        if value < 0:
            raise ValueError(f"--{key.replace('_', '-')} deve ser >= 0")

    if heal_args["max_backoff_ms"] < heal_args["initial_backoff_ms"]:
        raise ValueError("--heal-max-backoff-ms deve ser >= --heal-initial-backoff-ms")

    return AutoHealOptions(**heal_args)
from .doctor import run_doctor


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="modbus",
        description="ModbusCore helper CLI (scaffolding & diagnostics)",
    )
    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND")

    new_parser = subparsers.add_parser("new", help="Generate project skeletons")
    new_sub = new_parser.add_subparsers(dest="new_command", metavar="SUBCOMMAND")

    app_parser = new_sub.add_parser(
        "app",
        help="Create a standalone ModbusCore application project",
    )
    app_parser.add_argument(
        "name",
        help="Nome do projeto (usado para o target principal)",
    )
    app_parser.add_argument(
        "--out",
        metavar="PATH",
        type=Path,
        help="Diretório de saída (default: ./<name>)",
    )
    app_parser.add_argument(
        "--transport",
        choices=("tcp", "rtu"),
        default="tcp",
        help="Transporte inicial do exemplo (default: tcp)",
    )
    app_parser.add_argument(
        "--unit",
        type=int,
        default=17,
        help="Unit/slave id padrão utilizado no exemplo gerado (default: 17)",
    )
    app_parser.add_argument(
        "--force",
        action="store_true",
        help="Sobrescreve arquivos existentes",
    )
    _add_autoheal_args(app_parser)

    profile_parser = new_sub.add_parser(
        "profile",
        help="Gerar projeto baseado em perfil pré-definido",
    )
    profile_parser.add_argument(
        "profile",
        nargs="?",
        help="Nome do perfil (use --list para ver opções)",
    )
    profile_parser.add_argument(
        "--name",
        metavar="NAME",
        help="Nome do projeto (default: nome padrão do perfil)",
    )
    profile_parser.add_argument(
        "--out",
        metavar="PATH",
        type=Path,
        help="Diretório de saída (default: ./<name>)",
    )
    profile_parser.add_argument(
        "--force",
        action="store_true",
        help="Sobrescreve arquivos existentes",
    )
    profile_parser.add_argument(
        "--list",
        action="store_true",
        help="Lista perfis disponíveis",
    )
    _add_autoheal_args(profile_parser)

    doctor_parser = subparsers.add_parser(
        "doctor",
        help="Executa checagens de ambiente e ferramentas",
    )
    doctor_parser.add_argument(
        "--json",
        action="store_true",
        help="Emite resultado em JSON (stderr permanece humano)",
    )

    return parser


def handle_new_app(args: argparse.Namespace) -> int:
    try:
        autoheal_opts = _autoheal_options_from_args(args)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        project_dir = create_app_project(
            AppConfig(
                name=args.name,
                out_dir=args.out if args.out else Path(args.name),
                transport=args.transport,
                unit=args.unit,
                overwrite=args.force,
                autoheal=autoheal_opts,
            )
        )
    except GenerationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Projeto criado em: {project_dir}")
    print("Próximos passos:")
    print(f"  cd {project_dir}")
    print("  cmake -S . -B build")
    print("  cmake --build build")
    if autoheal_opts:
        print(
            "  Auto-heal: max_retries={max_retries}, initial_backoff_ms={initial_backoff_ms}, "
            "max_backoff_ms={max_backoff_ms}, cooldown_ms={cooldown_ms}".format(
                max_retries=autoheal_opts.max_retries,
                initial_backoff_ms=autoheal_opts.initial_backoff_ms,
                max_backoff_ms=autoheal_opts.max_backoff_ms,
                cooldown_ms=autoheal_opts.cooldown_ms,
            )
        )
    return 0


def handle_new_profile(args: argparse.Namespace) -> int:
    profiles = available_profiles()

    if args.list:
        print("Perfis disponíveis:\n")
        for name, definition in profiles.items():
            autoheal_status = "on" if definition.autoheal else "off"
            print(
                f"  {name:<18} transporte={definition.transport} auto-heal={autoheal_status}"
                f" (default: {definition.default_name})"
            )
        return 0

    if not args.profile:
        print("error: informe o nome do perfil ou use --list", file=sys.stderr)
        return 1

    try:
        autoheal_override = _autoheal_options_from_args(args)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        project_dir = create_profile_project(
            profile_name=args.profile,
            name=args.name,
            out_dir=args.out,
            overwrite=args.force,
            autoheal=autoheal_override,
        )
    except GenerationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Projeto criado em: {project_dir}")
    print("Próximos passos:")
    print(f"  cd {project_dir}")
    print("  cmake -S . -B build")
    print("  cmake --build build")
    definition = profiles.get(args.profile)
    effective_autoheal = autoheal_override or (definition.autoheal if definition else None)
    if effective_autoheal:
        print(
            "  Auto-heal: max_retries={max_retries}, initial_backoff_ms={initial_backoff_ms}, "
            "max_backoff_ms={max_backoff_ms}, cooldown_ms={cooldown_ms}".format(
                max_retries=effective_autoheal.max_retries,
                initial_backoff_ms=effective_autoheal.initial_backoff_ms,
                max_backoff_ms=effective_autoheal.max_backoff_ms,
                cooldown_ms=effective_autoheal.cooldown_ms,
            )
        )
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.command == "new":
        if args.new_command == "app":
            return handle_new_app(args)
        if args.new_command == "profile":
            return handle_new_profile(args)
        parser.error("subcomando obrigatório: app")
        return 1

    if args.command == "doctor":
        return run_doctor(json_output=args.json)

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())

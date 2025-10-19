"""Command-line entry point for the Modbus CLI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .generator import (
    AppConfig,
    GenerationError,
    available_profiles,
    create_app_project,
    create_profile_project,
)
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
        project_dir = create_app_project(
            AppConfig(
                name=args.name,
                out_dir=args.out if args.out else Path(args.name),
                transport=args.transport,
                unit=args.unit,
                overwrite=args.force,
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
    return 0


def handle_new_profile(args: argparse.Namespace) -> int:
    if args.list:
        print("Perfis disponíveis:\n")
        for name, definition in available_profiles().items():
            print(f"  {name:<18} transporte={definition.transport} (default: {definition.default_name})")
        return 0

    if not args.profile:
        print("error: informe o nome do perfil ou use --list", file=sys.stderr)
        return 1

    try:
        project_dir = create_profile_project(
            profile_name=args.profile,
            name=args.name,
            out_dir=args.out,
            overwrite=args.force,
        )
    except GenerationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Projeto criado em: {project_dir}")
    print("Próximos passos:")
    print(f"  cd {project_dir}")
    print("  cmake -S . -B build")
    print("  cmake --build build")
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

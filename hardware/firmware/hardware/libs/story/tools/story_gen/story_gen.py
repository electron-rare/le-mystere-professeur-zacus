#!/usr/bin/env python3
"""Compatibility wrapper to Zacus Story generator library (Yamale + Jinja2)."""

from __future__ import annotations

import argparse
import hashlib
import sys
import tarfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

FW_ROOT = Path(__file__).resolve().parents[5]
LIB_SRC = FW_ROOT / "lib" / "zacus_story_gen_ai" / "src"
if str(LIB_SRC) not in sys.path:
    sys.path.insert(0, str(LIB_SRC))

from zacus_story_gen_ai.cli import main as cli_main  # noqa: E402
from zacus_story_gen_ai.generator import (  # noqa: E402
    StoryGenerationError,
    _normalize_story_specs,
    generate_bundle_files,
    generate_cpp_files,
)


@dataclass
class ValidationIssue:
    file: str
    field: str
    reason: str
    code: str = "VALIDATION"

    def format(self) -> str:
        return f"{self.file}: line=n/a code={self.code} field={self.field}: {self.reason}"


@dataclass
class ValidationNotice:
    file: str
    field: str
    reason: str
    code: str = "NOTICE"

    def format(self) -> str:
        return f"{self.file}: line=n/a code={self.code} field={self.field}: {self.reason}"


def compute_sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def collect_scenarios(spec_dir: Path, strict: bool) -> tuple[list[dict[str, Any]], list[ValidationIssue], list[ValidationNotice]]:
    del strict
    files = sorted([path for path in spec_dir.glob("*.y*ml") if path.is_file()])
    if not files:
        return [], [ValidationIssue(spec_dir.as_posix(), "spec_dir", "no YAML files found", "SPEC_DIR")], []
    try:
        return _normalize_story_specs(files), [], []
    except StoryGenerationError as exc:
        return [], [ValidationIssue(spec_dir.as_posix(), "root", str(exc), "NORMALIZE")], []


def build_story_fs(root: Path, scenarios: list[dict[str, Any]]) -> None:
    spec_hash = compute_sha256_hex(
        str(sorted([scenario["id"] for scenario in scenarios])).encode("utf-8")
    )[:12]
    generate_bundle_files(scenarios, root, spec_hash)


def cmd_validate(args: argparse.Namespace) -> int:
    scenarios, issues, notices = collect_scenarios(Path(args.spec_dir), bool(args.strict))
    if issues:
        for issue in issues:
            print(f"[story-validate] ERR {issue.format()}")
        return 1
    for notice in notices:
        print(f"[story-validate] WARN {notice.format()}")
    for scenario in scenarios:
        print(
            f"[story-validate] OK file={scenario['source']} id={scenario['id']} "
            f"steps={len(scenario['steps'])} bindings={len(scenario['app_bindings'])}"
        )
    print(f"[story-validate] OK total={len(scenarios)} strict={1 if args.strict else 0}")
    return 0


def cmd_generate(args: argparse.Namespace) -> int:
    scenarios, issues, notices = collect_scenarios(Path(args.spec_dir), bool(args.strict))
    if issues:
        for issue in issues:
            print(f"[story-gen] ERR {issue.format()}")
        return 1
    for notice in notices:
        print(f"[story-gen] WARN {notice.format()}")
    out_dir = Path(args.out_dir)
    spec_hash = generate_cpp_files(scenarios, out_dir)
    print(
        f"[story-gen] OK scenarios={len(scenarios)} out={out_dir.as_posix()} "
        f"strict={1 if args.strict else 0} spec_hash={spec_hash}"
    )
    return 0


def create_tarball(root: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(out_path, "w:gz") as tar:
        for path in sorted(root.rglob("*")):
            if path.is_file():
                tar.add(path, arcname=path.relative_to(root))


def cmd_deploy(args: argparse.Namespace) -> int:
    scenarios, issues, notices = collect_scenarios(Path(args.spec_dir), bool(args.strict))
    if issues:
        for issue in issues:
            print(f"[story-deploy] ERR {issue.format()}")
        return 1
    for notice in notices:
        print(f"[story-deploy] WARN {notice.format()}")

    if args.scenario_id:
        scenarios = [scenario for scenario in scenarios if scenario["id"] == args.scenario_id]
        if not scenarios:
            print(f"[story-deploy] ERR scenario '{args.scenario_id}' introuvable")
            return 1

    out_dir = Path(args.out_dir)
    root = out_dir / "deploy"
    if root.exists():
        for file_path in sorted(root.rglob("*"), reverse=True):
            if file_path.is_file():
                file_path.unlink()
    spec_hash = compute_sha256_hex(str([scenario["id"] for scenario in scenarios]).encode("utf-8"))[:12]
    generate_bundle_files(scenarios, root, spec_hash)

    archive_path = Path(args.archive)
    create_tarball(root, archive_path)
    print(
        f"[story-deploy] OK scenarios={len(scenarios)} root={root.as_posix()} archive={archive_path.as_posix()}"
    )
    if args.port:
        print("[story-deploy] WARN serial deploy disabled in wrapper; use runtime API")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="StorySpec YAML validator/generator")
    sub = parser.add_subparsers(dest="command", required=True)

    validate_p = sub.add_parser("validate", help="validate YAML scenarios")
    validate_p.add_argument("--spec-dir", default=str(FW_ROOT / "docs/protocols/story_specs/scenarios"))
    validate_p.add_argument("--strict", action="store_true", help="reject unknown fields")
    validate_p.set_defaults(func=cmd_validate)

    generate_p = sub.add_parser("generate", help="generate C++ from YAML scenarios")
    generate_p.add_argument("--spec-dir", default=str(FW_ROOT / "docs/protocols/story_specs/scenarios"))
    generate_p.add_argument("--out-dir", default=str(FW_ROOT / "hardware/libs/story/src/generated"))
    generate_p.add_argument("--strict", action="store_true", help="validate in strict mode before generate")
    generate_p.set_defaults(func=cmd_generate)

    deploy_p = sub.add_parser("deploy", help="generate JSON + checksums + tar archive")
    deploy_p.add_argument("--spec-dir", default=str(FW_ROOT / "docs/protocols/story_specs/scenarios"))
    deploy_p.add_argument("--out-dir", default=str(FW_ROOT / "artifacts/story_fs"))
    deploy_p.add_argument("--archive", default=str(FW_ROOT / "artifacts/story_fs/story_deploy.tar.gz"))
    deploy_p.add_argument("--scenario-id", default="", help="deploy single scenario id only")
    deploy_p.add_argument("--strict", action="store_true", help="validate in strict mode before deploy")
    deploy_p.add_argument("--port", default="", help="serial port for STORY_DEPLOY")
    deploy_p.add_argument("--baud", type=int, default=115200, help="serial baud (default 115200)")
    deploy_p.set_defaults(func=cmd_deploy)

    # New CLI entrypoints
    sub.add_parser("generate-cpp", help="alias to library CLI")
    sub.add_parser("generate-bundle", help="alias to library CLI")
    sub.add_parser("all", help="alias to library CLI")

    return parser


def main(argv: list[str]) -> int:
    if argv and argv[0] in {"generate-cpp", "generate-bundle", "all"}:
        # Delegate full command handling to new package CLI.
        return cli_main(argv)

    parser = build_arg_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

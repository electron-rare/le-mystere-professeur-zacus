from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .generator import (
    StoryGenerationError,
    default_paths,
    run_generate_bundle,
    run_generate_cpp,
    run_sync_screens,
    run_validate,
)


def _path_or_none(value: str | None) -> Path | None:
    if value is None or value == "":
        return None
    return Path(value)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Zacus story generator (Yamale + Jinja2)")
    sub = parser.add_subparsers(dest="command", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--spec-dir", default="", help="Story spec directory")
    common.add_argument("--game-dir", default="", help="Game scenario directory")

    validate_p = sub.add_parser("validate", parents=[common], help="Validate game + story specs")

    cpp_p = sub.add_parser("generate-cpp", parents=[common], help="Generate scenarios_gen/apps_gen C++")
    cpp_p.add_argument("--out-dir", default="", help="Output directory for generated C++")

    generate_alias = sub.add_parser("generate", parents=[common], help="Alias of generate-cpp")
    generate_alias.add_argument("--out-dir", default="", help="Output directory for generated C++")

    bundle_p = sub.add_parser("generate-bundle", parents=[common], help="Generate story bundle JSON+sha")
    bundle_p.add_argument("--out-dir", default="", help="Bundle root directory")
    bundle_p.add_argument("--archive", default="", help="Optional .tar.gz archive output")

    sync_screens_p = sub.add_parser(
        "sync-screens",
        parents=[common],
        help="Synchronize data/story/screens and legacy_payloads/fs_excluded/screens from palette",
    )
    sync_screens_p.add_argument(
        "--check",
        action="store_true",
        help="Check drift only (no writes); non-zero exit on mismatch",
    )

    deploy_alias = sub.add_parser("deploy", parents=[common], help="Alias of generate-bundle")
    deploy_alias.add_argument("--out-dir", default="", help="Bundle root directory")
    deploy_alias.add_argument("--archive", default="", help="Optional .tar.gz archive output")

    all_p = sub.add_parser("all", parents=[common], help="Validate + generate-cpp + generate-bundle")
    all_p.add_argument("--cpp-out-dir", default="", help="Output directory for generated C++")
    all_p.add_argument("--bundle-out-dir", default="", help="Bundle root directory")
    all_p.add_argument("--archive", default="", help="Optional .tar.gz archive output")

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    paths = default_paths()
    spec_dir = _path_or_none(args.spec_dir)
    game_dir = _path_or_none(args.game_dir)

    try:
        if args.command == "validate":
            result = run_validate(paths, spec_dir=spec_dir, game_dir=game_dir)
            print(
                f"[story-gen] OK validate scenarios={result['scenario_count']} "
                f"game_scenarios={result['game_scenario_count']}"
            )
            return 0

        if args.command in {"generate-cpp", "generate"}:
            result = run_generate_cpp(
                paths,
                out_dir=_path_or_none(args.out_dir),
                spec_dir=spec_dir,
                game_dir=game_dir,
            )
            print(
                f"[story-gen] OK generate-cpp out={result['out_dir']} "
                f"scenarios={result['scenario_count']} spec_hash={result['spec_hash']}"
            )
            return 0

        if args.command in {"generate-bundle", "deploy"}:
            archive = _path_or_none(args.archive)
            result = run_generate_bundle(
                paths,
                out_dir=_path_or_none(args.out_dir),
                archive=archive,
                spec_dir=spec_dir,
                game_dir=game_dir,
            )
            print(
                f"[story-gen] OK generate-bundle out={result['out_dir']} "
                f"scenarios={result['scenario_count']} spec_hash={result['spec_hash']}"
            )
            if result["archive"] is not None:
                print(f"[story-gen] archive={result['archive']}")
            return 0

        if args.command == "sync-screens":
            result = run_sync_screens(paths, check_only=bool(args.check))
            mode = "check" if args.check else "write"
            print(
                f"[story-gen] OK sync-screens mode={mode} story={result['story_count']} "
                f"story_written={result['story_written']} legacy_written={result['legacy_written']} "
                f"palette={result['palette_path']} legacy_dir={result['legacy_dir']}"
            )
            return 0

        if args.command == "all":
            validate = run_validate(paths, spec_dir=spec_dir, game_dir=game_dir)
            cpp = run_generate_cpp(
                paths,
                out_dir=_path_or_none(args.cpp_out_dir),
                spec_dir=spec_dir,
                game_dir=game_dir,
            )
            bundle = run_generate_bundle(
                paths,
                out_dir=_path_or_none(args.bundle_out_dir),
                archive=_path_or_none(args.archive),
                spec_dir=spec_dir,
                game_dir=game_dir,
            )
            print(
                "[story-gen] OK all "
                f"validate={validate['scenario_count']} cpp={cpp['scenario_count']} "
                f"bundle={bundle['scenario_count']} spec_hash={cpp['spec_hash']}"
            )
            if bundle["archive"] is not None:
                print(f"[story-gen] archive={bundle['archive']}")
            return 0

        parser.error(f"Unknown command: {args.command}")
        return 2
    except StoryGenerationError as exc:
        print(f"[story-gen] ERR {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

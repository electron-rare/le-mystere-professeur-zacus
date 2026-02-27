#!/usr/bin/env python3
"""Fail when duplicate-copy filenames are present in payload trees."""

from __future__ import annotations

import argparse
from pathlib import Path

DEFAULT_SCOPES = ("data",)
DEFAULT_PATTERNS = ("* 2.*", "* 3.*")
DEFAULT_FORBIDDEN_GLOBS = ("data/screens/*.json", "data/SCENE_*.json")


def find_duplicate_files(repo_root: Path, scopes: list[str], patterns: list[str]) -> list[Path]:
    matches: list[Path] = []
    for scope in scopes:
        scope_path = (repo_root / scope).resolve()
        if not scope_path.exists():
            continue
        for pattern in patterns:
            for candidate in scope_path.rglob(pattern):
                if candidate.is_file():
                    matches.append(candidate)
    return sorted(set(matches))


def find_forbidden_files(repo_root: Path, globs: list[str]) -> list[Path]:
    matches: list[Path] = []
    for pattern in globs:
        for candidate in repo_root.glob(pattern):
            if candidate.is_file():
                matches.append(candidate)
    return sorted(set(matches))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument(
        "--scope",
        action="append",
        default=None,
        help="Relative path scope to scan (repeatable). Default: data",
    )
    parser.add_argument(
        "--pattern",
        action="append",
        default=None,
        help="Filename pattern to reject (repeatable). Defaults: '* 2.*', '* 3.*'",
    )
    parser.add_argument(
        "--forbid-glob",
        action="append",
        default=None,
        help="Repository-root glob to reject (repeatable). Defaults: data/screens/*.json, data/SCENE_*.json",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    scopes = args.scope if args.scope else list(DEFAULT_SCOPES)
    patterns = args.pattern if args.pattern else list(DEFAULT_PATTERNS)
    forbid_globs = args.forbid_glob if args.forbid_glob else list(DEFAULT_FORBIDDEN_GLOBS)

    duplicates = find_duplicate_files(repo_root, scopes, patterns)
    forbidden = find_forbidden_files(repo_root, forbid_globs)
    if not duplicates and not forbidden:
        print(f"[ok] payload guard passed (scopes={','.join(scopes)})")
        return 0

    if duplicates:
        print("[fail] duplicate payload copies detected:")
        for path in duplicates:
            print(path.relative_to(repo_root).as_posix())
    if forbidden:
        print("[fail] legacy payload paths detected inside data/:")
        for path in forbidden:
            print(path.relative_to(repo_root).as_posix())
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

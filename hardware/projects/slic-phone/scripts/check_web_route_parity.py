#!/usr/bin/env python3
"""Check parity between frontend calls and backend routes/commands."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


Route = tuple[str, str]


@dataclass(frozen=True)
class RouteCommand:
    route: Route
    command: Optional[str]
    dynamic: bool


BACKEND_ROUTE_RE = re.compile(
    r'server_\.on\(\s*"(?P<path>/api/[^\"]+)"\s*,\s*HTTP_(?P<method>[A-Z]+)'
)
BACKEND_DISPATCH_RE = re.compile(
    r'handleDispatch\(\s*request\s*,\s*"(?P<command>[A-Z0-9_]+)'
)
BACKEND_DISPATCH_DYNAMIC_RE = re.compile(
    r'handleDispatch\(\s*request\s*,\s*[A-Za-z_][A-Za-z0-9_]*'
)
COMMAND_REG_RE = re.compile(r'registerCommand\(\s*"(?P<command>[A-Z0-9_]+)"')
FRONTEND_CALL_START_RE = re.compile(r"\brequestJson\(")
FRONTEND_PATH_ARG_RE = re.compile(r"^\s*([\"'])(?P<path>/api/[^\"']+)\1")
METHOD_RE = re.compile(r"method\s*:\s*[\"'](?P<method>[A-Z]+)[\"']")


def find_matching_delim(source: str, open_index: int, open_delim: str, close_delim: str) -> int:
    if open_index < 0 or source[open_index] != open_delim:
        return -1

    depth = 1
    in_string: str | None = None
    in_single_line_comment = False
    in_multi_line_comment = False
    escaped = False

    for index in range(open_index + 1, len(source)):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if in_single_line_comment:
            if char == "\n":
                in_single_line_comment = False
            continue

        if in_multi_line_comment:
            if char == "*" and next_char == "/":
                in_multi_line_comment = False
            continue

        if in_string is not None:
            if escaped:
                escaped = False
                continue
            if char == "\\":
                escaped = True
                continue
            if char == in_string:
                in_string = None
            continue

        if char in ('"', "'"):
            in_string = char
            continue

        if char == "/" and next_char == "/":
            in_single_line_comment = True
            continue

        if char == "/" and next_char == "*":
            in_multi_line_comment = True
            continue

        if char == open_delim:
            depth += 1
            continue

        if char == close_delim:
            depth -= 1
            if depth == 0:
                return index

    return -1


def parse_backend_route_commands(source: str) -> dict[Route, RouteCommand]:
    routes: dict[Route, RouteCommand] = {}
    for match in BACKEND_ROUTE_RE.finditer(source):
        route = (match.group("method"), match.group("path"))
        if route in routes:
            continue

        callback_open = source.find("{", match.end())
        if callback_open < 0:
            routes[route] = RouteCommand(route=route, command=None, dynamic=False)
            continue

        callback_close = find_matching_delim(source, callback_open, "{", "}")
        if callback_close < 0:
            routes[route] = RouteCommand(route=route, command=None, dynamic=False)
            continue

        callback_block = source[callback_open:callback_close]
        cmd_match = BACKEND_DISPATCH_RE.search(callback_block)
        if cmd_match:
            routes[route] = RouteCommand(
                route=route,
                command=cmd_match.group("command"),
                dynamic=False,
            )
            continue

        dynamic_match = BACKEND_DISPATCH_DYNAMIC_RE.search(callback_block)
        routes[route] = RouteCommand(route=route, command=None, dynamic=bool(dynamic_match))

    return routes


def parse_backend_routes(source: str) -> set[Route]:
    return set(parse_backend_route_commands(source).keys())


def parse_frontend_routes(source: str) -> set[Route]:
    routes: set[Route] = set()
    for match in FRONTEND_CALL_START_RE.finditer(source):
        open_paren = match.end() - 1
        open_paren = source.find("(", match.start())
        if open_paren < 0:
            continue

        close_paren = find_matching_delim(source, open_paren, "(", ")")
        if close_paren < 0:
            continue

        args = source[open_paren + 1 : close_paren]
        path_match = FRONTEND_PATH_ARG_RE.match(args)
        if not path_match:
            continue

        path = path_match.group("path")
        method_match = METHOD_RE.search(args)
        method = method_match.group("method").upper() if method_match else "GET"
        routes.add((method, path))
    return routes


def parse_registered_commands(source: str) -> set[str]:
    return {match.group("command") for match in COMMAND_REG_RE.finditer(source)}


def format_routes(routes: Iterable[Route]) -> str:
    ordered = sorted(routes, key=lambda route: (route[1], route[0]))
    return "\n".join(f"  - {method} {path}" for method, path in ordered)


def routes_to_payload(routes: Iterable[Route]) -> list[dict[str, str]]:
    ordered = sorted(routes, key=lambda route: (route[1], route[0]))
    return [{"method": method, "path": path} for method, path in ordered]


def missing_commands_to_payload(missing: Iterable[RouteCommand]) -> list[dict[str, object]]:
    ordered = sorted(missing, key=lambda rc: (rc.route[1], rc.route[0]))
    payload = []
    for item in ordered:
        payload.append({
            "method": item.route[0],
            "path": item.route[1],
            "command": item.command or "",
            "dynamic": item.dynamic,
        })
    return payload


def build_report(
    backend_routes: set[Route],
    frontend_routes: set[Route],
    missing_in_backend: set[Route],
    unused_backend: set[Route],
    missing_commands: set[RouteCommand],
    strict_unused_backend: bool,
    status: str,
) -> dict[str, object]:
    dynamic_routes = sorted(
        [rc for rc in missing_commands if rc.dynamic],
        key=lambda rc: (rc.route[1], rc.route[0]),
    )
    missing_static_commands = [rc for rc in missing_commands if not rc.dynamic]

    return {
        "backend_count": len(backend_routes),
        "frontend_count": len(frontend_routes),
        "backend_routes": routes_to_payload(backend_routes),
        "frontend_routes": routes_to_payload(frontend_routes),
        "missing_in_backend": routes_to_payload(missing_in_backend),
        "unused_backend": routes_to_payload(unused_backend),
        "missing_mapped_commands": missing_commands_to_payload(set(missing_static_commands)),
        "dynamic_routes": missing_commands_to_payload(set(dynamic_routes)),
        "strict_unused_backend": strict_unused_backend,
        "status": status,
    }


def collect_missing_commands(
    backend_command_map: dict[Route, RouteCommand],
    registered_commands: set[str],
) -> set[RouteCommand]:
    missing: set[RouteCommand] = set()
    for item in backend_command_map.values():
        if item.command is None:
            continue
        if item.command in registered_commands:
            continue
        missing.add(item)
    return missing


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate that every WebUI API call is backed by a firmware HTTP route and command binding."
        )
    )
    parser.add_argument(
        "--backend",
        default="src/web/WebServerManager.cpp",
        help="Path to backend route source file.",
    )
    parser.add_argument(
        "--frontend",
        default="data/webui/script.js",
        help="Path to frontend source file.",
    )
    parser.add_argument(
        "--commands",
        default="src/main.cpp",
        help="Source file containing registerCommand() calls.",
    )
    parser.add_argument(
        "--strict-unused-backend",
        action="store_true",
        help="Fail if backend API routes are not used by the WebUI.",
    )
    parser.add_argument(
        "--report-json",
        default="",
        help="Optional path to write a JSON parity report.",
    )
    args = parser.parse_args()

    backend_path = Path(args.backend)
    frontend_path = Path(args.frontend)
    command_path = Path(args.commands)
    backend_source = load_text(backend_path)
    frontend_source = load_text(frontend_path)
    command_source = load_text(command_path)

    backend_route_commands = parse_backend_route_commands(backend_source)
    backend_routes = set(backend_route_commands.keys())
    frontend_routes = parse_frontend_routes(frontend_source)
    registered_commands = parse_registered_commands(command_source)
    missing_in_backend = frontend_routes - backend_routes
    unused_backend = backend_routes - frontend_routes
    missing_commands = collect_missing_commands(backend_route_commands, registered_commands)

    if not backend_routes:
        print("[route-parity] no backend /api routes detected", file=sys.stderr)
        if args.report_json:
            report = build_report(
                backend_routes,
                frontend_routes,
                missing_in_backend=missing_in_backend,
                unused_backend=unused_backend,
                missing_commands=missing_commands,
                strict_unused_backend=args.strict_unused_backend,
                status="fail",
            )
            write_report_json(Path(args.report_json), report)
        return 2
    if not frontend_routes:
        print("[route-parity] no frontend /api requestJson() calls detected", file=sys.stderr)
        if args.report_json:
            report = build_report(
                backend_routes,
                frontend_routes,
                missing_in_backend=missing_in_backend,
                unused_backend=unused_backend,
                missing_commands=missing_commands,
                strict_unused_backend=args.strict_unused_backend,
                status="fail",
            )
            write_report_json(Path(args.report_json), report)
        return 2

    if missing_in_backend:
        print("[route-parity] missing backend routes for frontend calls:", file=sys.stderr)
        print(format_routes(missing_in_backend), file=sys.stderr)

    if missing_commands:
        print("[route-parity] backend routes mapped to unregistered commands:", file=sys.stderr)
        for item in sorted(missing_commands, key=lambda rc: (rc.route[1], rc.route[0])):
            print(f"  - {item.route[0]} {item.route[1]} -> {item.command}", file=sys.stderr)

    if args.strict_unused_backend and unused_backend:
        print("[route-parity] backend routes currently unused by WebUI:", file=sys.stderr)
        print(format_routes(unused_backend), file=sys.stderr)

    print(
        f"[route-parity] backend routes: {len(backend_routes)} | frontend routes: {len(frontend_routes)}"
    )

    if missing_in_backend or (args.strict_unused_backend and unused_backend) or missing_commands:
        status = "fail"
        if args.report_json:
            write_report_json(
                Path(args.report_json),
                build_report(
                    backend_routes,
                    frontend_routes,
                    missing_in_backend=missing_in_backend,
                    unused_backend=unused_backend,
                    missing_commands=missing_commands,
                    strict_unused_backend=args.strict_unused_backend,
                    status=status,
                ),
            )
        return 1

    if args.report_json:
        write_report_json(
            Path(args.report_json),
            build_report(
                backend_routes,
                frontend_routes,
                missing_in_backend=missing_in_backend,
                unused_backend=unused_backend,
                missing_commands=missing_commands,
                strict_unused_backend=args.strict_unused_backend,
                status="pass",
            ),
        )

    print("[route-parity] parity check passed")
    return 0


def load_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"[route-parity] missing file: {path}", file=sys.stderr)
        raise


def write_report_json(path: Path, report: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())

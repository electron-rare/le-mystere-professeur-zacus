#!/usr/bin/env python3
"""Extra contract tests for firmware runtime/API guards."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_source(*parts: str) -> str:
    return (ROOT.joinpath(*parts)).read_text(encoding="utf-8")


def extract_route_block(source: str, route: str) -> str:
    marker = f'server_.on("{route}"'
    start = source.find(marker)
    assert start >= 0, f"route {route} not found"

    brace_start = source.find("{", start)
    if brace_start < 0:
        return source[start : start + 4096]

    depth = 0
    in_line_comment = False
    in_block_comment = False
    in_string = None
    in_char = False
    escaped = False

    end = brace_start
    for index in range(brace_start, len(source)):
        ch = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
            continue

        if in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
            continue

        if in_string is not None:
            if escaped:
                escaped = False
                continue
            if ch == "\\":
                escaped = True
                continue
            if ch == '"':
                in_string = None
            continue

        if in_char:
            if escaped:
                escaped = False
                continue
            if ch == "\\":
                escaped = True
                continue
            if ch == "'":
                in_char = False
            continue

        if ch == '/' and nxt == '/':
            in_line_comment = True
            continue
        if ch == '/' and nxt == '*':
            in_block_comment = True
            continue
        if ch == '"':
            in_string = '"'
            continue
        if ch == "'":
            in_char = True
            continue
        if ch == '{':
            depth += 1
            continue
        if ch == '}':
            depth -= 1
            if depth == 0:
                end = index
                break

    return source[start : end + 1]


def read_main() -> str:
    return read_source("src", "main.cpp")


def read_webserver() -> str:
    return read_source("src", "web", "WebServerManager.cpp")


def read_dispatcher() -> str:
    return read_source("src", "config", "A252ConfigStore.cpp")


class RuntimeContractTests(unittest.TestCase):
    def test_unknown_dispatched_command_is_rejected(self) -> None:
        src = read_webserver()
        self.assertIn("!isCommandRegistered(command_line, command_validator_)", src)
        self.assertIn("unsupported_command", src)

    def test_espnow_send_requires_explicit_mac(self) -> None:
        src = read_webserver()
        block = extract_route_block(src, "/api/network/espnow/send")
        self.assertIn('const String mac = doc["mac"] | "";', block)
        self.assertIn("isValidInput(mac, 32)", block)
        self.assertIn("ESPNOW_SEND", block)

    def test_wifi_loop_is_invoked(self) -> None:
        src = read_main()
        self.assertRegex(src, r"\bg_wifi\.loop\(\);")

    def test_auth_is_disabled_for_dispatch_paths_by_default(self) -> None:
        main = read_main()
        self.assertIn("kWebAuthEnabledByDefault", main)
        web = read_webserver()
        self.assertIn("kWebAuthEnabledByDefault", main)
        self.assertIn("kWebAuthEnabledByDefault = false", main)
        self.assertIn("authenticateRequest(request)", web)

    def test_dev_auth_override_is_local_flagged(self) -> None:
        main = read_main()
        self.assertIn("RTC_WEB_AUTH_DEV_DISABLE", main)
        self.assertIn("!kWebAuthLocalDisableEnabled", main)

    def test_gpio_validation_blocks_invalid_values_in_source(self) -> None:
        src = read_dispatcher()
        self.assertIn("const int required_pins[]", src)
        self.assertIn("pin < 0", src)
        self.assertIn("cfg.slic_adc_in", src)


if __name__ == "__main__":
    unittest.main()

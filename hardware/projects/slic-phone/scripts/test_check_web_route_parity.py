#!/usr/bin/env python3
"""Unit tests for web route / command parity checker."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scripts.check_web_route_parity import (
    RouteCommand,
    build_report,
    collect_missing_commands,
    parse_backend_route_commands,
    parse_frontend_routes,
    parse_registered_commands,
    write_report_json,
)


class RouteParsingTest(unittest.TestCase):
    def test_detects_frontend_request_calls(self) -> None:
        source = """
        const [wifi] = await Promise.all([
          requestJson("/api/network/wifi"),
        ]);
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("GET", "/api/network/wifi"), routes)

    def test_detects_frontend_payload_requests(self) -> None:
        source = """
        await requestJson("/api/network/espnow/send", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ payload: { cmd: "STATUS" } }),
        });
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("POST", "/api/network/espnow/send"), routes)

    def test_detects_single_quotes(self) -> None:
        source = """
        await requestJson('/api/network/wifi/scan', { method: 'POST' });
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("POST", "/api/network/wifi/scan"), routes)


class BackendParsingTest(unittest.TestCase):
    def test_backend_route_mapping_extracts_command_id(self) -> None:
        source = """
        server_.on("/api/network/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
            handleDispatch(request, "WIFI_CONNECT " + quoteArg(ssid));
        });
        """
        routes = parse_backend_route_commands(source)
        self.assertEqual(
            routes.get(("POST", "/api/network/wifi/connect")),
            RouteCommand(route=("POST", "/api/network/wifi/connect"), command="WIFI_CONNECT", dynamic=False),
        )

    def test_backend_route_detects_dynamic_dispatch(self) -> None:
        source = """
        server_.on("/api/dispatch", HTTP_POST, [this](AsyncWebServerRequest* request) {
            handleDispatch(request, command_line);
        });
        """
        routes = parse_backend_route_commands(source)
        self.assertEqual(
            routes.get(("POST", "/api/dispatch")),
            RouteCommand(route=("POST", "/api/dispatch"), command=None, dynamic=True),
        )

    def test_registered_command_detection(self) -> None:
        source = """
        g_dispatcher.registerCommand("WIFI_CONNECT", [](const String&) {});
        g_dispatcher.registerCommand("PLAY", [](const String&) {});
        """
        commands = parse_registered_commands(source)
        self.assertEqual(commands, {"WIFI_CONNECT", "PLAY"})


class ParityReportTest(unittest.TestCase):
    def test_collects_missing_static_commands(self) -> None:
        backend = {
            ("POST", "/api/network/wifi/connect"): RouteCommand(
                route=("POST", "/api/network/wifi/connect"), command="WIFI_CONNECT", dynamic=False
            ),
            ("POST", "/api/control"): RouteCommand(
                route=("POST", "/api/control"), command="UNKNOWN", dynamic=False
            ),
            ("POST", "/api/relay"): RouteCommand(
                route=("POST", "/api/relay"), command="ESPNOW_SEND", dynamic=True
            ),
        }
        registered = {"WIFI_CONNECT"}
        missing = collect_missing_commands(backend, registered)

        command_payload = {m.command for m in missing if not m.dynamic}
        dynamic_payload = {m.route for m in missing if m.dynamic}
        self.assertIn("UNKNOWN", command_payload)
        self.assertIn(("POST", "/api/relay"), dynamic_payload)

    def test_build_report_includes_mapping_fields(self) -> None:
        backend = {
            ("GET", "/api/status"),
        }
        frontend = {
            ("GET", "/api/status"),
            ("POST", "/api/network/wifi/scan"),
        }
        missing_in_backend = frontend - backend
        backend_commands = {
            ("GET", "/api/status"): RouteCommand(("GET", "/api/status"), "STATUS", False),
            ("POST", "/api/network/wifi/scan"): RouteCommand(
                ("POST", "/api/network/wifi/scan"), "WIFI_SCAN", False
            ),
            ("POST", "/api/network/espnow/send"): RouteCommand(
                ("POST", "/api/network/espnow/send"), "ESPNOW_SEND", False
            ),
        }
        missing_commands = collect_missing_commands(backend_commands, {"STATUS"})
        report = build_report(
            backend_routes=backend,
            frontend_routes=frontend,
            missing_in_backend=missing_in_backend,
            unused_backend=set(),
            missing_commands=missing_commands,
            strict_unused_backend=False,
            status="fail",
        )

        self.assertIn("missing_mapped_commands", report)
        self.assertIn("dynamic_routes", report)
        self.assertEqual(
            report["missing_mapped_commands"],
            [
                {
                    "command": "ESPNOW_SEND",
                    "dynamic": False,
                    "method": "POST",
                    "path": "/api/network/espnow/send",
                },
                {
                    "command": "WIFI_SCAN",
                    "dynamic": False,
                    "method": "POST",
                    "path": "/api/network/wifi/scan",
                }
            ],
        )
        self.assertEqual(report["status"], "fail")

    def test_write_report_json_writes_json(self) -> None:
        report = build_report(
            backend_routes={("GET", "/api/status")},
            frontend_routes={("GET", "/api/status")},
            missing_in_backend=set(),
            unused_backend=set(),
            missing_commands=set(),
            strict_unused_backend=False,
            status="pass",
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "route_parity_report.json"
            write_report_json(path, report)
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["status"], "pass")
            self.assertEqual(loaded["backend_count"], 1)
            self.assertEqual(loaded["missing_mapped_commands"], [])


if __name__ == "__main__":
    unittest.main()

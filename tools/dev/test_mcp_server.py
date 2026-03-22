#!/usr/bin/env python3
"""
Test suite for MCP Hardware Server.

Tests the JSON-RPC protocol handling and tool dispatch by piping messages
to the server's process_message() method, with mocked HTTP calls to ESP32.

Usage:
    python3 test_mcp_server.py [-v]

All tests use stdlib only (unittest + unittest.mock).
"""

import json
import sys
import os
import unittest
from unittest.mock import patch, MagicMock
from io import BytesIO

# Add parent dir so we can import the server module
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from mcp_hardware_server import (
    MCPServer,
    ESP32Client,
    TOOL_DEFINITIONS,
    handle_initialize,
    handle_tools_list,
)


class TestInitialize(unittest.TestCase):
    """Test MCP initialize handshake."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080", "test-token")

    def test_initialize_returns_protocol_version(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "init-1",
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "test-client", "version": "0.1"},
                "capabilities": {},
            },
        })
        resp = self.server.process_message(msg)
        self.assertIsNotNone(resp)
        self.assertEqual(resp["jsonrpc"], "2.0")
        self.assertEqual(resp["id"], "init-1")
        self.assertIn("result", resp)
        self.assertEqual(resp["result"]["protocolVersion"], "2024-11-05")
        self.assertIn("serverInfo", resp["result"])
        self.assertEqual(resp["result"]["serverInfo"]["name"], "zacus-hardware")
        self.assertIn("capabilities", resp["result"])

    def test_initialized_notification_no_response(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "method": "notifications/initialized",
        })
        resp = self.server.process_message(msg)
        self.assertIsNone(resp)
        self.assertTrue(self.server.initialized)


class TestToolsList(unittest.TestCase):
    """Test MCP tools/list."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    def test_tools_list_returns_all_tools(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "list-1",
            "method": "tools/list",
        })
        resp = self.server.process_message(msg)
        self.assertEqual(resp["id"], "list-1")
        tools = resp["result"]["tools"]
        self.assertEqual(len(tools), 6)
        names = {t["name"] for t in tools}
        self.assertEqual(names, {
            "puzzle_set_state",
            "audio_play",
            "led_set",
            "camera_capture",
            "scenario_advance",
            "device_status",
        })

    def test_each_tool_has_input_schema(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "list-2",
            "method": "tools/list",
        })
        resp = self.server.process_message(msg)
        for tool in resp["result"]["tools"]:
            self.assertIn("inputSchema", tool, f"{tool['name']} missing inputSchema")
            self.assertIn("properties", tool["inputSchema"])
            self.assertIn("required", tool["inputSchema"])


class TestToolCallDeviceStatus(unittest.TestCase):
    """Test device_status tool call with mocked HTTP."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080", "tok123")

    def _make_call_msg(self, tool_name, arguments, id_="call-1"):
        return json.dumps({
            "jsonrpc": "2.0",
            "id": id_,
            "method": "tools/call",
            "params": {"name": tool_name, "arguments": arguments},
        })

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_device_status_success(self, mock_urlopen):
        """device_status should proxy GET /api/status and return JSON."""
        mock_resp = MagicMock()
        mock_resp.status = 200
        mock_resp.read.return_value = json.dumps({
            "uptime": 3600,
            "free_heap": 120000,
            "wifi_rssi": -42,
            "step": "step_03",
        }).encode()
        mock_resp.headers = {"Content-Type": "application/json"}
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        msg = self._make_call_msg("device_status", {"device_id": "zacus-main"})
        resp = self.server.process_message(msg)

        self.assertEqual(resp["id"], "call-1")
        self.assertIn("result", resp)
        content = resp["result"]["content"]
        self.assertEqual(len(content), 1)
        self.assertEqual(content[0]["type"], "text")
        self.assertIn("zacus-main", content[0]["text"])
        self.assertIn("uptime", content[0]["text"])

        # Verify the URL called
        call_args = mock_urlopen.call_args
        req = call_args[0][0]
        self.assertTrue(req.full_url.endswith("/api/status"))
        self.assertEqual(req.get_method(), "GET")
        self.assertEqual(req.get_header("Authorization"), "Bearer tok123")

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_device_status_unreachable(self, mock_urlopen):
        """device_status should return error when ESP32 is unreachable."""
        import urllib.error
        mock_urlopen.side_effect = urllib.error.URLError("Connection refused")

        msg = self._make_call_msg("device_status", {"device_id": "zacus-main"})
        resp = self.server.process_message(msg)

        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32000)  # DEVICE_UNREACHABLE
        self.assertIn("unreachable", resp["error"]["message"].lower())

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_device_status_auth_failure(self, mock_urlopen):
        """device_status should return auth error on 401."""
        import urllib.error
        exc = urllib.error.HTTPError(
            url="http://fake-esp32:8080/api/status",
            code=401,
            msg="Unauthorized",
            hdrs={},
            fp=BytesIO(b"Unauthorized"),
        )
        mock_urlopen.side_effect = exc

        msg = self._make_call_msg("device_status", {"device_id": "zacus-main"})
        resp = self.server.process_message(msg)

        # 401 is returned as an HTTP status, handler raises PermissionError
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32002)  # AUTH_FAILED


class TestToolCallLedSet(unittest.TestCase):
    """Test led_set tool call."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    def _make_call_msg(self, tool_name, arguments, id_="call-1"):
        return json.dumps({
            "jsonrpc": "2.0",
            "id": id_,
            "method": "tools/call",
            "params": {"name": tool_name, "arguments": arguments},
        })

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_led_set_success(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.status = 200
        mock_resp.read.return_value = b'{"ok":true}'
        mock_resp.headers = {"Content-Type": "application/json"}
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        msg = self._make_call_msg("led_set", {
            "device_id": "zacus-main",
            "zone": "puzzle",
            "color": "#00FF00",
            "pattern": "pulse",
            "brightness": 200,
        })
        resp = self.server.process_message(msg)

        self.assertIn("result", resp)
        text = resp["result"]["content"][0]["text"]
        self.assertIn("puzzle", text)
        self.assertIn("#00FF00", text)
        self.assertIn("pulse", text)
        self.assertIn("200", text)

        # Check the POST body sent to ESP32
        req = mock_urlopen.call_args[0][0]
        self.assertTrue(req.full_url.endswith("/api/led"))
        self.assertEqual(req.get_method(), "POST")
        sent_body = json.loads(req.data)
        self.assertEqual(sent_body["zone"], "puzzle")
        self.assertEqual(sent_body["color"], "#00FF00")
        self.assertEqual(sent_body["pattern"], "pulse")
        self.assertEqual(sent_body["bright"], 200)

    def test_led_set_missing_required(self):
        msg = self._make_call_msg("led_set", {
            "device_id": "zacus-main",
            "color": "#FF0000",
            # missing "zone"
        })
        resp = self.server.process_message(msg)
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32602)  # INVALID_PARAMS
        self.assertIn("zone", resp["error"]["message"])


class TestToolCallPuzzle(unittest.TestCase):
    """Test puzzle_set_state tool call."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    def _make_call_msg(self, arguments, id_="call-1"):
        return json.dumps({
            "jsonrpc": "2.0",
            "id": id_,
            "method": "tools/call",
            "params": {"name": "puzzle_set_state", "arguments": arguments},
        })

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_puzzle_unlock(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.status = 200
        mock_resp.read.return_value = b'{"state":"unlocked"}'
        mock_resp.headers = {"Content-Type": "application/json"}
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        msg = self._make_call_msg({
            "device_id": "zacus-main",
            "puzzle_id": "PUZZLE_COFFRE",
            "state": "unlocked",
        })
        resp = self.server.process_message(msg)
        self.assertIn("result", resp)
        self.assertIn("PUZZLE_COFFRE", resp["result"]["content"][0]["text"])
        self.assertIn("unlocked", resp["result"]["content"][0]["text"])

    def test_puzzle_invalid_state(self):
        msg = self._make_call_msg({
            "device_id": "zacus-main",
            "puzzle_id": "PUZZLE_COFFRE",
            "state": "exploded",
        })
        resp = self.server.process_message(msg)
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32602)


class TestProtocolErrors(unittest.TestCase):
    """Test JSON-RPC error handling."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    def test_malformed_json(self):
        resp = self.server.process_message("{not valid json")
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32700)

    def test_missing_jsonrpc_field(self):
        resp = self.server.process_message(json.dumps({"id": 1, "method": "foo"}))
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32600)

    def test_unknown_method(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "err-1",
            "method": "resources/list",
        })
        resp = self.server.process_message(msg)
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32601)

    def test_unknown_tool_name(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "err-2",
            "method": "tools/call",
            "params": {"name": "nonexistent_tool", "arguments": {}},
        })
        resp = self.server.process_message(msg)
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32601)

    def test_tools_call_missing_params(self):
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "err-3",
            "method": "tools/call",
        })
        resp = self.server.process_message(msg)
        self.assertIn("error", resp)
        self.assertEqual(resp["error"]["code"], -32602)


class TestCameraCapture(unittest.TestCase):
    """Test camera_capture tool with binary response."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_camera_returns_base64_image(self, mock_urlopen):
        fake_jpeg = b"\xff\xd8\xff\xe0" + b"\x00" * 100  # fake JPEG header
        mock_resp = MagicMock()
        mock_resp.status = 200
        mock_resp.read.return_value = fake_jpeg
        mock_resp.headers = {"Content-Type": "image/jpeg"}
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "cam-1",
            "method": "tools/call",
            "params": {
                "name": "camera_capture",
                "arguments": {"device_id": "zacus-main", "resolution": "VGA"},
            },
        })
        resp = self.server.process_message(msg)
        self.assertIn("result", resp)
        content = resp["result"]["content"]
        # Should have image + text
        self.assertEqual(len(content), 2)
        self.assertEqual(content[0]["type"], "image")
        self.assertEqual(content[0]["mimeType"], "image/jpeg")
        # Verify base64 is decodable
        import base64
        decoded = base64.b64decode(content[0]["data"])
        self.assertEqual(decoded, fake_jpeg)

        # Check query params
        req = mock_urlopen.call_args[0][0]
        self.assertIn("res=VGA", req.full_url)


class TestScenarioAdvance(unittest.TestCase):
    """Test scenario_advance tool."""

    def setUp(self):
        self.server = MCPServer("http://fake-esp32:8080")

    @patch("mcp_hardware_server.urllib.request.urlopen")
    def test_scenario_advance_with_target(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.status = 200
        mock_resp.read.return_value = b'{"ok":true}'
        mock_resp.headers = {"Content-Type": "application/json"}
        mock_resp.__enter__ = lambda s: s
        mock_resp.__exit__ = MagicMock(return_value=False)
        mock_urlopen.return_value = mock_resp

        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": "sc-1",
            "method": "tools/call",
            "params": {
                "name": "scenario_advance",
                "arguments": {
                    "device_id": "zacus-main",
                    "event_type": "manual",
                    "event_name": "MANUAL_ADVANCE",
                    "target_step_id": "step_05",
                },
            },
        })
        resp = self.server.process_message(msg)
        self.assertIn("result", resp)

        req = mock_urlopen.call_args[0][0]
        sent_body = json.loads(req.data)
        self.assertEqual(sent_body["event_type"], "manual")
        self.assertEqual(sent_body["event_name"], "MANUAL_ADVANCE")
        self.assertEqual(sent_body["target"], "step_05")


if __name__ == "__main__":
    unittest.main()

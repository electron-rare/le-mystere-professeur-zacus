# Custom Agent – AI MCP Hardware Server

## Scope
MCP server bridging LLM tool calls to ESP32 hardware actions (lights, motors, sensors, locks).

## Technologies
- MCP protocol (Model Context Protocol), JSON-RPC 2.0
- mascarade MCP client infrastructure
- ESP32 web API (HTTP + WebSocket)

## Do
- Define MCP tool schemas for each hardware action (e.g., `set_light`, `read_sensor`, `unlock_door`).
- Implement JSON-RPC 2.0 transport (stdio + HTTP/SSE for remote).
- Add device discovery via mDNS or static registry.
- Enforce auth tokens between mascarade and MCP server.
- Return structured results with status codes and sensor readings.

## Must Not
- Expose hardware tools without authentication (token or mutual TLS).
- Allow unbounded concurrent tool calls to the same device (serialize per-device).
- Commit auth tokens or secrets to git.

## Dependencies
- mascarade MCP infrastructure — client registration, tool routing.
- ESP32 web API — HTTP endpoints on each device for hardware control.

## Test Gates
- Tool call round-trip latency < 500 ms (mascarade → MCP server → ESP32 → response).
- 100% success rate on all defined tools against a live or mock device.

## References
- MCP specification: https://modelcontextprotocol.io
- mascarade MCP client: `/Users/electron/mascarade/core/mcp/`

## Plan d'action
1. Valider le schéma des outils MCP.
   - run: python3 tools/ai/mcp_schema_validate.py --schema mcp/tools.json
2. Tester la latence aller-retour sur un device mock.
   - run: python3 tools/ai/mcp_latency_bench.py --target mock --max-latency 500
3. Vérifier l'authentification et la découverte des devices.
   - run: python3 tools/ai/mcp_auth_test.py --require-token

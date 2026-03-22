# Security Specification

## Status
- State: draft
- Date: 2026-03-21
- Supersedes: previous 4-line stub
- Related: `specs/AI_INTEGRATION_SPEC.md`, `specs/MCP_HARDWARE_SERVER_SPEC.md`

## 1) Threat Model

### 1.1 Deployment Context

The Zacus escape room system operates on a **local WiFi network** in a controlled physical space. It is not exposed to the public internet. The primary attack surface is:

| Vector | Risk | Likelihood |
|--------|------|-----------|
| Local network access (same WiFi) | Players or visitors on the game WiFi | HIGH |
| Physical access to ESP32 device | Players can touch/see the hardware | HIGH |
| Firmware extraction (USB/JTAG) | Sophisticated attacker with tools | LOW |
| Server-side (mascarade VM) | Only reachable via LAN/Tailscale | LOW |
| Supply chain (dependencies) | Compromised PlatformIO/npm package | VERY LOW |

### 1.2 Assets to Protect

| Asset | Sensitivity | Impact if Compromised |
|-------|------------|----------------------|
| WiFi credentials | HIGH | Network access, lateral movement |
| API bearer tokens | HIGH | Full device control |
| Game scenario content | MEDIUM | Spoilers, cheating |
| Player interaction data | LOW | Privacy (no PII collected) |
| Firmware binary | LOW | IP, reverse engineering |
| mascarade API key | HIGH | Full orchestration control |

### 1.3 Threat Actors

1. **Curious player**: Tries to cheat by accessing the API from their phone
2. **Tech-savvy visitor**: Scans the network, finds ESP32 endpoints
3. **Malicious local user**: Attempts injection, DoS, or firmware dump
4. **Remote attacker**: Not applicable (no internet exposure)

## 2) API Authentication

### 2.1 Bearer Token Scheme

All ESP32 HTTP API endpoints require a Bearer token:

```
GET /api/status
Authorization: Bearer <token>

Response 200: { "step": "STEP_U_SON_PROTO", ... }
Response 401: { "error": "unauthorized" }
```

### 2.2 Token Lifecycle

| Phase | Mechanism |
|-------|-----------|
| Generation | Random 48-char hex string (cryptographically secure) |
| Storage (ESP32) | NVS encrypted partition |
| Storage (server) | `.env` file, not committed to git |
| Provisioning | QR code scanned at setup (see 3.2) |
| Rotation | Manual, at each deployment |
| Revocation | Reflash NVS or change `.env` |

### 2.3 Endpoint Protection Matrix

| Endpoint | Auth Required | Rate Limit | Notes |
|----------|:------------:|:----------:|-------|
| `GET /api/status` | Yes | 10/s | Device health |
| `GET /api/scenario` | Yes | 5/s | Current state |
| `POST /api/scenario/transition` | Yes | 2/s | State change |
| `POST /api/audio` | Yes | 5/s | Audio control |
| `POST /api/led` | Yes | 10/s | LED control |
| `GET /api/camera` | Yes | 2/s | Snapshot |
| `POST /api/puzzle` | Yes | 2/s | Puzzle state |
| `GET /` | No | 20/s | Web UI (static) |
| `GET /health` | No | 20/s | Health check |
| `WS /ws` | Yes (on connect) | — | Event stream |

### 2.4 Implementation (Firmware)

```cpp
// Auth middleware for AsyncWebServer
bool checkAuth(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Authorization")) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }
    String authHeader = request->header("Authorization");
    if (!authHeader.startsWith("Bearer ")) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }
    String token = authHeader.substring(7);
    String storedToken = nvs_get_string("auth", "api_token");
    if (token != storedToken) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }
    return true;
}
```

## 3) WiFi Credential Management

### 3.1 Current State (CRITICAL — FW-01)

WiFi credentials are currently **hardcoded** in `storage_manager.cpp:73`. This must be remediated immediately.

### 3.2 Target: NVS + QR Provisioning

**Storage**: ESP32 NVS (Non-Volatile Storage) encrypted partition.

**Provisioning flow**:
1. First boot: ESP32 starts in AP mode (`Zacus-Setup`)
2. Game master connects and opens captive portal
3. Game master scans QR code containing WiFi config:
   ```
   WIFI:T:WPA;S:EscapeRoom-Net;P:s3cur3p4ss;;
   ```
4. ESP32 stores credentials in NVS, restarts in STA mode
5. On connection failure: fallback to AP mode after 30 s

**NVS keys**:
| Key | Namespace | Encrypted |
|-----|-----------|:---------:|
| `wifi_ssid` | `network` | Yes |
| `wifi_pass` | `network` | Yes |
| `api_token` | `auth` | Yes |
| `device_id` | `device` | No |
| `mcp_token` | `auth` | Yes |

### 3.3 NVS Encryption

Enable NVS encryption with a flash encryption key:
```ini
# platformio.ini
board_build.partitions = partitions_encrypted.csv
board_build.flash_mode = dio
board_build.esp-idf.sdkconfig =
    CONFIG_NVS_ENCRYPTION=y
    CONFIG_SECURE_FLASH_ENC_ENABLED=y
```

## 4) Input Validation

### 4.1 Requirements

All API inputs must be validated before processing:

| Input | Validation |
|-------|-----------|
| JSON body | Max size 4 KB, max depth 5 levels |
| `step_id` | Alphanumeric + underscore, max 64 chars, must exist in scenario |
| `event_name` | Alphanumeric + underscore, max 64 chars |
| `audio_path` | Must start with `/audio/`, no `..`, max 128 chars |
| `color` | Regex `^#[0-9a-fA-F]{6}$` or named color enum |
| `volume` | Integer 0-100 |
| `brightness` | Integer 0-255 |
| `device_id` | Alphanumeric + hyphen, max 32 chars |

### 4.2 JSON Parsing Safety

```cpp
// Safe JSON parsing with limits
StaticJsonDocument<4096> doc;  // 4 KB max
DeserializationError err = deserializeJson(doc, body, DeserializationOption::NestingLimit(5));
if (err) {
    request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
    return;
}
```

### 4.3 String Safety

- All string operations use length-bounded functions (`strncpy`, `snprintf`)
- No `sprintf` or unbounded `strcpy` in firmware code
- LVGL text buffers use fixed-size arrays with null termination

## 5) Rate Limiting

### 5.1 Token Bucket Algorithm

Each endpoint has a token bucket rate limiter:

```cpp
struct RateLimiter {
    uint32_t tokens;
    uint32_t max_tokens;
    uint32_t refill_rate_ms;  // ms per token refill
    uint32_t last_refill;
};
```

### 5.2 Limits

| Category | Rate | Burst |
|----------|------|-------|
| Read endpoints (GET) | 10/s | 20 |
| Write endpoints (POST) | 5/s | 10 |
| Camera capture | 2/s | 3 |
| State transitions | 2/s | 5 |
| WebSocket messages | 20/s | 50 |
| Static files | 20/s | 50 |

### 5.3 Response on Limit

```
HTTP 429 Too Many Requests
Retry-After: 1
Content-Type: application/json

{"error": "rate_limited", "retry_after_ms": 1000}
```

## 6) CORS Policy

### 6.1 Configuration

```cpp
// CORS headers for AsyncWebServer
server.on("/*", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", allowedOrigin);
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
});
```

### 6.2 Allowed Origins

| Environment | Allowed Origin |
|-------------|---------------|
| Development | `http://localhost:5173` (Vite dev server) |
| Production | `http://<esp32-ip>:8080` (self-hosted UI) |
| Game master | `http://<gm-dashboard-ip>:3000` |

Wildcard `*` is **never** used.

## 7) Future Security Enhancements

### 7.1 Secure Boot (Phase 2)

Enable ESP32-S3 Secure Boot v2:
- RSA-3072 signature verification on boot
- Prevents unsigned firmware from running
- Key stored in eFuse (one-time programmable)

```ini
# platformio.ini (future)
board_build.esp-idf.sdkconfig =
    CONFIG_SECURE_BOOT=y
    CONFIG_SECURE_BOOT_V2_ENABLED=y
```

### 7.2 Flash Encryption (Phase 2)

Enable flash encryption to protect firmware and NVS:
- AES-256-XTS encryption
- Prevents firmware extraction via SPI flash dump
- Development mode allows re-flashing; release mode is permanent

### 7.3 TLS / HTTPS (Phase 3)

Upgrade HTTP API to HTTPS:
- Self-signed certificate stored in NVS
- Certificate pinning in frontend and MCP client
- Adds ~50 KB RAM overhead on ESP32

### 7.4 OTA Security (Phase 3)

Signed OTA updates:
- Firmware images signed with project key
- ESP32 verifies signature before applying update
- Rollback on failed verification

## 8) Incident Response

### 8.1 Severity Levels

| Level | Description | Example |
|-------|------------|---------|
| SEV-1 | Game completely compromised | Token leaked, full device control by attacker |
| SEV-2 | Security bypass discovered | Auth bypass, injection working |
| SEV-3 | Vulnerability found, not exploited | Missing validation on one endpoint |
| SEV-4 | Hardening improvement | Missing rate limit on low-risk endpoint |

### 8.2 Response Procedure

1. **Detect**: Monitor serial logs, API access patterns, unexpected state changes
2. **Contain**: Power off affected devices if SEV-1/SEV-2
3. **Assess**: Identify scope (which devices, which data)
4. **Remediate**:
   - Rotate all tokens immediately
   - Reflash firmware with fix
   - Update NVS credentials
5. **Review**: Post-incident analysis, update threat model

### 8.3 Contact

Security issues: open a private issue on the GitHub repository or contact the project maintainer directly. Do not disclose vulnerabilities publicly before a fix is available.

## 9) Security Checklist (Pre-Deployment)

- [ ] WiFi credentials stored in NVS, not in source code
- [ ] API bearer token generated and provisioned
- [ ] All API endpoints require authentication
- [ ] Input validation on all POST endpoints
- [ ] Rate limiting enabled
- [ ] CORS configured with specific origins
- [ ] No credentials in git history
- [ ] Serial debug output disabled in production build
- [ ] Firmware compiled with optimization (-Os)
- [ ] NVS encryption enabled
- [ ] Default passwords changed
- [ ] `.env` file excluded from git (in `.gitignore`)

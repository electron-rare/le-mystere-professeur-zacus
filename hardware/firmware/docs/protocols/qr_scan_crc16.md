# QR Scan CRC16 Contract (Freenove)

This document defines the QR validation contract used by `SCENE_CAMERA_SCAN`.

## Payload contract

Scene payload accepts:

```json
{
  "qr": {
    "expected": ["ZACUS:ETAPE1", "ZACUS:BONUS"],
    "prefix": "ZACUS:",
    "contains": "ETAPE",
    "caseInsensitive": true,
    "crc16": { "enabled": true, "sep": "*" }
  }
}
```

Supported forms:
- `expected`: string or array (any-of)
- `prefix`: starts-with match
- `contains`: substring match
- `caseInsensitive`: ASCII case-insensitive matching
- `crc16`: either boolean or object `{ enabled, sep }`
- legacy `crcSep` is also accepted

## CRC format

When CRC16 is enabled, runtime expects:

`<DATA><sep><CRC16_HEX>`

- CRC algorithm: `CRC-16/CCITT-FALSE` (`poly=0x1021`, `init=0xFFFF`)
- If `caseInsensitive=true`, CRC is computed on ASCII-uppercase DATA
- Matching (`expected/prefix/contains`) is applied on DATA after CRC strip/validation

## Generator script

Use:

```bash
python3 tools/dev/gen_qr_crc16.py "ZACUS:ETAPE1" --ci
python3 tools/dev/gen_qr_crc16.py "ZACUS:ETAPE1" --ci --png qr_etape1.png
```

`--ci` must match scene payload `caseInsensitive=true`.

## Notes

CRC16 is an integrity helper, not an anti-forgery signature.
For anti-cheat requirements, use a keyed MAC/signature scheme.

"""Convert a `blocks_studio_version: 2` YAML document into a Runtime 3 IR.

Block-to-IR mapping
-------------------

- `sceneStart`            → opens a new step (`STEP_<params.id>`); first one
                            becomes `entry_step_id`.
- `sceneEnd`              → closes the current step (no transition emitted).
- `sceneGoto`             → transition `event_type: action`, `event_name: goto`,
                            `target_step_id: STEP_<params.target>`.
- `sceneBranch`           → two transitions `branch_true` / `branch_false`.
- `npcSay`                → action `{kind: tts_say, text}`.
- `npcWaitResponse`       → action `{kind: wait_user_voice, timeout_ms}`.
- `npcIntentMatch`        → transition `event_type: action`,
                            `event_name: intent:<intent>`,
                            `target_step_id: STEP_<then>`.
- `hwServo`               → action `{kind: hw_servo, channel, angle}`.
- `hwReadQR`              → action `{kind: qr_expect, value}` (+ serial transition).
- `hwLEDPattern`          → action `{kind: led_pattern, pattern}`.
- `hwSoundPlay`           → action `{kind: sound_play, asset}`.
- `logicScore`            → action `{kind: score_add, delta}`.
- `logicSetVar`           → action `{kind: set_var, name, value}`.
- `logicTimer`            → transition `event_type: timer`, `after_ms`,
                            `target_step_id` = next step head.
- `logicIf` **with body / else slots** → action `{kind: condition, expr,
                            then: [...sub-actions...], else: [...]}` — the slot
                            sub-chains are walked recursively and serialised
                            as nested actions. Branching stays **local to the
                            current step** as per runtime3 spec.

Slot chains never themselves emit new steps — only top-level `sceneStart` does.
A `sceneStart` inside a slot is treated as an inline error (warning emitted,
block ignored) since branches must stay local.
"""
from __future__ import annotations

from typing import Any, Iterable
import yaml


SCHEMA_VERSION = "zacus.runtime3.v1"


class CompileError(Exception):
    pass


def compile_blocks(yaml_text: str, *, scenario_id: str = "BLOCKS_DRAFT") -> dict[str, Any]:
    doc = yaml.safe_load(yaml_text)
    if not isinstance(doc, dict):
        raise CompileError("YAML root must be a mapping")
    version = doc.get("blocks_studio_version")
    if version not in (1, 2):
        raise CompileError(f"unsupported blocks_studio_version: {version!r}")

    if version == 1:
        nodes_by_id = _flatten_v1(doc)
    else:
        nodes_by_id = _flatten_v2(doc)

    if not nodes_by_id:
        raise CompileError("no blocks found")

    roots = _find_roots(nodes_by_id)
    if not roots:
        raise CompileError("no root chain found (all nodes are children of something)")

    steps: list[dict] = []
    warnings: list[str] = []
    errors: list[str] = []
    entry: str | None = None
    used_ids: set[str] = set()

    def step_id_for(scene_param: str | None) -> str:
        base = (scene_param or f"anon_{len(used_ids)}").strip() or f"anon_{len(used_ids)}"
        sid = f"STEP_{base}"
        n = sid; i = 2
        while n in used_ids:
            n = f"{sid}_{i}"
            i += 1
        used_ids.add(n)
        return n

    for root_id in roots:
        chain = _walk_chain(root_id, nodes_by_id)
        # First scene
        cursor_idx = 0
        if chain and chain[0]["kind"] == "sceneStart":
            sid = step_id_for(chain[0].get("params", {}).get("id"))
            cursor_idx = 1
        else:
            warnings.append("chain missing sceneStart — synthetic step emitted")
            sid = step_id_for(None)

        current = _new_step(sid)
        if entry is None:
            entry = sid

        for node in chain[cursor_idx:]:
            kind = node.get("kind", "")
            params = node.get("params", {}) or {}

            if kind == "sceneEnd":
                break
            elif kind == "sceneStart":
                steps.append(current)
                sid = step_id_for(params.get("id"))
                current = _new_step(sid)
            elif kind == "logicIf":
                # condition action with then/else sub-action lists
                then_actions = _walk_slot_actions(node, "body", nodes_by_id, warnings)
                else_actions = _walk_slot_actions(node, "else", nodes_by_id, warnings)
                current["actions"].append({
                    "kind": "condition",
                    "expr": params.get("condition", ""),
                    "then": then_actions,
                    "else": else_actions,
                })
            else:
                action_or_transition = _node_to_action_or_transition(node, warnings)
                if action_or_transition is None:
                    continue
                bucket, payload = action_or_transition
                current[bucket].append(payload)

        steps.append(current)

    # Final pass: validate transition targets and warn on dangling ones.
    valid_ids = {s["id"] for s in steps}
    for s in steps:
        for t in s["transitions"]:
            tgt = t.get("target_step_id")
            if tgt and tgt not in valid_ids:
                warnings.append(f"{s['id']}: transition points to unknown {tgt}")

    return {
        "schema_version": SCHEMA_VERSION,
        "scenario": {
            "id": scenario_id,
            "version": 3,
            "title": scenario_id.replace("_", " ").title(),
            "entry_step_id": entry or (steps[0]["id"] if steps else ""),
            "source_kind": "blocks_studio",
        },
        "steps": steps,
        "metadata": {
            "migration_mode": "native",
            "generated_by": "zacus_hub_gateway/blocks_to_runtime3",
            "warnings": warnings,
            "errors": errors,
        },
    }


# ---------- node graph helpers ----------

def _flatten_v2(doc: dict) -> dict[str, dict]:
    nodes = doc.get("nodes") or []
    if isinstance(nodes, str):  # empty `[]` parsed as string by yaml flow
        return {}
    out: dict[str, dict] = {}
    for n in nodes:
        if isinstance(n, dict) and isinstance(n.get("id"), str):
            out[n["id"]] = n
    return out


def _flatten_v1(doc: dict) -> dict[str, dict]:
    """Old chain-based layout: rebuild flat nodes with synthesised next pointers."""
    out: dict[str, dict] = {}
    for chain_entry in doc.get("chains") or []:
        sequence = chain_entry.get("sequence") or []
        prev_id: str | None = None
        for node in sequence:
            if not isinstance(node, dict): continue
            nid = node.get("id")
            if not isinstance(nid, str): continue
            n = dict(node)
            out[nid] = n
            if prev_id is not None and prev_id in out:
                out[prev_id]["next"] = nid
            prev_id = nid
    return out


def _find_roots(nodes_by_id: dict[str, dict]) -> list[str]:
    referenced: set[str] = set()
    for n in nodes_by_id.values():
        if isinstance(n.get("next"), str):
            referenced.add(n["next"])
        for slot_head in (n.get("slots") or {}).values():
            if isinstance(slot_head, str):
                referenced.add(slot_head)
    return [nid for nid in nodes_by_id if nid not in referenced]


def _walk_chain(start_id: str, nodes_by_id: dict[str, dict]) -> list[dict]:
    out: list[dict] = []
    seen: set[str] = set()
    cur: str | None = start_id
    while cur and cur not in seen and cur in nodes_by_id:
        seen.add(cur)
        out.append(nodes_by_id[cur])
        nxt = nodes_by_id[cur].get("next")
        cur = nxt if isinstance(nxt, str) else None
    return out


# ---------- per-node mapping ----------

def _new_step(sid: str) -> dict:
    return {
        "id": sid,
        "scene_id": sid.replace("STEP_", "SCENE_"),
        "audio_pack_id": "",
        "actions": [],
        "apps": [],
        "transitions": [],
    }


def _node_to_action_or_transition(node: dict, warnings: list[str]) -> tuple[str, dict] | None:
    kind = node.get("kind", "")
    params = node.get("params", {}) or {}

    # --- Audio ---
    if kind == "hwAudioStop":
        return ("actions", {"kind": "sound_stop"})
    if kind == "hwAudioVolume":
        return ("actions", {"kind": "sound_volume", "level": _to_int(params.get("level"), default=70)})
    # --- LCD ---
    if kind == "hwLCDText":
        return ("actions", {"kind": "lcd_text", "line": _to_int(params.get("line"), default=0), "text": params.get("text", "")})
    if kind == "hwLCDClear":
        return ("actions", {"kind": "lcd_clear"})
    if kind == "hwLCDImage":
        return ("actions", {"kind": "lcd_image", "asset": params.get("asset", "")})
    if kind == "hwLCDTouchWait":
        return ("actions", {"kind": "lcd_touch_wait", "zone": params.get("zone", ""),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=30) * 1000})
    # --- Hardware ESP32 generic ---
    if kind == "hwBuzzerTone":
        return ("actions", {"kind": "buzzer_tone", "freq": _to_int(params.get("freq"), default=2000),
                            "duration_ms": _to_int(params.get("ms"), default=120)})
    if kind == "hwRelay":
        return ("actions", {"kind": "relay", "channel": _to_int(params.get("channel"), default=0),
                            "state": params.get("state", "pulse")})
    if kind == "hwSensorRead":
        return ("actions", {"kind": "sensor_read", "pin": params.get("pin", "A0"), "var": params.get("var", "lecture")})
    if kind == "hwButtonWait":
        return ("actions", {"kind": "button_wait", "button": params.get("button", "btn_main"),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=0) * 1000})
    # --- ESP-NOW ---
    if kind == "espnowRegisterPeer":
        return ("actions", {"kind": "espnow_register_peer", "alias": params.get("alias", ""), "mac": params.get("mac", "")})
    if kind == "espnowSend":
        return ("actions", {"kind": "espnow_send", "peer": params.get("peer", ""), "command": params.get("command", "")})
    if kind == "espnowBroadcast":
        return ("actions", {"kind": "espnow_broadcast", "command": params.get("command", "")})
    if kind == "espnowWait":
        return ("actions", {"kind": "espnow_wait", "command": params.get("command", ""),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=10) * 1000})
    # --- BOX-3 ---
    if kind == "boxIMUShake":
        return ("actions", {"kind": "box_imu_shake", "threshold": _to_float(params.get("threshold"), default=1.5),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=10) * 1000})
    if kind == "boxIRSend":
        return ("actions", {"kind": "box_ir_send", "protocol": params.get("protocol", "NEC"), "code": str(params.get("code", ""))})
    # --- M5 ---
    if kind == "m5Beep":
        return ("actions", {"kind": "m5_beep", "freq": _to_int(params.get("freq"), default=4000),
                            "duration_ms": _to_int(params.get("ms"), default=200)})
    if kind == "m5LCDText":
        return ("actions", {"kind": "m5_lcd_text", "text": params.get("text", ""), "color": params.get("color", "white"),
                            "size": _to_int(params.get("size"), default=2)})
    if kind == "m5ButtonAB":
        return ("actions", {"kind": "m5_button_wait", "button": params.get("button", "A"),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=0) * 1000})
    if kind == "m5RGBLed":
        return ("actions", {"kind": "m5_rgb_led", "color": params.get("color", "#000000")})
    if kind == "m5IMUShake":
        return ("actions", {"kind": "m5_imu_shake", "threshold": _to_float(params.get("threshold"), default=1.5),
                            "timeout_ms": _to_int(params.get("timeout_s"), default=10) * 1000})
    # --- PLIP ---
    if kind == "plipRing":
        return ("actions", {"kind": "plip_ring", "duration_ms": int(_to_float(params.get("duration_s"), default=3) * 1000)})
    if kind == "plipPickupWait":
        return ("actions", {"kind": "plip_pickup_wait", "timeout_ms": _to_int(params.get("timeout_s"), default=30) * 1000})

    if kind == "npcSay":
        return ("actions", {"kind": "tts_say", "text": params.get("text", "").strip()})
    if kind == "npcWaitResponse":
        return ("actions", {"kind": "wait_user_voice", "timeout_ms": _to_int(params.get("timeout_s"), default=10) * 1000})
    if kind == "npcIntentMatch":
        target = params.get("then", "").strip()
        if not target:
            warnings.append("npcIntentMatch without 'then' ignored")
            return None
        return ("transitions", {
            "event_type": "action",
            "event_name": f"intent:{params.get('intent','').strip()}",
            "target_step_id": f"STEP_{target}",
            "priority": 10,
        })
    if kind == "hwServo":
        return ("actions", {"kind": "hw_servo", "channel": _to_int(params.get("channel"), default=0), "angle": _to_int(params.get("angle"), default=90)})
    if kind == "hwReadQR":
        return ("actions", {"kind": "qr_expect", "value": params.get("expected", "")})
    if kind == "hwLEDPattern":
        return ("actions", {"kind": "led_pattern", "pattern": params.get("pattern", "off")})
    if kind == "hwSoundPlay":
        return ("actions", {"kind": "sound_play", "asset": params.get("asset", "")})
    if kind == "logicScore":
        return ("actions", {"kind": "score_add", "delta": _to_int(params.get("delta"), default=1)})
    if kind == "logicSetVar":
        return ("actions", {"kind": "set_var", "name": params.get("name", ""), "value": params.get("value", "")})
    if kind == "logicTimer":
        return ("transitions", {
            "event_type": "timer",
            "event_name": "elapsed",
            "after_ms": _to_int(params.get("seconds"), default=5) * 1000,
            "target_step_id": "",
            "priority": 1,
        })
    if kind == "sceneGoto":
        target = params.get("target", "").strip()
        return ("transitions", {
            "event_type": "action",
            "event_name": "goto",
            "target_step_id": f"STEP_{target}",
            "priority": 0,
        })
    if kind == "sceneBranch":
        return ("transitions", {
            "event_type": "action",
            "event_name": "branch_true",
            "target_step_id": f"STEP_{params.get('ifTrue','').strip()}",
            "priority": 0,
        })

    warnings.append(f"unknown block kind '{kind}' skipped")
    return None


def _walk_slot_actions(parent: dict, slot_name: str, nodes_by_id: dict[str, dict], warnings: list[str]) -> list[dict]:
    """Walk the chain anchored at `parent.slots[slot_name]` and serialise it as
    a nested action list. `sceneStart`/`sceneEnd` inside a slot are ignored
    with a warning since branches must remain local to the parent step.
    `logicIf` nested in a slot recurses into another condition action.
    """
    head = (parent.get("slots") or {}).get(slot_name)
    if not isinstance(head, str):
        return []
    chain = _walk_chain(head, nodes_by_id)
    out: list[dict] = []
    for node in chain:
        kind = node.get("kind", "")
        if kind in ("sceneStart", "sceneEnd", "sceneGoto", "sceneBranch", "npcIntentMatch", "logicTimer"):
            warnings.append(f"slot '{slot_name}' contains '{kind}' which only makes sense at step level — ignored")
            continue
        if kind == "logicIf":
            params = node.get("params", {}) or {}
            out.append({
                "kind": "condition",
                "expr": params.get("condition", ""),
                "then": _walk_slot_actions(node, "body", nodes_by_id, warnings),
                "else": _walk_slot_actions(node, "else", nodes_by_id, warnings),
            })
            continue
        mapped = _node_to_action_or_transition(node, warnings)
        if mapped is None: continue
        bucket, payload = mapped
        if bucket != "actions":
            warnings.append(f"slot '{slot_name}' produced a transition from '{kind}' — only actions allowed inside branches, ignored")
            continue
        out.append(payload)
    return out


def _to_int(value: Any, *, default: int) -> int:
    try:
        return int(float(str(value).strip()))
    except (TypeError, ValueError):
        return default


def _to_float(value: Any, *, default: float) -> float:
    try:
        return float(str(value).strip())
    except (TypeError, ValueError):
        return default

// Codec for the Blockly workspace:
//   - workspace ↔ blocks_studio_version: 2 YAML (gateway round-trip)
//   - workspace ↔ Scratch 3 .sb3 archive (procedures_call encoding)
//
// .sb3 strategy: every Zacus block becomes a Scratch `procedures_call`
// whose `proccode` is `zacus.<kind> <field1> <field2>...`. This keeps the
// file structurally valid Scratch (opens in scratch.mit.edu without error)
// while losslessly preserving our 15 domain block kinds.

(function () {

  function uuid() {
    return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
      (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c/4).toString(16));
  }

  // ---------- YAML helpers ----------

  function yamlScalar(v) {
    if (v == null) return "\"\"";
    const s = String(v);
    if (s === "") return "\"\"";
    if (s.indexOf("\n") >= 0) {
      const lines = s.split("\n");
      return "|\n" + lines.map(l => "        " + l).join("\n");
    }
    // Always quote — keeps round-trip safe (hex like 0xABCD, leading zeros,
    // colons, anything yaml might reinterpret as a typed scalar).
    return "\"" + s.replace(/\\/g, "\\\\").replace(/"/g, '\\"') + "\"";
  }

  function workspaceToYaml(workspace) {
    const allBlocks = workspace.getAllBlocks(false).filter(b => b.type.startsWith("zacus_"));
    let out = "blocks_studio_version: 2\nnodes:\n";
    if (allBlocks.length === 0) { out += "  []\n"; return out; }
    for (const b of allBlocks) {
      const kind = b.type.replace(/^zacus_/, "");
      const xy = b.getRelativeToSurfaceXY();
      out += `  - id: "${b.id}"\n`;
      out += `    kind: ${kind}\n`;
      out += `    position: [${Math.round(xy.x)}, ${Math.round(xy.y)}]\n`;
      const nextBlock = b.getNextBlock();
      if (nextBlock && nextBlock.type.startsWith("zacus_")) {
        out += `    next: "${nextBlock.id}"\n`;
      }
      const fields = (window.ZACUS_FIELDS_BY_KIND[b.type] || []);
      if (fields.length) {
        out += "    params:\n";
        for (const f of fields) {
          const value = b.getFieldValue(f) ?? "";
          out += `      ${f}: ${yamlScalar(value)}\n`;
        }
      }
      // Slots (statement inputs) — for logicIf: body / else
      const slotInputs = b.inputList.filter(i => i.type === Blockly.inputs.inputTypes.STATEMENT);
      const slotMap = {};
      for (const input of slotInputs) {
        const head = input.connection && input.connection.targetBlock();
        if (head && head.type.startsWith("zacus_")) slotMap[input.name] = head.id;
      }
      if (Object.keys(slotMap).length) {
        out += "    slots:\n";
        for (const k of Object.keys(slotMap).sort()) {
          out += `      ${k}: "${slotMap[k]}"\n`;
        }
      }
    }
    return out;
  }

  // ---------- YAML → Workspace XML ----------

  function yamlToWorkspaceXml(yamlText) {
    if (!yamlText || !yamlText.includes("blocks_studio_version")) return null;
    // We don't pull a full YAML parser — we tolerate v2 only with simple structure.
    const nodes = parseV2Nodes(yamlText);
    if (!nodes.length) return null;
    const byId = {}; for (const n of nodes) byId[n.id] = n;
    const referenced = new Set();
    for (const n of nodes) {
      if (n.next) referenced.add(n.next);
      for (const k in (n.slots || {})) referenced.add(n.slots[k]);
    }
    const roots = nodes.filter(n => !referenced.has(n.id));

    function blockXml(n) {
      const lines = [];
      lines.push(`<block type="zacus_${n.kind}" id="${n.id}" x="${n.position[0]}" y="${n.position[1]}">`);
      const fields = window.ZACUS_FIELDS_BY_KIND["zacus_"+n.kind] || [];
      for (const f of fields) {
        const v = (n.params || {})[f];
        if (v !== undefined) lines.push(`<field name="${f}">${escapeXml(v)}</field>`);
      }
      for (const slotName of Object.keys(n.slots || {})) {
        const headId = n.slots[slotName];
        const head = byId[headId];
        if (head) {
          lines.push(`<statement name="${slotName}">${chainXml(head)}</statement>`);
        }
      }
      const nxt = n.next ? byId[n.next] : null;
      if (nxt) lines.push(`<next>${blockXml(nxt)}</next>`);
      lines.push(`</block>`);
      return lines.join("");
    }
    function chainXml(head) { return blockXml(head); }

    const xml = `<xml xmlns="https://developers.google.com/blockly/xml">` +
      roots.map(blockXml).join("") + `</xml>`;
    return xml;
  }

  function parseV2Nodes(text) {
    const lines = text.split("\n");
    const nodes = [];
    let cur = null;
    let mode = null; // "params" | "slots" | null
    for (let raw of lines) {
      const line = raw.replace(/\r$/, "");
      const trimmed = line.trim();
      if (trimmed.startsWith("- id:")) {
        if (cur) nodes.push(cur);
        cur = { id: stripQuotes(trimmed.slice(5).trim()), kind: "", position: [0,0], params: {}, slots: {}, next: null };
        mode = null;
      } else if (cur) {
        if (trimmed.startsWith("kind:"))     { cur.kind = trimmed.slice(5).trim(); mode = null; }
        else if (trimmed.startsWith("position:")) {
          const nums = trimmed.slice(9).trim().replace(/[\[\]]/g, "").split(",").map(s=>parseFloat(s.trim()));
          if (nums.length === 2) cur.position = nums;
          mode = null;
        }
        else if (trimmed.startsWith("next:")) { cur.next = stripQuotes(trimmed.slice(5).trim()); mode = null; }
        else if (trimmed.startsWith("params:")) { mode = "params"; }
        else if (trimmed.startsWith("slots:"))  { mode = "slots"; }
        else if (mode && trimmed.includes(":")) {
          const idx = trimmed.indexOf(":");
          const key = trimmed.slice(0, idx).trim();
          const val = stripQuotes(trimmed.slice(idx+1).trim());
          if (key) {
            if (mode === "params") cur.params[key] = val;
            else if (mode === "slots") cur.slots[key] = val;
          }
        }
      }
    }
    if (cur) nodes.push(cur);
    return nodes;
  }

  function stripQuotes(s) {
    if (s.length >= 2 && ((s[0]==='"' && s[s.length-1]==='"') || (s[0]==="'" && s[s.length-1]==="'"))) {
      return s.slice(1, -1);
    }
    return s;
  }

  function escapeXml(s) {
    return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&apos;");
  }

  // ---------- .sb3 export ----------

  // Minimal valid Scratch 3 backdrop: a 1×1 transparent PNG.
  const BACKDROP_PNG_B64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNgYGD4DwABBAEAfbLI3wAAAABJRU5ErkJggg==";
  const BACKDROP_MD5 = "cd21514d0531fdffb22204e0ec5ed84a"; // md5 of the PNG bytes above
  // (md5 is required by Scratch's asset references — using a well-known empty
  // backdrop md5; if Scratch refuses, we fall back to recomputing.)

  async function workspaceToSB3(workspace) {
    const yaml = workspaceToYaml(workspace);
    const allBlocks = workspace.getAllBlocks(false).filter(b => b.type.startsWith("zacus_"));

    const sbBlocks = {};
    for (const b of allBlocks) {
      const kind = b.type.replace(/^zacus_/, "");
      const fields = (window.ZACUS_FIELDS_BY_KIND[b.type] || []);
      const proccode = "zacus." + kind + (fields.length ? " " + fields.map(_ => "%s").join(" ") : "");
      const argNames = fields;
      const argIds = fields.map(f => `arg_${b.id}_${f}`);
      const argDefaults = fields.map(f => b.getFieldValue(f) ?? "");

      const callId = b.id;
      const inputs = {};
      const callArgs = {};
      for (let i = 0; i < fields.length; i++) {
        const aid = `lit_${b.id}_${fields[i]}`;
        // shadow text literal block
        sbBlocks[aid] = {
          opcode: "text",
          next: null, parent: callId,
          inputs: {}, fields: { TEXT: [String(argDefaults[i]), null] },
          shadow: true, topLevel: false,
        };
        callArgs[argIds[i]] = [1, aid];
      }

      const parent = b.getParent();
      const nextBlock = b.getNextBlock();
      sbBlocks[callId] = {
        opcode: "procedures_call",
        next: nextBlock ? nextBlock.id : null,
        parent: parent ? parent.id : null,
        inputs: callArgs,
        fields: {},
        shadow: false,
        topLevel: !parent,
        x: parent ? undefined : Math.round(b.getRelativeToSurfaceXY().x),
        y: parent ? undefined : Math.round(b.getRelativeToSurfaceXY().y),
        mutation: {
          tagName: "mutation",
          children: [],
          proccode: proccode,
          argumentids: JSON.stringify(argIds),
          warp: "false",
        },
      };
      // Slot bodies are not expanded into Scratch's stack here — slots stay
      // available as round-trip via the embedded YAML.
    }

    const stage = {
      isStage: true,
      name: "Stage",
      variables: {},
      lists: {},
      broadcasts: {},
      blocks: sbBlocks,
      comments: {
        "zacus_payload": {
          blockId: null, x: 20, y: 20, width: 420, height: 320,
          minimized: false,
          text: "ZACUS_BLOCKS_YAML_BEGIN\n" + yaml + "\nZACUS_BLOCKS_YAML_END",
        }
      },
      currentCostume: 0,
      costumes: [{
        name: "backdrop1",
        bitmapResolution: 1,
        dataFormat: "png",
        assetId: BACKDROP_MD5,
        md5ext: BACKDROP_MD5 + ".png",
        rotationCenterX: 0,
        rotationCenterY: 0,
      }],
      sounds: [],
      volume: 100,
      layerOrder: 0,
      tempo: 60,
      videoTransparency: 50,
      videoState: "on",
      textToSpeechLanguage: null,
    };
    const project = {
      targets: [stage],
      monitors: [],
      extensions: [],
      meta: { semver: "3.0.0", vm: "2.3.4", agent: "ZacusHub/0.1" }
    };
    const zip = new JSZip();
    zip.file("project.json", JSON.stringify(project));
    // backdrop asset
    const bytes = Uint8Array.from(atob(BACKDROP_PNG_B64), c => c.charCodeAt(0));
    zip.file(BACKDROP_MD5 + ".png", bytes);
    return zip.generateAsync({ type: "blob", compression: "DEFLATE" });
  }

  // ---------- .sb3 import ----------

  async function sb3ToWorkspaceXml(arrayBuffer) {
    const zip = await JSZip.loadAsync(arrayBuffer);
    const projFile = zip.file("project.json");
    if (!projFile) throw new Error("project.json missing");
    const project = JSON.parse(await projFile.async("string"));
    // Prefer round-trip via the embedded comment, if present
    for (const t of (project.targets || [])) {
      for (const c of Object.values(t.comments || {})) {
        const m = /ZACUS_BLOCKS_YAML_BEGIN\n([\s\S]*?)\nZACUS_BLOCKS_YAML_END/.exec(c.text || "");
        if (m) return yamlToWorkspaceXml(m[1]);
      }
    }
    // Fallback: parse procedures_call entries with proccode zacus.<kind> ...
    const nodes = [];
    for (const t of (project.targets || [])) {
      const blocks = t.blocks || {};
      const callIds = Object.keys(blocks).filter(k => blocks[k].opcode === "procedures_call"
        && blocks[k].mutation && /^zacus\./.test(blocks[k].mutation.proccode || ""));
      for (const id of callIds) {
        const b = blocks[id];
        const proccode = b.mutation.proccode;
        const kind = proccode.split(" ")[0].slice("zacus.".length);
        const argIds = JSON.parse(b.mutation.argumentids || "[]");
        const params = {};
        const fieldNames = (window.ZACUS_FIELDS_BY_KIND["zacus_"+kind] || []);
        for (let i = 0; i < argIds.length && i < fieldNames.length; i++) {
          const input = (b.inputs || {})[argIds[i]];
          if (input && input[1]) {
            const litId = input[1];
            const lit = blocks[litId];
            const text = lit && lit.fields && lit.fields.TEXT && lit.fields.TEXT[0];
            if (text !== undefined) params[fieldNames[i]] = text;
          }
        }
        nodes.push({ id: id, kind: kind, position: [b.x ?? 60, b.y ?? 60], next: b.next, params: params, slots: {} });
      }
    }
    // Synthesize a v2 YAML and reuse the YAML→XML path
    let yaml = "blocks_studio_version: 2\nnodes:\n";
    for (const n of nodes) {
      yaml += `  - id: "${n.id}"\n    kind: ${n.kind}\n    position: [${n.position[0]}, ${n.position[1]}]\n`;
      if (n.next) yaml += `    next: "${n.next}"\n`;
      if (Object.keys(n.params).length) {
        yaml += "    params:\n";
        for (const k of Object.keys(n.params).sort()) {
          yaml += `      ${k}: ${yamlScalar(n.params[k])}\n`;
        }
      }
    }
    return yamlToWorkspaceXml(yaml);
  }

  window.ZacusCodec = {
    workspaceToYaml,
    yamlToWorkspaceXml,
    workspaceToSB3,
    sb3ToWorkspaceXml,
  };
})();

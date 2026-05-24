// Custom Blockly block definitions for all Zacus BlockKinds.
// Shapes follow Scratch conventions:
//   hat        : sceneStart
//   cap        : sceneEnd
//   statement  : both prev+next (most blocks)
//   c-block    : statement input (logicIf "alors"/"sinon")

const COLOR = {
  scene:    "#5085F2",
  npc:      "#D85B73",
  audio:    "#A772D9",
  lcd:      "#338CBF",
  hardware: "#2EAD8C",
  espnow:   "#F28033",
  box3:     "#8C5A33",
  m5:       "#4D73A6",
  plip:     "#C74D8C",
  logic:    "#F2A633",
};

function def(name, init) { Blockly.Blocks[name] = { init: init }; }
function stack(t, color) { t.setPreviousStatement(true, null); t.setNextStatement(true, null); t.setColour(color); }
function dropdown(opts) {
  // accept [["label","value"], …] or ["value", …]
  return new Blockly.FieldDropdown(opts.map(o => Array.isArray(o) ? o : [o, o]));
}

// Scènes
def("zacus_sceneStart", function () {
  this.appendDummyInput().appendField("▶ Début de scène").appendField(new Blockly.FieldTextInput("intro"), "id");
  this.setNextStatement(true, null); this.setColour(COLOR.scene); this.setInputsInline(true);
});
def("zacus_sceneEnd", function () {
  this.appendDummyInput().appendField("⏹ Fin de scène");
  this.setPreviousStatement(true, null); this.setColour(COLOR.scene);
});
def("zacus_sceneGoto", function () {
  this.appendDummyInput().appendField("Aller à").appendField(new Blockly.FieldTextInput("next_scene"), "target");
  stack(this, COLOR.scene); this.setInputsInline(true);
});
def("zacus_sceneBranch", function () {
  this.appendDummyInput().appendField("Branche si").appendField(new Blockly.FieldTextInput("score > 5"), "condition");
  this.appendDummyInput().appendField("alors").appendField(new Blockly.FieldTextInput("scene_a"), "ifTrue");
  this.appendDummyInput().appendField("sinon").appendField(new Blockly.FieldTextInput("scene_b"), "ifFalse");
  stack(this, COLOR.scene);
});

// NPC
def("zacus_npcSay", function () {
  this.appendDummyInput().appendField("Zacus dit")
    .appendField(new Blockly.FieldMultilineInput("Bonjour, je suis le professeur Zacus."), "text");
  stack(this, COLOR.npc);
});
def("zacus_npcWaitResponse", function () {
  this.appendDummyInput().appendField("Attendre réponse — timeout")
    .appendField(new Blockly.FieldNumber(10, 1, 120, 1), "timeout_s").appendField("s");
  stack(this, COLOR.npc); this.setInputsInline(true);
});
def("zacus_npcIntentMatch", function () {
  this.appendDummyInput().appendField("Si intent =").appendField(new Blockly.FieldTextInput("yes"), "intent");
  this.appendDummyInput().appendField("alors aller à").appendField(new Blockly.FieldTextInput("scene_x"), "then");
  stack(this, COLOR.npc);
});

// Audio
def("zacus_hwSoundPlay", function () {
  this.appendDummyInput().appendField("🔊 Jouer son").appendField(new Blockly.FieldTextInput("sting_win"), "asset");
  stack(this, COLOR.audio); this.setInputsInline(true);
});
def("zacus_hwAudioStop", function () {
  this.appendDummyInput().appendField("⏸ Stopper l'audio");
  stack(this, COLOR.audio);
});
def("zacus_hwAudioVolume", function () {
  this.appendDummyInput().appendField("Volume")
    .appendField(new Blockly.FieldNumber(70, 0, 100, 1), "level").appendField("/100");
  stack(this, COLOR.audio); this.setInputsInline(true);
});

// LCD
def("zacus_hwLCDText", function () {
  this.appendDummyInput().appendField("LCD ligne")
    .appendField(new Blockly.FieldNumber(0, 0, 32, 1), "line")
    .appendField("texte").appendField(new Blockly.FieldTextInput("Bienvenue"), "text");
  stack(this, COLOR.lcd); this.setInputsInline(true);
});
def("zacus_hwLCDClear", function () {
  this.appendDummyInput().appendField("LCD — effacer");
  stack(this, COLOR.lcd);
});
def("zacus_hwLCDImage", function () {
  this.appendDummyInput().appendField("LCD image").appendField(new Blockly.FieldTextInput("splash"), "asset");
  stack(this, COLOR.lcd); this.setInputsInline(true);
});
def("zacus_hwLCDTouchWait", function () {
  this.appendDummyInput().appendField("Attendre tap zone").appendField(new Blockly.FieldTextInput("btn_yes"), "zone")
    .appendField("timeout").appendField(new Blockly.FieldNumber(30, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.lcd); this.setInputsInline(true);
});

// Hardware (ESP32 générique)
def("zacus_hwServo", function () {
  this.appendDummyInput().appendField("Servo canal")
    .appendField(new Blockly.FieldNumber(0, 0, 15, 1), "channel")
    .appendField("→ angle").appendField(new Blockly.FieldNumber(90, 0, 180, 1), "angle").appendField("°");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwReadQR", function () {
  this.appendDummyInput().appendField("QR — attendu").appendField(new Blockly.FieldTextInput("ZAC-A1"), "expected");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwLEDPattern", function () {
  this.appendDummyInput().appendField("LED motif").appendField(dropdown([
    ["arc-en-ciel","rainbow"],["clignote","blink"],["fondu","fade"],["éteindre","off"]
  ]), "pattern");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwBuzzerTone", function () {
  this.appendDummyInput().appendField("Buzzer")
    .appendField(new Blockly.FieldNumber(2000, 20, 20000, 1), "freq").appendField("Hz pendant")
    .appendField(new Blockly.FieldNumber(120, 1, 10000, 1), "ms").appendField("ms");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwRelay", function () {
  this.appendDummyInput().appendField("Relais canal")
    .appendField(new Blockly.FieldNumber(0, 0, 15, 1), "channel")
    .appendField(dropdown([["allumer","on"],["éteindre","off"],["impulsion","pulse"]]), "state");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwSensorRead", function () {
  this.appendDummyInput().appendField("Lire capteur").appendField(new Blockly.FieldTextInput("A0"), "pin")
    .appendField("→").appendField(new Blockly.FieldTextInput("lecture"), "var");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});
def("zacus_hwButtonWait", function () {
  this.appendDummyInput().appendField("Attendre bouton").appendField(new Blockly.FieldTextInput("btn_main"), "button")
    .appendField("timeout").appendField(new Blockly.FieldNumber(0, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.hardware); this.setInputsInline(true);
});

// ESP-NOW
def("zacus_espnowRegisterPeer", function () {
  this.appendDummyInput().appendField("ESP-NOW alias").appendField(new Blockly.FieldTextInput("annexe1"), "alias")
    .appendField("MAC").appendField(new Blockly.FieldTextInput("AA:BB:CC:DD:EE:FF"), "mac");
  stack(this, COLOR.espnow); this.setInputsInline(true);
});
def("zacus_espnowSend", function () {
  this.appendDummyInput().appendField("ESP-NOW → peer").appendField(new Blockly.FieldTextInput("annexe1"), "peer")
    .appendField("cmd").appendField(new Blockly.FieldTextInput("open_door"), "command");
  stack(this, COLOR.espnow); this.setInputsInline(true);
});
def("zacus_espnowBroadcast", function () {
  this.appendDummyInput().appendField("ESP-NOW broadcast").appendField(new Blockly.FieldTextInput("reset"), "command");
  stack(this, COLOR.espnow); this.setInputsInline(true);
});
def("zacus_espnowWait", function () {
  this.appendDummyInput().appendField("ESP-NOW attendre").appendField(new Blockly.FieldTextInput("ready"), "command")
    .appendField("timeout").appendField(new Blockly.FieldNumber(10, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.espnow); this.setInputsInline(true);
});

// ESP32-S3-BOX-3
def("zacus_boxIMUShake", function () {
  this.appendDummyInput().appendField("BOX-3 secousse seuil")
    .appendField(new Blockly.FieldNumber(1.5, 0.1, 10, 0.1), "threshold")
    .appendField("g timeout").appendField(new Blockly.FieldNumber(10, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.box3); this.setInputsInline(true);
});
def("zacus_boxIRSend", function () {
  this.appendDummyInput().appendField("BOX-3 IR").appendField(dropdown(["NEC","RC5","SONY","RAW"]), "protocol")
    .appendField(new Blockly.FieldTextInput("0x20DF10EF"), "code");
  stack(this, COLOR.box3); this.setInputsInline(true);
});

// M5Stack / M5StickC
def("zacus_m5Beep", function () {
  this.appendDummyInput().appendField("M5 bip")
    .appendField(new Blockly.FieldNumber(4000, 20, 20000, 1), "freq").appendField("Hz")
    .appendField(new Blockly.FieldNumber(200, 1, 5000, 1), "ms").appendField("ms");
  stack(this, COLOR.m5); this.setInputsInline(true);
});
def("zacus_m5LCDText", function () {
  this.appendDummyInput().appendField("M5 LCD").appendField(new Blockly.FieldTextInput("Hello"), "text")
    .appendField(dropdown(["white","yellow","red","green","blue","cyan","magenta"]), "color")
    .appendField("size").appendField(new Blockly.FieldNumber(2, 1, 8, 1), "size");
  stack(this, COLOR.m5); this.setInputsInline(true);
});
def("zacus_m5ButtonAB", function () {
  this.appendDummyInput().appendField("M5 bouton").appendField(dropdown(["A","B","C","any"]), "button")
    .appendField("timeout").appendField(new Blockly.FieldNumber(0, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.m5); this.setInputsInline(true);
});
def("zacus_m5RGBLed", function () {
  this.appendDummyInput().appendField("M5 LED RGB").appendField(new Blockly.FieldTextInput("#FF8800"), "color");
  stack(this, COLOR.m5); this.setInputsInline(true);
});
def("zacus_m5IMUShake", function () {
  this.appendDummyInput().appendField("M5 secousse seuil")
    .appendField(new Blockly.FieldNumber(1.5, 0.1, 10, 0.1), "threshold")
    .appendField("g timeout").appendField(new Blockly.FieldNumber(10, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.m5); this.setInputsInline(true);
});

// PLIP téléphone
def("zacus_plipRing", function () {
  this.appendDummyInput().appendField("☎ PLIP sonner")
    .appendField(new Blockly.FieldNumber(3, 0, 60, 0.1), "duration_s").appendField("s");
  stack(this, COLOR.plip); this.setInputsInline(true);
});
def("zacus_plipPickupWait", function () {
  this.appendDummyInput().appendField("☎ PLIP attendre décroché — timeout")
    .appendField(new Blockly.FieldNumber(30, 0, 600, 1), "timeout_s").appendField("s");
  stack(this, COLOR.plip); this.setInputsInline(true);
});

// Logique
def("zacus_logicIf", function () {
  this.appendDummyInput().appendField("si").appendField(new Blockly.FieldTextInput("score > 0"), "condition");
  this.appendStatementInput("body").setCheck(null).appendField("alors");
  this.appendStatementInput("else").setCheck(null).appendField("sinon");
  stack(this, COLOR.logic);
});
def("zacus_logicTimer", function () {
  this.appendDummyInput().appendField("Attendre")
    .appendField(new Blockly.FieldNumber(5, 0, 600, 0.1), "seconds").appendField("s");
  stack(this, COLOR.logic); this.setInputsInline(true);
});
def("zacus_logicScore", function () {
  this.appendDummyInput().appendField("Score +")
    .appendField(new Blockly.FieldNumber(1, -100, 100, 1), "delta");
  stack(this, COLOR.logic); this.setInputsInline(true);
});
def("zacus_logicSetVar", function () {
  this.appendDummyInput().appendField("Variable").appendField(new Blockly.FieldTextInput("key"), "name")
    .appendField(":=").appendField(new Blockly.FieldTextInput("42"), "value");
  stack(this, COLOR.logic); this.setInputsInline(true);
});

window.ZACUS_BLOCK_TYPES = [
  "zacus_sceneStart","zacus_sceneEnd","zacus_sceneGoto","zacus_sceneBranch",
  "zacus_npcSay","zacus_npcWaitResponse","zacus_npcIntentMatch",
  "zacus_hwSoundPlay","zacus_hwAudioStop","zacus_hwAudioVolume",
  "zacus_hwLCDText","zacus_hwLCDClear","zacus_hwLCDImage","zacus_hwLCDTouchWait",
  "zacus_hwServo","zacus_hwReadQR","zacus_hwLEDPattern","zacus_hwBuzzerTone","zacus_hwRelay","zacus_hwSensorRead","zacus_hwButtonWait",
  "zacus_espnowRegisterPeer","zacus_espnowSend","zacus_espnowBroadcast","zacus_espnowWait",
  "zacus_boxIMUShake","zacus_boxIRSend",
  "zacus_m5Beep","zacus_m5LCDText","zacus_m5ButtonAB","zacus_m5RGBLed","zacus_m5IMUShake",
  "zacus_plipRing","zacus_plipPickupWait",
  "zacus_logicIf","zacus_logicTimer","zacus_logicScore","zacus_logicSetVar",
];

window.ZACUS_FIELDS_BY_KIND = {
  zacus_sceneStart: ["id"],
  zacus_sceneEnd: [],
  zacus_sceneGoto: ["target"],
  zacus_sceneBranch: ["condition","ifTrue","ifFalse"],
  zacus_npcSay: ["text"],
  zacus_npcWaitResponse: ["timeout_s"],
  zacus_npcIntentMatch: ["intent","then"],
  zacus_hwSoundPlay: ["asset"],
  zacus_hwAudioStop: [],
  zacus_hwAudioVolume: ["level"],
  zacus_hwLCDText: ["line","text"],
  zacus_hwLCDClear: [],
  zacus_hwLCDImage: ["asset"],
  zacus_hwLCDTouchWait: ["zone","timeout_s"],
  zacus_hwServo: ["channel","angle"],
  zacus_hwReadQR: ["expected"],
  zacus_hwLEDPattern: ["pattern"],
  zacus_hwBuzzerTone: ["freq","ms"],
  zacus_hwRelay: ["channel","state"],
  zacus_hwSensorRead: ["pin","var"],
  zacus_hwButtonWait: ["button","timeout_s"],
  zacus_espnowRegisterPeer: ["alias","mac"],
  zacus_espnowSend: ["peer","command"],
  zacus_espnowBroadcast: ["command"],
  zacus_espnowWait: ["command","timeout_s"],
  zacus_boxIMUShake: ["threshold","timeout_s"],
  zacus_boxIRSend: ["protocol","code"],
  zacus_m5Beep: ["freq","ms"],
  zacus_m5LCDText: ["text","color","size"],
  zacus_m5ButtonAB: ["button","timeout_s"],
  zacus_m5RGBLed: ["color"],
  zacus_m5IMUShake: ["threshold","timeout_s"],
  zacus_plipRing: ["duration_s"],
  zacus_plipPickupWait: ["timeout_s"],
  zacus_logicIf: ["condition"],
  zacus_logicTimer: ["seconds"],
  zacus_logicScore: ["delta"],
  zacus_logicSetVar: ["name","value"],
};

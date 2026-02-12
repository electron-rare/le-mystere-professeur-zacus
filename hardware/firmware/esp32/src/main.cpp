#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// -------------------- ESP32 DevKit GPIO --------------------
// RGB LED externe (3 sorties digitales)
static constexpr uint8_t PIN_LED_R = 16;
static constexpr uint8_t PIN_LED_G = 17;
static constexpr uint8_t PIN_LED_B = 4;

// Micro analogique
static constexpr uint8_t PIN_MIC = 34;

// DAC interne pour sinus
static constexpr uint8_t PIN_DAC_SINE = 25;

// Carte SD (bus VSPI standard ESP32 DevKit)
static constexpr uint8_t PIN_SD_CS = 5;
static constexpr uint8_t PIN_SD_SCK = 18;
static constexpr uint8_t PIN_SD_MISO = 19;
static constexpr uint8_t PIN_SD_MOSI = 23;

// I2S sortie MP3 (vers DAC/ampli I2S externe type MAX98357A)
static constexpr uint8_t PIN_I2S_BCLK = 26;
static constexpr uint8_t PIN_I2S_LRC = 27;
static constexpr uint8_t PIN_I2S_DOUT = 22;

static constexpr char MP3_PATH[] = "/track001.mp3";

// -------------------- MP3 --------------------
AudioGeneratorMP3* mp3 = nullptr;
AudioFileSourceSD* mp3File = nullptr;
AudioOutputI2S* i2sOut = nullptr;

// -------------------- DAC sinus 440 Hz --------------------
static constexpr float SINE_FREQ_HZ = 440.0f;
static constexpr uint16_t DAC_SAMPLE_RATE = 22050;
static constexpr uint16_t SINE_TABLE_SIZE = 128;
uint8_t sineTable[SINE_TABLE_SIZE];
uint32_t lastDacMicros = 0;
uint32_t dacPeriodUs = 1000000UL / DAC_SAMPLE_RATE;

// -------------------- Détection LA (Goertzel) --------------------
static constexpr float DETECT_FS = 4000.0f;
static constexpr uint16_t DETECT_N = 128;
static constexpr float DETECT_TARGET_HZ = 440.0f;
static constexpr float DETECT_RATIO_THRESHOLD = 1.8f;
static constexpr uint16_t DETECT_EVERY_MS = 100;

bool laDetected = false;
uint32_t lastDetectMs = 0;

// -------------------- LED random --------------------
uint32_t nextLedUpdateMs = 0;

void setLed(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

void updateLedRandom(uint32_t nowMs) {
  if (nowMs < nextLedUpdateMs) {
    return;
  }

  // Pas de vert seul pour garder la signification "LA détecté"
  const uint8_t mode = random(0, 5);
  switch (mode) {
    case 0:
      setLed(true, false, false);
      break;  // rouge
    case 1:
      setLed(false, false, true);
      break;  // bleu
    case 2:
      setLed(true, false, true);
      break;  // violet
    case 3:
      setLed(true, true, false);
      break;  // jaune
    default:
      setLed(false, false, false);
      break;  // off
  }

  nextLedUpdateMs = nowMs + random(120, 500);
}

void buildSineTable() {
  for (uint16_t i = 0; i < SINE_TABLE_SIZE; ++i) {
    const float phase = 2.0f * PI * static_cast<float>(i) / static_cast<float>(SINE_TABLE_SIZE);
    const float normalized = 0.5f + 0.5f * sinf(phase);
    sineTable[i] = static_cast<uint8_t>(normalized * 255.0f);
  }
}

void updateDacSine() {
  const uint32_t nowUs = micros();
  if ((nowUs - lastDacMicros) < dacPeriodUs) {
    return;
  }

  lastDacMicros = nowUs;

  static float phaseAcc = 0.0f;
  const float step = (SINE_FREQ_HZ * static_cast<float>(SINE_TABLE_SIZE)) /
                     static_cast<float>(DAC_SAMPLE_RATE);
  phaseAcc += step;
  if (phaseAcc >= SINE_TABLE_SIZE) {
    phaseAcc -= SINE_TABLE_SIZE;
  }

  const uint8_t sample = sineTable[static_cast<uint16_t>(phaseAcc)];
  dacWrite(PIN_DAC_SINE, sample);
}

float goertzelPower(const int16_t* x, uint16_t n, float fs, float targetHz) {
  const float k = roundf((static_cast<float>(n) * targetHz) / fs);
  const float omega = (2.0f * PI * k) / static_cast<float>(n);
  const float coeff = 2.0f * cosf(omega);

  float s0 = 0.0f;
  float s1 = 0.0f;
  float s2 = 0.0f;

  for (uint16_t i = 0; i < n; ++i) {
    s0 = x[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }

  return (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
}

bool detectLA440() {
  static int16_t samples[DETECT_N];

  int32_t meanAccum = 0;
  for (uint16_t i = 0; i < DETECT_N; ++i) {
    const int16_t v = static_cast<int16_t>(analogRead(PIN_MIC));
    samples[i] = v;
    meanAccum += v;
    delayMicroseconds(static_cast<uint32_t>(1000000.0f / DETECT_FS));
  }

  const float mean = static_cast<float>(meanAccum) / static_cast<float>(DETECT_N);
  float totalEnergy = 0.0f;

  for (uint16_t i = 0; i < DETECT_N; ++i) {
    const int16_t centered = static_cast<int16_t>(samples[i] - mean);
    samples[i] = centered;
    totalEnergy += static_cast<float>(centered) * static_cast<float>(centered);
  }

  if (totalEnergy < 1.0f) {
    return false;
  }

  const float targetEnergy = goertzelPower(samples, DETECT_N, DETECT_FS, DETECT_TARGET_HZ);
  const float ratio = targetEnergy / (totalEnergy + 1.0f);
  return ratio > DETECT_RATIO_THRESHOLD;
}

void startMp3() {
  if (!SD.exists(MP3_PATH)) {
    Serial.printf("[MP3] Fichier absent: %s\n", MP3_PATH);
    return;
  }

  mp3File = new AudioFileSourceSD(MP3_PATH);
  i2sOut = new AudioOutputI2S();
  i2sOut->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  i2sOut->SetGain(0.20f);

  mp3 = new AudioGeneratorMP3();
  mp3->begin(mp3File, i2sOut);
  Serial.println("[MP3] Lecture en cours.");
}

void initSdAndMp3() {
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("[MP3] Carte SD non détectée.");
    return;
  }

  startMp3();
}

void loopMp3() {
  if (mp3 == nullptr) {
    return;
  }

  if (mp3->isRunning()) {
    mp3->loop();
    return;
  }

  mp3->stop();
  delete mp3;
  delete mp3File;
  mp3 = nullptr;
  mp3File = nullptr;

  startMp3();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLed(false, false, false);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MIC, ADC_11db);

  randomSeed(analogRead(PIN_MIC));

  buildSineTable();
  initSdAndMp3();

  Serial.println("[BOOT] U-SON / ESP32 DevKit prêt.");
}

void loop() {
  const uint32_t nowMs = millis();

  updateDacSine();
  loopMp3();

  if ((nowMs - lastDetectMs) >= DETECT_EVERY_MS) {
    lastDetectMs = nowMs;
    laDetected = detectLA440();
  }

  if (laDetected) {
    setLed(false, true, false);
  } else {
    updateLedRandom(nowMs);
  }
}

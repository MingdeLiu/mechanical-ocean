
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <math.h>
#include "sea_data.h"
// ========================================
// PCA9685
// ========================================

Adafruit_PWMServoDriver pca1(0x40);
Adafruit_PWMServoDriver pca2(0x41);

const uint8_t OE_PIN = 8;
const uint8_t SERVO_COUNT = 25;

// ========================================
// SERVO LIMITS
// ========================================

const int TOP_US = 450;
const int MID_US = 1450;
const int BOTTOM_US = 2300;
const int SERVO_OFFSET_US[SERVO_COUNT] = {
  0, -60, -40, 55, -70,
  0, 80, 80, 30, 90,
  -50, 30, 30, 50, 0,
  50, 20, 20, 10, -10,
  -10, -10, 40, 30, 0
};

// ========================================
// NORMAL OCEAN WAVE
// ========================================

const float AMPLITUDE_US = 470.0f;
const float PERIOD_MS = 3000.0f;

const float SEA_PLAY_MS = 600000.0f;
const float SEA_LOOP_BLEND_MS = 20000.0f;
// Selected hours in the same Shenzhen record: 23 Oct 12:00 to 25 Oct 08:00 UTC.
const int SEA_DEMO_FIRST = 36;
const int SEA_DEMO_LAST = 80;
const float SEA_DEMO_PLAY_MS = 38000.0f;
const float SEA_DEMO_BLEND_MS = 6000.0f;
const float SEA_DEMO_AMP_MIN_US = 210.0f;
const float SEA_DEMO_PHASE_SPREAD = 1.35f;
const float SEA_AMP_MIN_US = 120.0f;
const float SEA_AMP_MAX_US = 300.0f;
const float SEA_FAST_PERIOD_MS = 2800.0f;
const float SEA_SLOW_PERIOD_MS = 5500.0f;
float seaClockMs = 0;
float seaDemoClockMs = 0;
bool seaDemoMode = false;
float seaAmplitudeUs = SEA_AMP_MIN_US;
float seaPeriodMs = SEA_SLOW_PERIOD_MS;
float seaPhaseSpread = 1.0f;
float seaTravelRad = 0;
float seaHeightM = 0;
float seaPeriodSeconds = 0;
float seaFromDegrees = 0;
float seaDisplayFromDegrees = SEA_DATA[0].direction;

float circularLerp(float a, float b, float t) {
  float delta = fmodf(b - a + 540.0f, 360.0f) - 180.0f;
  return fmodf(a + delta * t + 360.0f, 360.0f);
}

void updateSea(float dt, bool playing) {
  float &clockMs = seaDemoMode ? seaDemoClockMs : seaClockMs;
  float playMs = seaDemoMode ? SEA_DEMO_PLAY_MS : SEA_PLAY_MS;
  float blendMs = seaDemoMode ? SEA_DEMO_BLEND_MS : SEA_LOOP_BLEND_MS;
  int first = seaDemoMode ? SEA_DEMO_FIRST : 0;
  int last = seaDemoMode ? SEA_DEMO_LAST : SEA_COUNT - 1;
  if (playing) clockMs = fmodf(clockMs + dt * 1000.0f, playMs + blendMs);
  int a, b;
  float t;
  if (clockMs < playMs) {
    float index = first + clockMs / playMs * (last - first);
    a = (int)index;
    b = a < last ? a + 1 : a;
    t = index - a;
  } else {
    a = last;
    b = first;
    t = (clockMs - playMs) / blendMs;
  }
  t = t * t * (3.0f - 2.0f * t);
  seaHeightM = SEA_DATA[a].height + (SEA_DATA[b].height - SEA_DATA[a].height) * t;
  seaPeriodSeconds = SEA_DATA[a].period + (SEA_DATA[b].period - SEA_DATA[a].period) * t;
  seaFromDegrees = circularLerp(SEA_DATA[a].direction, SEA_DATA[b].direction, t);
  float heightScale = constrain((seaHeightM - SEA_HEIGHT_MIN) /
                     (SEA_HEIGHT_MAX - SEA_HEIGHT_MIN), 0.0f, 1.0f);
  float periodScale = constrain((seaPeriodSeconds - SEA_PERIOD_MIN) /
                     (SEA_PERIOD_MAX - SEA_PERIOD_MIN), 0.0f, 1.0f);
  float minAmplitude = seaDemoMode ? SEA_DEMO_AMP_MIN_US : SEA_AMP_MIN_US;
  float amplitude = minAmplitude + heightScale * (SEA_AMP_MAX_US - minAmplitude);
  float period = SEA_FAST_PERIOD_MS + periodScale * (SEA_SLOW_PERIOD_MS - SEA_FAST_PERIOD_MS);
  float phaseSpread = seaDemoMode ? SEA_DEMO_PHASE_SPREAD : 1.0f;
  float ease = 1.0f - expf(-dt / 1.0f);
  seaAmplitudeUs += (amplitude - seaAmplitudeUs) * ease;
  seaPeriodMs += (period - seaPeriodMs) * ease;
  seaPhaseSpread += (phaseSpread - seaPhaseSpread) * ease;
  seaDisplayFromDegrees = circularLerp(seaDisplayFromDegrees, seaFromDegrees,
                                      1.0f - expf(-dt / 0.55f));
  seaTravelRad = fmodf(seaDisplayFromDegrees + 180.0f, 360.0f) * PI / 180.0f;
}

void reportSea() {
  Serial.println("Shenzhen offshore / MFWAM via Open-Meteo / model data, not observations");
  if (seaDemoMode) {
    Serial.println("Exhibition: 2026-10-23 12:00 to 2026-10-25 08:00 UTC; 38s playback + 6s loop blend");
  } else {
    Serial.println("2026-10-22 00:00 to 2026-10-28 23:00 UTC; 600s playback + 20s loop blend");
  }
  Serial.print("Height m: "); Serial.print(seaHeightM);
  Serial.print(" Mean period s: "); Serial.print(seaPeriodSeconds);
  Serial.print(" Source from degrees: "); Serial.print(seaFromDegrees);
  Serial.print(" Display from degrees: "); Serial.println(seaDisplayFromDegrees);
  Serial.print("Amplitude us: "); Serial.print(seaAmplitudeUs);
  Serial.print(" Motion period ms: "); Serial.println(seaPeriodMs);
}


// ========================================
// BLOW WAVE SPEED
// ========================================

// NEW:
// Near side = 6x normal speed
// Far side  = 3x normal speed

const float NEAR_SPEED_MULTIPLIER = 6.0f;
const float BLOW_SPEED_MULTIPLIER = 3.0f;

// ========================================
// MOVEMENT SETTINGS
// ========================================

const unsigned long UPDATE_MS = 20;
const unsigned long CENTER_INTERVAL_MS = 1200;
const bool AUTO_START_EXHIBITION = true;
const unsigned long POWER_SETTLE_MS = 2500;
const unsigned long PCA_RETRY_MS = 1500;

// Normal movement remains unchanged
const float NORMAL_STEP_US = 25.0f;

// NEW: faster blow response
// Previous: 40 us / 20 ms
// New:      70 us / 20 ms

const float BLOW_STEP_US = 70.0f;

// Timeout return remains unchanged
const float RELEASE_STEP_US = 4.0f;

// ========================================
// SERVO STATE
// ========================================

float positionUs[SERVO_COUNT];
bool enabled[SERVO_COUNT];

bool hardwareReady = false;
bool waveAllowed = false;
bool autoStartPending = AUTO_START_EXHIBITION;

char mode = 's';
char lastBlowDirection = 'r';

int selected = 0;
int nextCenter = 0;

unsigned long lastTick = 0;
unsigned long modeStart = 0;
unsigned long nextCenterTime = 0;
unsigned long nextPcaCheck = 0;

String inputLine;

// ========================================
// BLOW SETTINGS
// ========================================

unsigned long lastBlowPacket = 0;
const unsigned long BLOW_TIMEOUT_MS = 700;

// Near: 2150 - 2250 us
const float NEAR_BLOW_US = 2200.0f;
const float NEAR_WAVE_US = 80.0f;

// Far: 450 - 650 us
const float FAR_BLOW_US = 550.0f;
const float FAR_WAVE_US = 100.0f;

// ========================================
// CUSTOM ROW HEIGHTS
// ========================================

// Distance 0 = near side
// Distance 4 = far side
//
// Row 1: 2200 us
// Row 2: 1958 us
// Row 3: 1375 us
// Row 4: 1000 us
// Row 5: 550 us

const float BLOW_ROW_HEIGHT[5] = {
  2200.0f,
  1958.0f,
  1375.0f,
  1000.0f,
  550.0f
};

// ========================================
// PROPAGATION AND SMOOTHING
// ========================================

// Keep 150 ms per row
const float PUSH_DELAY_MS = 65.0f;

// Keep original blow smoothing
const float BLOW_TAU_NEAR_S = 0.04f;
const float BLOW_TAU_ROW_S = 0.06f;

// Keep release smoothing
const float RELEASE_TAU_S = 0.45f;

// ========================================
// WAVE STATE
// ========================================

float waveFade = 0.0f;

float bandBias[5] = {
  0, 0, 0, 0, 0
};

float bandBlowMix[5] = {
  0, 0, 0, 0, 0
};

// Separate phase for each row
float normalPhase[3] = {0, 0, 0};
float blowPhase = 0;
const float BLOW_PERIOD_MS = 450.0f;
const float NORMAL_CENTER_US = MID_US + 450.0f;

// ========================================
// SMOOTHSTEP
// ========================================


const uint8_t SOUND_PINS[2] = {A0, A1};
const unsigned long SOUND_WINDOW_MS = 40;
const unsigned long SOUND_CALIBRATE_MS = 8000;
const unsigned long SOUND_GUST_MS = 1800;
const float SOUND_DIRECTION_RATIO = 1.35f;
const unsigned long SOUND_RECOVERY_MS = 3600;
const unsigned long SOUND_QUIET_MS = 600;
// 0: waiting for quiet, 1: armed, 2: gust, 3: recovery.
uint8_t soundStage = 0;
unsigned long soundStageStart = 0;
unsigned long soundQuietStart = 0;
bool soundQuietTiming = false;
float soundThreshold = 40.0f;
bool soundEnabled = false;
bool soundCalibrating = false;
bool soundMonitor = false;
unsigned long soundCalibrationStart = 0;
unsigned long soundWindowStart = 0;
unsigned long soundLastSample = 0;
unsigned long soundLastReport = 0;
uint16_t soundMin[2] = {1023, 1023};
uint16_t soundMax[2] = {0, 0};
uint32_t soundSum[2] = {0, 0};
uint32_t soundSamples = 0;
float soundNoiseSpan[2] = {0, 0};
float soundNoiseMean[2] = {0, 0};
float soundMeanMin[2] = {1023, 1023};
float soundMeanMax[2] = {0, 0};
float soundScore[2] = {0, 0};
float soundRawMean[2] = {0, 0};
float soundRawSpan[2] = {0, 0};
uint32_t soundCalibrationWindows = 0;
char soundCandidate = 0;
uint8_t soundCandidateCount = 0;

void resetSoundWindow() {
  for (int i = 0; i < 2; ++i) {
    soundMin[i] = 1023;
    soundMax[i] = 0;
    soundSum[i] = 0;
  }
  soundSamples = 0;
  soundWindowStart = millis();
}

void stopSoundControl() {
  soundEnabled = false;
  soundCalibrating = false;
  soundCandidate = 0;
  soundCandidateCount = 0;
  soundStage = 0;
  soundQuietTiming = false;
}

void startSoundCalibration() {
  stopSoundControl();
  soundCalibrationWindows = 0;
  for (int i = 0; i < 2; ++i) {
    soundNoiseSpan[i] = 0;
    soundNoiseMean[i] = 0;
    soundMeanMin[i] = 1023;
    soundMeanMax[i] = 0;
  }
  mode = 'w';
  modeStart = millis();
  soundCalibrationStart = millis();
  soundCalibrating = true;
  resetSoundWindow();
  Serial.println("Calibrating for 8 seconds with waves running. Do NOT blow or speak.");
}

void reportSound() {
  Serial.print("Left="); Serial.print(soundScore[0], 1);
  Serial.print(" Right="); Serial.print(soundScore[1], 1);
  Serial.print(" Threshold="); Serial.print(soundThreshold, 1);
  Serial.print(" MeanL/R="); Serial.print(soundRawMean[0], 1);
  Serial.print('/'); Serial.print(soundRawMean[1], 1);
  Serial.print(" SpanL/R="); Serial.print(soundRawSpan[0], 1);
  Serial.print('/'); Serial.print(soundRawSpan[1], 1);
  Serial.print(" Sound=");
  Serial.print(soundCalibrating ? "CAL" : (soundEnabled ? "ON" : "OFF"));
  Serial.print(" Mode="); Serial.println(mode);
  Serial.print("Trigger stage (0 quiet / 1 armed / 2 gust / 3 recovery)=");
  Serial.println(soundStage);
}

void updateSound() {
  unsigned long now = millis();
  unsigned long sampleTime = micros();
  if (sampleTime - soundLastSample >= 250) {
    soundLastSample = sampleTime;
    for (int i = 0; i < 2; ++i) {
      int value = analogRead(SOUND_PINS[i]);
      if (value < soundMin[i]) soundMin[i] = value;
      if (value > soundMax[i]) soundMax[i] = value;
      soundSum[i] += value;
    }
    ++soundSamples;
  }
  if (now - soundWindowStart < SOUND_WINDOW_MS || soundSamples == 0) return;

  for (int i = 0; i < 2; ++i) {
    soundRawMean[i] = (float)soundSum[i] / soundSamples;
    soundRawSpan[i] = soundMax[i] - soundMin[i];
  }

  if (soundCalibrating) {
    // Allow wave startup to settle before collecting the noise reference.
    if (now - soundCalibrationStart >= 1500) {
      ++soundCalibrationWindows;
      for (int i = 0; i < 2; ++i) {
        soundNoiseSpan[i] = fmaxf(soundNoiseSpan[i], soundRawSpan[i]);
        soundNoiseMean[i] += (soundRawMean[i] - soundNoiseMean[i]) / soundCalibrationWindows;
        soundMeanMin[i] = fminf(soundMeanMin[i], soundRawMean[i]);
        soundMeanMax[i] = fmaxf(soundMeanMax[i], soundRawMean[i]);
      }
    }
    if (now - soundCalibrationStart >= SOUND_CALIBRATE_MS && soundCalibrationWindows > 0) {
      soundCalibrating = false;
      soundEnabled = true;
      soundStage = 0;
      soundQuietTiming = false;
      Serial.println("Calibration done. Waiting for quiet. A0: LEFT R5 -> R1. A1: RIGHT R1 -> R5.");
      Serial.println("Blow near one microphone. v: readings; t10/t20/t40: threshold; s: hold; x: off.");
    }
  } else {
    for (int i = 0; i < 2; ++i) {
      float meanTolerance = soundMeanMax[i] - soundMeanMin[i];
      soundScore[i] = fmaxf(0.0f, fmaxf(
        soundRawSpan[i] - soundNoiseSpan[i],
        fabsf(soundRawMean[i] - soundNoiseMean[i]) - meanTolerance));
    }
    if (soundEnabled && waveAllowed) {
      bool quiet = soundScore[0] < soundThreshold * 0.60f &&
                   soundScore[1] < soundThreshold * 0.60f;
      if (soundStage == 2) {
        // Ignore sound during the gust so motor noise cannot extend it.
        lastBlowPacket = now;
        if (now - soundStageStart >= SOUND_GUST_MS) {
          mode = 'b';
          soundStage = 3;
          soundStageStart = now;
          Serial.println("Returning to normal waves. Sound triggers temporarily locked.");
        }
      } else if (soundStage == 3) {
        if (now - soundStageStart >= SOUND_RECOVERY_MS) {
          soundStage = 0;
          soundQuietTiming = false;
        }
      } else if (soundStage == 0) {
        if (!quiet) soundQuietTiming = false;
        else {
          if (!soundQuietTiming) {
            soundQuietTiming = true;
            soundQuietStart = now;
          }
          if (now - soundQuietStart >= SOUND_QUIET_MS) {
            soundStage = 1;
            soundCandidate = 0;
            soundCandidateCount = 0;
            Serial.println("Sound READY - blow again.");
          }
        }
      } else {
      char candidate = 0;
      float ratio = SOUND_DIRECTION_RATIO;
      float threshold = soundThreshold;
      if (soundScore[0] > threshold && soundScore[0] > soundScore[1] * ratio) candidate = 'l';
      if (soundScore[1] > threshold && soundScore[1] > soundScore[0] * ratio) candidate = 'r';
      if (candidate != 0 && candidate == soundCandidate) {
        if (soundCandidateCount < 10) ++soundCandidateCount;
      } else {
        soundCandidate = candidate;
        soundCandidateCount = candidate ? 1 : 0;
      }
      if (candidate != 0 && soundCandidateCount >= 2) {
          soundStage = 2;
          soundStageStart = now;
          lastBlowPacket = now;
          mode = candidate;
          lastBlowDirection = candidate;
          modeStart = now;
          Serial.println(candidate == 'l' ? "SOUND LEFT" : "SOUND RIGHT");
      }
      }
    }
  }
  resetSoundWindow();
  if (soundMonitor && now - soundLastReport >= 300) {
    soundLastReport = now;
    reportSound();
  }
}

float smoothstep(float t) {

  t = constrain(t, 0.0f, 1.0f);

  return t * t * (3.0f - 2.0f * t);
}

// ========================================
// BLOW TARGET HEIGHT
// ========================================

float blowTargetUs(int distance) {

  distance = constrain(distance, 0, 4);

  return BLOW_ROW_HEIGHT[distance];
}

// ========================================
// BLOW WAVE AMPLITUDE
// ========================================

// Near: +/- 50 us
// Far:  +/- 100 us

float blowWaveAmplitude(int distance) {

  float t = smoothstep(
    (float)distance / 4.0f
  );

  return NEAR_WAVE_US +
    (FAR_WAVE_US - NEAR_WAVE_US) * t;
}

// ========================================
// BLOW WAVE SPEED
// ========================================

// NEW SPEED PROFILE:
//
// Row 1: 6.0000x
// Row 2: 5.53125x
// Row 3: 4.5000x
// Row 4: 3.46875x
// Row 5: 3.0000x

float blowWaveSpeed(int distance) {

  float t = smoothstep(
    (float)distance / 4.0f
  );

  return NEAR_SPEED_MULTIPLIER +
    (
      BLOW_SPEED_MULTIPLIER -
      NEAR_SPEED_MULTIPLIER
    ) * t;
}

// ========================================
// SERVO OUTPUT
// ========================================

// Unit 0-12 -> PCA1 CH0-12
// Unit 13-24 -> PCA2 CH0-11

void writeServo(int index, float us) {

  us += SERVO_OFFSET_US[index];

  us = constrain(
    us,
    (float)TOP_US,
    (float)BOTTOM_US
  );

  uint16_t pulse = (uint16_t)lroundf(us);

  if (index < 13) {

    pca1.writeMicroseconds(index, pulse);

  } else {

    pca2.writeMicroseconds(index - 13, pulse);

  }
}

// ========================================
// DISABLE ALL
// ========================================

void disableAll() {
  stopSoundControl();

  digitalWrite(OE_PIN, HIGH);

  for (int ch = 0; ch < 16; ch++) {

    pca1.setPWM(ch, 0, 4096);
    pca2.setPWM(ch, 0, 4096);

  }

  for (int i = 0; i < SERVO_COUNT; i++) {

    enabled[i] = false;

  }

  waveAllowed = false;
  mode = 's';
}

// ========================================
// ENABLE SERVO
// ========================================

void enableServo(int index) {

  if (!enabled[index]) {

    positionUs[index] = MID_US;

    writeServo(index, MID_US);

    enabled[index] = true;

  }

  digitalWrite(OE_PIN, LOW);
}

void startCentering() {
  stopSoundControl();
  waveAllowed = false;
  mode = 'm';
  nextCenter = 0;
  nextCenterTime = millis();
  Serial.println("Centering 25 units...");
}

// ========================================
// DEVICE CHECK
// ========================================

bool devicePresent(uint8_t address) {

  Wire.beginTransmission(address);

  return Wire.endTransmission() == 0;
}

// ========================================
// PRINT POSITION
// ========================================

void printPosition(int index) {

  Serial.print("Unit ");
  Serial.print(index + 1);

  Serial.print(" = R");
  Serial.print(index / 5 + 1);

  Serial.print("C");
  Serial.print(index % 5 + 1);

  Serial.print(" / PCA");
  Serial.print(index < 13 ? 1 : 2);

  Serial.print(" CH");
  Serial.println(index < 13 ? index : index - 13);
}

// ========================================
// SERIAL COMMANDS
// ========================================

void handleCommand(String cmd) {

  cmd.trim();
  cmd.toLowerCase();

  if (cmd.length() == 0) return;
  if (cmd == "s" || cmd == "x") autoStartPending = false;

  if (!hardwareReady) {

    Serial.println("PCA not ready.");
    return;
  }


  if (cmd == "p") { reportSea(); return; }
  if (cmd == "v") {
    soundMonitor = !soundMonitor;
    reportSound();
    return;
  }
  if (cmd.startsWith("t") && cmd.length() > 1) {
    bool valid = true;
    for (unsigned int i = 1; i < cmd.length(); ++i) {
      if (cmd[i] < '0' || cmd[i] > '9') valid = false;
    }
    int value = cmd.substring(1).toInt();
    if (valid && value >= 5 && value <= 500) {
      soundThreshold = value;
      Serial.print("Sound threshold = "); Serial.println(soundThreshold);
    } else Serial.println("Use t5 through t500.");
    return;
  }
  if (cmd == "a" || cmd == "c") {
    if (!waveAllowed || mode == 'm' || mode == 't') {
      Serial.println("Send m first and wait for Centered.");
      return;
    }
    startSoundCalibration();
    return;
  }
  bool manualNumber = cmd.length() > 0;
  for (unsigned int i = 0; i < cmd.length(); ++i) {
    if (cmd[i] < '0' || cmd[i] > '9') manualNumber = false;
  }
  if (cmd == "x" || cmd == "s" || cmd == "w" ||
      cmd == "l" || cmd == "r" || cmd == "b" ||
      (manualNumber && cmd.toInt() >= 1 && cmd.toInt() <= 25)) {
    stopSoundControl();
  }

  // ========================================
  // DISABLE OUTPUTS
  // ========================================


  if (cmd == "d") {
    autoStartPending = false;
    stopSoundControl();
    waveAllowed = false;
    mode = 'd';
    nextCenter = 0;
    nextCenterTime = millis();
    Serial.println("Lowering all units to calibrated bottom positions...");
    return;
  }

  if (cmd == "x") {

    disableAll();

    Serial.println("Signals OFF.");

    return;
  }

  // ========================================
  // HOLD POSITION
  // ========================================

  if (cmd == "s") {

    mode = 's';

    Serial.println("Holding positions.");

    return;
  }

  // ========================================
  // CENTER ALL SERVOS
  // ========================================

  if (cmd == "m") {
    autoStartPending = false;
    startCentering();
    return;
  }

  // ========================================
  // RELEASE BLOW
  // ========================================

  if (cmd == "b") {

    if (waveAllowed) {

      mode = 'b';

      Serial.println("Releasing blow.");

    }

    return;
  }

  // ========================================
  // LEFT / RIGHT BLOW
  // ========================================

  if (cmd == "l" || cmd == "r") {

    if (!waveAllowed) {

      Serial.println("Send m first.");

      return;
    }

    lastBlowPacket = millis();

    if (mode != cmd[0]) {

      mode = cmd[0];
      lastBlowDirection = mode;
      modeStart = millis();

      if (mode == 'l') {

        Serial.println(
          "BLOW LEFT R5 -> R1"
        );

      } else {

        Serial.println(
          "BLOW RIGHT R1 -> R5"
        );

      }

      Serial.println(
        "Fast blow ripples, 65 ms propagation per band"
      );

      Serial.println(
        "Fast blow response: 70 us / 20 ms"
      );
    }

    return;
  }

  // ========================================
  // NORMAL OCEAN WAVE
  // ========================================

  if (cmd == "w" || cmd == "f") {

    if (!waveAllowed) {

      Serial.println("Send m first.");

      return;
    }

    seaDemoMode = cmd == "f";
    if (seaDemoMode) seaDemoClockMs = 0.0f;
    if (!seaDemoMode || (mode != 'l' && mode != 'r' && mode != 'b')) {
      mode = 'w';
      modeStart = millis();
    }
    Serial.println(seaDemoMode ? "SHENZHEN EXHIBITION LOOP: 38s playback, 6s blend"
                               : "SHENZHEN DATA WAVE");

    return;
  }

  // ========================================
  // INDIVIDUAL SERVO TEST
  // ========================================

  bool isNumber = true;

  for (unsigned int i = 0; i < cmd.length(); i++) {

    if (cmd[i] < '0' || cmd[i] > '9') {

      isNumber = false;
      break;
    }
  }

  if (isNumber) {

    int number = cmd.toInt();

    if (number >= 1 && number <= 25) {

      autoStartPending = false;
      selected = number - 1;

      enableServo(selected);

      waveAllowed = false;
      mode = 't';
      modeStart = millis();

      printPosition(selected);

      Serial.println(
        "Individual test started."
      );

      return;
    }
  }

  Serial.println(
    "Commands: 1-25, m, w, f, L, R, b, s, x"
  );
}

// ========================================
// SETUP
// ========================================

void setup() {

  digitalWrite(OE_PIN, HIGH);
  pinMode(OE_PIN, OUTPUT);

  Serial.begin(115200);
  analogReadResolution(10);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  resetSoundWindow();

  Wire.begin();
  Wire.setClock(100000);

  for (int i = 0; i < SERVO_COUNT; i++) {

    enabled[i] = false;
    positionUs[i] = MID_US;

  }

  nextPcaCheck = millis() + POWER_SETTLE_MS;
}

bool initializePcas() {
  bool found1 = devicePresent(0x40);
  bool found2 = devicePresent(0x41);

  Serial.print("PCA1 0x40: ");
  Serial.println(found1 ? "FOUND" : "MISSING");

  Serial.print("PCA2 0x41: ");
  Serial.println(found2 ? "FOUND" : "MISSING");

  if (!found1 || !found2) {

    Serial.println("Waiting for both PCA boards. Check logic power, addresses and I2C wiring.");
    return false;
  }

  bool started1 = pca1.begin();
  bool started2 = pca2.begin();

  if (!started1 || !started2) {

    Serial.println(
      "PCA initialization failed."
    );

    return false;
  }

  pca1.setPWMFreq(50);
  pca2.setPWMFreq(50);

  delay(10);

  disableAll();

  Serial.println(
    "READY - outputs disabled."
  );

  Serial.println(
    "m: center all sequentially"
  );

  Serial.println(
    "w: full Shenzhen playback; f: 44s exhibition loop; p: data status"
  );

  Serial.println(
    "L/R: fast directional blow"
  );

  Serial.println(
    "b: release blow"
  );

  Serial.println(
    "s: hold"
  );

  Serial.println(
    "x: signals off"
  );

  Serial.println(
    "1-25: individual test"
  );

  Serial.println(
    "115200 baud, Newline. a: calibrate sound; d: lower all; v: readings; c: recalibrate; t20: threshold."
  );
  return true;
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {

  // ========================================
  // SERIAL INPUT
  // ========================================

  while (Serial.available() > 0) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') {

      if (inputLine.length() > 0) {

        handleCommand(inputLine);

        inputLine = "";

      }

    } else if (inputLine.length() < 16) {

      inputLine += c;

    }
  }

  if (!hardwareReady) {
    if ((long)(millis() - nextPcaCheck) < 0) return;
    hardwareReady = initializePcas();
    if (!hardwareReady) {
      nextPcaCheck = millis() + PCA_RETRY_MS;
      return;
    }
    if (autoStartPending) startCentering();
  }

  updateSound();
  unsigned long now = millis();

  // ========================================
  // BLOW TIMEOUT
  // ========================================

  if (
    (mode == 'l' || mode == 'r') &&
    now - lastBlowPacket > BLOW_TIMEOUT_MS
  ) {

    mode = 'b';

    Serial.println(
      "Blow timeout: returning to normal waves."
    );
  }

  // ========================================
  // SEQUENTIAL CENTERING
  // ========================================

  if (
    (mode == 'm' || mode == 'd') &&
    nextCenter < SERVO_COUNT &&
    (long)(now - nextCenterTime) >= 0
  ) {

    enableServo(nextCenter);

    printPosition(nextCenter);

    nextCenter++;

    nextCenterTime =
      now + CENTER_INTERVAL_MS;
  }

  // ========================================
  // UPDATE INTERVAL
  // ========================================

  if (now - lastTick < UPDATE_MS) return;

  float dt = fminf(
    (float)(now - lastTick) / 1000.0f,
    0.1f
  );

  lastTick = now;
  updateSea(dt, mode == 'w' || mode == 'l' || mode == 'r' || mode == 'b');
  const float phaseRate[3] = {1.0f, 0.73f, 1.37f};
  for (int k = 0; k < 3; ++k) {
    normalPhase[k] = fmodf(normalPhase[k] + 2.0f * PI * dt * phaseRate[k] /
                          (seaPeriodMs / 1000.0f), 2.0f * PI);
  }
  blowPhase = fmodf(blowPhase + 2.0f * PI * dt /
                    (BLOW_PERIOD_MS / 1000.0f), 2.0f * PI);

  bool oceanMode =
    mode == 'w' ||
    mode == 'l' ||
    mode == 'r' ||
    mode == 'b';

  bool blowing =
    mode == 'l' ||
    mode == 'r';

  // ========================================
  // OCEAN / BLOW CALCULATION
  // ========================================

  if (oceanMode) {

    waveFade +=
      (1.0f - waveFade) *
      (1.0f - expf(-dt / 0.9f));

    // ========================================
    // ROW HEIGHT PROPAGATION
    // ========================================

    for (int row = 0; row < 5; row++) {

      float desired = 0.0f;
      float desiredMix = 0.0f;
      float tau = RELEASE_TAU_S;

      if (blowing) {

        // R: R1 -> R5
        // L: R5 -> R1

        int distance =
          mode == 'l' ? 4 - row : row;

        // 150 ms delay per row
        bool arrived =
          (float)(now - modeStart) >=
          distance * PUSH_DELAY_MS;

        if (arrived) {

          desired =
            blowTargetUs(distance) - NORMAL_CENTER_US;

          desiredMix = 1.0f;
        }

        // Keep existing row smoothing
        tau =
          BLOW_TAU_NEAR_S +
          BLOW_TAU_ROW_S * distance;
      }

      // When the gust ends, all rows blend back to the ongoing sea wave together.
      float blend = 1.0f - expf(-dt / tau);
      bandBias[row] +=
        (desired - bandBias[row]) * blend;

      bandBlowMix[row] +=
        (desiredMix - bandBlowMix[row]) * blend;
    }

    // ========================================
    // Discard the final sub-microsecond release residue.
    if (!blowing) {
      for (int row = 0; row < 5; ++row) {
        if (fabsf(bandBias[row]) < 0.1f && bandBlowMix[row] < 0.001f) {
          bandBias[row] = 0;
          bandBlowMix[row] = 0;
        }
      }
    }

  } else {

    waveFade = 0.0f;

    for (int row = 0; row < 5; row++) {

      bandBias[row] = 0.0f;
      bandBlowMix[row] = 0.0f;

    }
  }

  bool allCentered = true;
  bool allLowered = true;

  // ========================================
  // UPDATE ALL 25 SERVOS
  // ========================================

  for (int i = 0; i < SERVO_COUNT; i++) {

    if (!enabled[i]) {

      allCentered = false;
      allLowered = false;
      continue;
    }

    float target = positionUs[i];
    if (mode == 'd') target = BOTTOM_US;

    // ========================================
    // CENTER MODE
    // ========================================

    if (mode == 'm') {

      target = MID_US;
    }

    // ========================================
    // INDIVIDUAL TEST
    // ========================================

    if (mode == 't' && i == selected) {

      unsigned long elapsed =
        now - modeStart;

      if (elapsed < 6000) {

        float phase =
          2.0f * PI * elapsed / 6000.0f;

        target =
          MID_US -
          AMPLITUDE_US * sinf(phase);

      } else {

        target = MID_US;
      }
    }

    // ========================================
    // TIMEOUT RETURN
    // ========================================

    if (mode == 'q') {

      target = MID_US;
    }

    // ========================================
    // OCEAN WAVE + BLOW
    // ========================================

    if (oceanMode) {

      int row = i / 5;
      int col = i % 5;

      float east = 2.0f - row;
      float north = 2.0f - col;
      float along = (east * sinf(seaTravelRad) + north * cosf(seaTravelRad)) * seaPhaseSpread;
      float across = east * cosf(seaTravelRad) - north * sinf(seaTravelRad);
      float normalWater =
        0.80f * sinf(normalPhase[0] - along * 0.58f) +
        0.14f * sinf(normalPhase[1] - along * 0.40f + across * 0.18f) +
        0.06f * sinf(normalPhase[2] - along * 0.32f - across * 0.25f);

      // BLOW PRESSURE
      // ========================================

      float pressure = constrain(
        bandBlowMix[row],
        0.0f,
        1.0f
      );

      // ========================================
      // MIRRORED DISTANCE
      // ========================================

      char direction = blowing ?
        mode : lastBlowDirection;

      int distance =
        direction == 'l' ? 4 - row : row;

      // ========================================
      // WAVE AMPLITUDE
      // ========================================

      float smallWave = blowWaveAmplitude(distance) +
        160.0f * sinf(PI * distance / 4.0f);
      float blowWater =
        0.85f * sinf(blowPhase - distance * 0.65f - col * 0.30f) +
        0.15f * sinf(2.0f * blowPhase - distance * 0.90f + col * 0.25f);

      float center = NORMAL_CENTER_US + bandBias[row];
      float waveOffset = waveFade * (
        (1.0f - pressure) * seaAmplitudeUs * normalWater +
        pressure * smallWave * blowWater);
      // Keep oscillation symmetric within the existing physical pulse limits.
      float safeLow = fmaxf((float)TOP_US, TOP_US - (float)SERVO_OFFSET_US[i]);
      float safeHigh = fminf((float)BOTTOM_US, BOTTOM_US - (float)SERVO_OFFSET_US[i]);
      center = constrain(center, safeLow, safeHigh);
      float requestedAmplitude = waveFade * (
        (1.0f - pressure) * seaAmplitudeUs + pressure * smallWave);
      float room = fmaxf(0.0f, fminf(center - safeLow, safeHigh - center));
      if (requestedAmplitude > room && requestedAmplitude > 0.0f) {
        waveOffset *= room / requestedAmplitude;
      }
      target = center - waveOffset;

    }

    // ========================================
    // GLOBAL LIMITS
    // ========================================

    target = constrain(
      target,
      (float)TOP_US,
      (float)BOTTOM_US
    );

    // ========================================
    // MOVEMENT SPEED
    // ========================================

    float maxStep = NORMAL_STEP_US;

    // NEW: faster response during blowing
    if (blowing) {

      maxStep = BLOW_STEP_US;

    } else if (mode == 'q' || mode == 'd') {

      maxStep = RELEASE_STEP_US;

    }

    float delta = constrain(
      target - positionUs[i],
      -maxStep,
      maxStep
    );

    if (fabsf(delta) > 0.01f) {

      positionUs[i] += delta;

      writeServo(i, positionUs[i]);
    }

    if (fabsf(positionUs[i] - BOTTOM_US) > 0.5f) allLowered = false;

    if (
      fabsf(positionUs[i] - MID_US) > 0.5f
    ) {

      allCentered = false;
    }
  }

  // ========================================
  // CENTERING COMPLETE
  // ========================================

  if (mode == 'd' && nextCenter == SERVO_COUNT && allLowered) {
    mode = 's';
    Serial.println("Lowered. Holding bottom positions. Send m before restarting waves.");
  }

  if (
    mode == 'm' &&
    nextCenter == SERVO_COUNT &&
    allCentered &&
    (!autoStartPending || (long)(now - nextCenterTime) >= 0)
  ) {

    mode = 's';
    waveAllowed = true;

    if (autoStartPending) {
      autoStartPending = false;
      seaDemoMode = true;
      seaDemoClockMs = 0.0f;
      Serial.println("Centered. Starting exhibition loop and sound calibration.");
      startSoundCalibration();
    } else {
      Serial.println("Centered. Send w for full playback, f for the exhibition loop, or L/R.");
    }
  }

  // ========================================
  // INDIVIDUAL TEST COMPLETE
  // ========================================

  if (
    mode == 't' &&
    now - modeStart >= 6500 &&
    fabsf(positionUs[selected] - MID_US) < 0.5f
  ) {

    mode = 's';

    Serial.println(
      "Individual test finished."
    );
  }
}

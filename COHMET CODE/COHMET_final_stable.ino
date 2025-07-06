#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

// ---------------------- Pin Definitions ----------------------
#define RX_PIN 0
#define TX_PIN 1
#define MANUAL_START_PIN 3
#define BUZZER_PIN 5
#define LED_PIN 6
#define BT_LED 7
#define POWER_INDICATOR 8
#define MUX_S0 9
#define MUX_S1 10
#define MUX_S2 11
#define MUX_S3 12
#define MUX_SIGNAL A0
#define BATTERY_READ A1

// ---------------------- Game Config ----------------------
#define NUM_WELLS 9
#define LEDS_PER_WELL 10
#define NUM_LEDS (NUM_WELLS * LEDS_PER_WELL)
#define COLOR_PINK   0
#define COLOR_BLUE   1
#define COLOR_YELLOW 2
#define COLOR_GREEN  3
#define COLOR_OFF    4

const int PINK_MIN = 300, PINK_MAX = 380;
const int BLUE_MIN = 470, BLUE_MAX = 550;
const int YELLOW_MIN = 640, YELLOW_MAX = 730;

uint8_t colorMap[3][3] = {
  {250, 50, 150},   // Pink
  {0, 70, 255},     // Blue
  {255, 128, 0}     // Yellow
};

// ---------------------- State Variables ----------------------
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial BT(RX_PIN, TX_PIN);

uint8_t wellColors[NUM_WELLS];
bool matchedWells[NUM_WELLS];
unsigned long wellTimes[NUM_WELLS];

bool gameRunning = false;
bool sequenceAssigned = false;

uint8_t currentWell = 0;
uint8_t errorCount = 0;
unsigned long wellStartTime;

unsigned long lastFlashTime = 0;
bool flashOn = false;
const unsigned long flashInterval = 300;

// ---------------------- Helper Functions ----------------------
void setMuxChannel(uint8_t ch) {
  digitalWrite(MUX_S0, ch & 0x01);
  digitalWrite(MUX_S1, (ch >> 1) & 0x01);
  digitalWrite(MUX_S2, (ch >> 2) & 0x01);
  digitalWrite(MUX_S3, (ch >> 3) & 0x01);
}

void setWellColor(uint8_t well, uint8_t colorId) {
  uint8_t r = 0, g = 0, b = 0;
  if (colorId <= COLOR_YELLOW) {
    r = colorMap[colorId][0];
    g = colorMap[colorId][1];
    b = colorMap[colorId][2];
  } else if (colorId == COLOR_GREEN) {
    r = 0; g = 255; b = 0;
  } else {
    r = g = b = 0;
  }

  for (uint8_t i = 0; i < LEDS_PER_WELL; i++) {
    strip.setPixelColor(well * LEDS_PER_WELL + i, strip.Color(r, g, b));
  }
}

void clearAllWells() {
  for (uint8_t i = 0; i < NUM_WELLS; i++) {
    setWellColor(i, COLOR_OFF);
  }
  strip.show();
}

uint8_t readPegColor(uint16_t adcValue) {
  if (adcValue >= PINK_MIN && adcValue <= PINK_MAX) return COLOR_PINK;
  if (adcValue >= BLUE_MIN && adcValue <= BLUE_MAX) return COLOR_BLUE;
  if (adcValue >= YELLOW_MIN && adcValue <= YELLOW_MAX) return COLOR_YELLOW;
  return 255;
}

void checkCurrentWell() {
  setMuxChannel(currentWell);
  delay(10);
  int adcVal = analogRead(MUX_SIGNAL);
  uint8_t pegColor = readPegColor(adcVal);

  if (pegColor == wellColors[currentWell]) {
    setWellColor(currentWell, COLOR_GREEN);
    strip.show();
    matchedWells[currentWell] = true;
    wellTimes[currentWell] = millis() - wellStartTime;

    BT.print("TIME:");
    BT.println(wellTimes[currentWell]);

    currentWell++;
    if (currentWell < NUM_WELLS) {
      wellStartTime = millis();
    } else {
      BT.print("TIME:");
      BT.println(errorCount);
      gameRunning = false;
      showCompletionAnimation();
      sequenceAssigned = false;
    }
  } else if (pegColor != 255) {
    errorCount++;

    for (int i = 0; i < 3; i++) {
      for (uint8_t j = 0; j < LEDS_PER_WELL; j++) {
        strip.setPixelColor(currentWell * LEDS_PER_WELL + j, strip.Color(255, 0, 0));
      }
      strip.show();
      tone(BUZZER_PIN, 1000, 100);
      delay(150);

      for (uint8_t j = 0; j < LEDS_PER_WELL; j++) {
        strip.setPixelColor(currentWell * LEDS_PER_WELL + j, strip.Color(0, 0, 0));
      }
      strip.show();
      delay(150);
    }

    uint8_t col = wellColors[currentWell];
    for (uint8_t j = 0; j < LEDS_PER_WELL; j++) {
      strip.setPixelColor(currentWell * LEDS_PER_WELL + j,
        strip.Color(colorMap[col][0], colorMap[col][1], colorMap[col][2]));
    }
    strip.show();
  }
}

void updateWellDisplay() {
  for (uint8_t i = 0; i < NUM_WELLS; i++) {
    if (matchedWells[i]) setWellColor(i, COLOR_GREEN);
    else if (i == currentWell && flashOn) setWellColor(i, wellColors[i]);
    else setWellColor(i, COLOR_OFF);
  }
  strip.show();
}

void showCompletionAnimation() {
  for (int k = 0; k < 3; k++) {
    for (uint8_t i = 0; i < NUM_WELLS; i++) setWellColor(i, COLOR_GREEN);
    strip.show(); delay(300);
    clearAllWells(); delay(300);
  }

  for (int frame = 0; frame < 50; frame++) {
    for (uint8_t w = 0; w < NUM_WELLS; w++) {
      for (uint8_t l = 0; l < LEDS_PER_WELL; l++) {
        uint16_t hue = (frame * 500 + w * 3000 + l * 400) % 65536;
        strip.setPixelColor(w * LEDS_PER_WELL + l, strip.gamma32(strip.ColorHSV(hue)));
      }
    }
    strip.show();
    delay(50);
  }

  // Ending tone sequence
  tone(BUZZER_PIN, 800, 150); delay(200);
  tone(BUZZER_PIN, 1200, 150); delay(200);
  tone(BUZZER_PIN, 1600, 250); delay(300);

  clearAllWells();
}

void randomizeColors() {
  uint8_t pos[NUM_WELLS] = {0,1,2,3,4,5,6,7,8};
  for (uint8_t i = 0; i < NUM_WELLS; i++) {
    uint8_t j = random(i, NUM_WELLS);
    uint8_t t = pos[i]; pos[i] = pos[j]; pos[j] = t;
  }

  for (uint8_t i = 0; i < NUM_WELLS; i++) {
    if (i < 3) wellColors[pos[i]] = COLOR_PINK;
    else if (i < 6) wellColors[pos[i]] = COLOR_BLUE;
    else wellColors[pos[i]] = COLOR_YELLOW;
  }

  sequenceAssigned = true;
  clearAllWells();
  tone(BUZZER_PIN, 1500, 100);
  delay(150);
}

void checkBattery() {
  int adcVal = analogRead(BATTERY_READ);
  float voltage = adcVal * (5.0 / 1023.0) * 2.0; // voltage divider assumed
  float percent = ((voltage - 5.8) / (6.4 - 5.8)) * 100.0;
  percent = constrain(percent, 0, 100);
  BT.println((int)percent);
}


// ---------------------- Setup ----------------------
void setup() {
  pinMode(MANUAL_START_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BT_LED, OUTPUT);
  pinMode(POWER_INDICATOR, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);

  digitalWrite(POWER_INDICATOR, HIGH);
  digitalWrite(BT_LED, LOW);

  strip.begin();
  strip.show();
  randomSeed(analogRead(0));
  BT.begin(9600);

  // Startup tone
  tone(BUZZER_PIN, 600, 100); delay(150);
  tone(BUZZER_PIN, 1000, 100); delay(150);
  tone(BUZZER_PIN, 1400, 150); delay(200);

  // RGB color blending animation
  for (int j = 0; j < 256; j++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint16_t hue = (i * 700 + j * 256) % 65536;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
    }
    strip.show();
    delay(15);
  }

  clearAllWells();
}

// ---------------------- Loop ----------------------
void loop() {
  if (BT.available()) {
    uint8_t cmd = BT.read();
    digitalWrite(BT_LED, HIGH); delay(100); digitalWrite(BT_LED, LOW);

    if (cmd == 1) {
      randomizeColors();
    } else if (cmd == 6 && sequenceAssigned) {
      gameRunning = true;
      currentWell = 0;
      errorCount = 0;
      for (int i = 0; i < NUM_WELLS; i++) matchedWells[i] = false;
      clearAllWells();
      wellStartTime = millis();
      lastFlashTime = millis();
      flashOn = false;
    } else if (cmd == 2) {
      checkBattery();
    }
  }

  if (gameRunning) {
    if (millis() - lastFlashTime > flashInterval) {
      flashOn = !flashOn;
      lastFlashTime = millis();
      updateWellDisplay();
    }
    checkCurrentWell();
  } else if (!gameRunning && sequenceAssigned) {
    for (uint8_t i = 0; i < NUM_WELLS; i++) setWellColor(i, wellColors[i]);
    strip.show();
    delay(100);
  } else {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      uint16_t hue = (millis() * 10 + i * 100) % 65536;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
    }
    strip.show();
    delay(30);
  }

  if (digitalRead(MANUAL_START_PIN)) {
    randomizeColors();
    delay(200);
    tone(BUZZER_PIN, 2000, 200);
    delay(500);
    gameRunning = true;
    currentWell = 0;
    errorCount = 0;
    for (int i = 0; i < NUM_WELLS; i++) matchedWells[i] = false;
    clearAllWells();
    wellStartTime = millis();
  }
}

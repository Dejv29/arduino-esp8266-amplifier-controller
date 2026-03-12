#include <IRremote.h>
#include <EEPROM.h>
#include <avr/wdt.h>





// === Piny ===
const int relayInputPins[6] = { 3, 4, 7, 8, 12, A0 };  // Vstupní relé
const int muteRelayPin = A1;
const int powerRelayPin = A2;
const int speakerRelayPin = A3;
const int pwmPin = 10;         // PWM výstup pro rychlost motoru
const int dirUpPin = 11;       // směr otáčení pro VOLUP
const int dirDownPin = A5;     // směr otáčení pro VOLDOWN
const int potPin = A7;

const int irReceiverPin = 2;

const int redPin = 5;
const int greenPin = 6;
const int bluePin = 9;

const int buttonPanelPin = A4;
//const int buttonTerracePin = A5;

int currentADC = 0;            // aktuální hodnota
int targetADC = 0;             // požadovaná hodnota (jen pro krátký stisk)
bool movingToTarget = false;   // zda se motor snaží dojet na cíl

// Zrychlování volume
unsigned long accelStartTime = 0;
const unsigned long accelDuration = 3000; // doba do maximální rychlosti
const int maxPWM = 255;
const int minPWM = 255;

//Krátké stisky volume
bool motorImpulseActive = false;
unsigned long motorImpulseEndTime = 0;
bool motorImpulseDirectionUp = true; // true = UP, false = DOWN

//Max volume
const int numInputs = 6;
int maxInputVol[numInputs];  // max. hlasitost v % pro každý vstup (0–100)
bool muteActive = false;

// Automaticke vypnuti po neaktivite
unsigned long lastActivity = 0;
const unsigned long autoOffTime = 3UL * 60UL * 60UL * 1000UL; // 3 hodiny

// Uzivatelsky casovac vypnuti
unsigned long timerEnd = 0;   // Čas, kdy má dojít k vypnutí
unsigned long timerRemaining = 0;  // Zbývajicí čas do vypnutí
bool timerActive = false;     // Jestli je časovač aktivní

// === IR Učení ===
const int maxIrCodes = 10;

unsigned long irCodesPower[maxIrCodes];
unsigned long irCodesVolUp[maxIrCodes];
unsigned long irCodesVolDown[maxIrCodes];
unsigned long irCodesSpeakers[maxIrCodes];
unsigned long irCodesInput[maxIrCodes];
unsigned long irCodesPlay[maxIrCodes];
unsigned long irCodesNext[maxIrCodes];
unsigned long irCodesPrev[maxIrCodes];

int irCountPower = 0;
int irCountVolUp = 0;
int irCountVolDown = 0;
int irCountSpeakers = 0;
int irCountInput = 0;
int irCountPlay = 0;
int irCountNext = 0;
int irCountPrev = 0;


enum IRLearnMode {
  IR_NONE,

  // Učení
  IR_LEARN_POWER,
  IR_LEARN_VOLUP,
  IR_LEARN_VOLDOWN,
  IR_LEARN_SPEAKERS,
  IR_LEARN_INPUT,
  IR_LEARN_PLAY,
  IR_LEARN_NEXT,
  IR_LEARN_PREV,

  // Odebrání jednoho
  IR_REMOVE_POWER,
  IR_REMOVE_VOLUP,
  IR_REMOVE_VOLDOWN,
  IR_REMOVE_SPEAKERS,
  IR_REMOVE_INPUT,
  IR_REMOVE_PLAY,
  IR_REMOVE_NEXT,
  IR_REMOVE_PREV,

  // Odebrání celé skupiny
  IR_REMOVE_POWER_ALL,
  IR_REMOVE_VOLUP_ALL,
  IR_REMOVE_VOLDOWN_ALL,
  IR_REMOVE_SPEAKERS_ALL,
  IR_REMOVE_INPUT_ALL,
  IR_REMOVE_PLAY_ALL,
  IR_REMOVE_NEXT_ALL,
  IR_REMOVE_PREV_ALL
};

IRLearnMode currentIRLearnMode = IR_NONE;

unsigned long irLearnStartTime = 0;
const unsigned long irLearnTimeout = 10000; // 10 sekund




// Kalibrace hlasitosti
int adcMin = 1;   // Hodnota pro 0 % (30)
int adcMax = 1023;  // Hodnota pro 100 % (991)

const int tolerance = 5;      // kolik jednotek může být od cíle, abychom považovali za dosažení


int volumePosition = 0;  // Startovní pozice



// === Stavové proměnné ===
bool powerState = false;
int activeInput = 0;  // 0 až 5
bool speakerState = false;
bool targetReached = false;   // true = motor se nepohybuje, stav dosažení cíle
bool poweringUp = false;
unsigned long powerOnTime = 0;


unsigned long muteEndTime = 0;

unsigned long lastPulseTime = 0;
bool pulseState = false;
const unsigned long pulseInterval = 250;  // 250 ms náběh/pád = 500 ms cyklus






// === Tlačítka ===
enum Button { BTN_NONE,
              BTN_POWER,
              BTN_INPUT,
              BTN_SPEAKERS,
              BTN_VOLUP,
              BTN_VOLDOWN,
              BTN_PLAY,
              BTN_NEXT,
              BTN_PREV };

              
// IR stisk
unsigned long irPressStartTime = 0;
bool irButtonHeld = false;
unsigned long lastIrReceiveTime = 0;
const unsigned long irHoldThreshold = 400;
const unsigned long irRepeatThreshold = 120;
Button lastIrButton = BTN_NONE;
bool irActionTaken = false;


// Short pulse
bool shortPulseActive = false;
unsigned long shortPulseStartTime = 0;
Button shortPulseButton = BTN_NONE;
const unsigned long shortPulseDuration = 250;

unsigned long volumeCurrentDelay = 50;   // výchozí prodleva (pomalejší začátek)
const unsigned long volumeMinDelay = 20;  // nejrychlejší krokování
const float volumeAccelFactor = 0.97;     // násobek pro zrychlování (čím menší, tím rychlejší zrychlení)


unsigned long lastStepTime = 0;
const int volumeStepDelay = 30;      // rychlost krokování v ms
bool volumeButtonHeld = false;       // příznak, že držíme tlačítko pro volume
Button heldVolumeButton = BTN_NONE;  // který tlačítko držíme

bool buttonActionTaken = false;



// Fiktivní hodnoty pro tlačítka (přes dělič odporů)
Button getButtonFromADC(int adcVal) {
  //Serial.println(adcVal);
  //delay(1000);
  if (adcVal < 100) return BTN_POWER; // 330r
  if (adcVal < 150) return BTN_INPUT; // 1K45
  if (adcVal < 240) return BTN_SPEAKERS; // 2K7
  if (adcVal < 340) return BTN_VOLUP; // 4K7
  if (adcVal < 430) return BTN_VOLDOWN; // 6K7
  if (adcVal < 480) return BTN_PLAY; // 8K2
  if (adcVal < 530) return BTN_NEXT; // 10K
  if (adcVal < 580) return BTN_PREV; // 12K
  return BTN_NONE;
}

// === Časování pro rozlišení stisku ===
unsigned long buttonPressTime = 0;
bool buttonHeld = false;
Button lastButton = BTN_NONE;
const unsigned long longPressThreshold = 400;

void calibratePotentiometer() {
  Serial.println(F("Spouštím časovou kalibraci potenciometru..."));

  int midPWM = 255;//(minPWM + maxPWM) / 2

  // --- 1. Směr Volume UP (adc roste) ---
  Serial.println(F("Kalibrace horní meze (volume UP)..."));
  digitalWrite(dirDownPin, LOW);
  delay(100);
  digitalWrite(dirUpPin, HIGH);
  analogWrite(pwmPin, midPWM);

  unsigned long startTime = millis();
  int lastADC = analogRead(potPin);
  while (millis() - startTime < 14000) {
    int adc = analogRead(potPin);
    if (adc >= 1023) {
      Serial.println(F("Dosaženo ADC = 1023 (předčasné zastavení)"));
      break;
    }
    lastADC = adc;
    delay(10);
  }
  stopMotor();
  adcMax = lastADC;
  Serial.print(F("adcMax = "));
  Serial.println(adcMax);

  delay(500);

  // --- 2. Směr Volume DOWN (adc klesá) ---
  Serial.println(F("Kalibrace dolní meze (volume DOWN)..."));
  digitalWrite(dirUpPin, LOW);
  delay(100);
  digitalWrite(dirDownPin, HIGH);
  analogWrite(pwmPin, midPWM);

  startTime = millis();
  lastADC = analogRead(potPin);
  while (millis() - startTime < 14000) {
    int adc = analogRead(potPin);
    if (adc <= 0) {
      Serial.println(F("Dosaženo ADC = 0 (předčasné zastavení)"));
      break;
    }
    lastADC = adc;
    delay(10);
  }
  stopMotor();
  adcMin = lastADC;
  Serial.print(F("adcMin = "));
  Serial.println(adcMin);

  Serial.println(F("Kalibrace dokončena."));
}


// Kalibrace potenciomteru
void calibrateVolumePot() {
  stopMotor();
  digitalWrite(dirUpPin, HIGH);
  analogWrite(pwmPin, 255);
  delay(6000);
  stopMotor();
  adcMax = analogRead(potPin);

  digitalWrite(dirDownPin, HIGH);
  analogWrite(pwmPin, 255);
  delay(6000);
  stopMotor();
  adcMin = analogRead(potPin);

  Serial.print(F("Kalibrace dokončena: adcMin = "));
  Serial.print(adcMin);
  Serial.print(F(", adcMax = "));
  Serial.println(adcMax);
  }


// === WiFi komunikace ===
void checkWifiSerial() {
  while (Serial.available()) {
    char cmd[32];
    byte len = Serial.readBytesUntil('\n', cmd, sizeof(cmd) - 1);
    cmd[len] = '\0'; // ukončíme string

    // Trim - odstranění bílých znaků na začátku a konci
    // Arduino nemá vestavěný trim pro char*, takže implementujeme jednoduchý trim:
    // odstraníme leading spaces
    int start = 0;
    while (cmd[start] == ' ' || cmd[start] == '\r' || cmd[start] == '\t') start++;
    // odstraníme trailing spaces
    int end = len - 1;
    while (end >= start && (cmd[end] == ' ' || cmd[end] == '\r' || cmd[end] == '\t')) {
      cmd[end] = '\0';
      end--;
    }
    // přesuneme string na začátek, pokud je start > 0
    if (start > 0) {
      for (int i = start; i <= end; i++) {
        cmd[i - start] = cmd[i];
      }
      cmd[end - start + 1] = '\0';
    }

    // Porovnání příkazů:    
    if (strcmp(cmd, "GET_POWER") == 0) {
      Serial.println(powerState ? "ON" : "OFF");

    } else if (strcmp(cmd, "SET_POWER_ON") == 0) {
      togglePower(true);

    } else if (strcmp(cmd, "SET_POWER_OFF") == 0) {
      togglePower(false);

    } else if (strncmp(cmd, "SET_INPUT_", 10) == 0) {
      if (!powerState) togglePower(true);
      int i = atoi(cmd + 10);
      if (i >= 0 && i < 6) {
        setInput(i);
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");
      }

    } else if (strncmp(cmd, "SET_VOLUME_", 11) == 0) {
      if (!powerState) togglePower(true);
      int percent = atoi(cmd + 11);
      moveMotorToTarget(percent);
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");

    } else if (strcmp(cmd, "GET_VOLUME_RAW") == 0) {
      int adc = analogRead(potPin);
      Serial.print(F("Volume RAW: "));
      Serial.println(adc);

    } else if (strcmp(cmd, "GET_VOLUME") == 0) {
      int volumePercent = getVolumePercent();
      Serial.print(F("Volume: "));
      Serial.print(volumePercent);
      Serial.println(F(" %"));

    } else if (strcmp(cmd, "SET_SPEAKER_A") == 0) {
      if (!powerState) togglePower(true);
      speakerState = false;
      digitalWrite(speakerRelayPin, LOW);
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");

    } else if (strcmp(cmd, "SET_SPEAKER_B") == 0) {
      if (!powerState) togglePower(true);
      speakerState = true;
      digitalWrite(speakerRelayPin, HIGH);
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");

    // IR LEARN
    } else if (strcmp(cmd, "IR_LEARN_POWER") == 0) {
      currentIRLearnMode = IR_LEARN_POWER;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: POWER"));

    } else if (strcmp(cmd, "IR_LEARN_VOLUP") == 0) {
      currentIRLearnMode = IR_LEARN_VOLUP;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: VOLUP"));

    } else if (strcmp(cmd, "IR_LEARN_VOLDOWN") == 0) {
      currentIRLearnMode = IR_LEARN_VOLDOWN;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: VOLDOWN"));

    } else if (strcmp(cmd, "IR_LEARN_SPEAKERS") == 0) {
      currentIRLearnMode = IR_LEARN_SPEAKERS;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: SPEAKERS"));

    } else if (strcmp(cmd, "IR_LEARN_INPUT") == 0) {
      currentIRLearnMode = IR_LEARN_INPUT;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: INPUT"));

    // IR REMOVE
    } else if (strcmp(cmd, "IR_REMOVE_POWER") == 0) {
      currentIRLearnMode = IR_REMOVE_POWER;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: POWER"));

    } else if (strcmp(cmd, "IR_REMOVE_VOLUP") == 0) {
      currentIRLearnMode = IR_REMOVE_VOLUP;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: VOLUP"));

    } else if (strcmp(cmd, "IR_REMOVE_VOLDOWN") == 0) {
      currentIRLearnMode = IR_REMOVE_VOLDOWN;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: VOLDOWN"));

    } else if (strcmp(cmd, "IR_REMOVE_SPEAKERS") == 0) {
      currentIRLearnMode = IR_REMOVE_SPEAKERS;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: SPEAKERS"));

    } else if (strcmp(cmd, "IR_REMOVE_INPUT") == 0) {
      currentIRLearnMode = IR_REMOVE_INPUT;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: INPUT"));

    } else if (strcmp(cmd, "CALIBRATE_POT") == 0) {
      togglePower(false);
      calibratePotentiometer();
      //calibrateVolumePot();
      togglePower(true);
      moveMotorToTarget(22);

    } else if (strcmp(cmd, "IR_PRINT") == 0) {
      printStoredIRCodes();

    } else if (strcmp(cmd, "IR_REMOVE_POWER_ALL") == 0) {
      clearIRGroup(IR_REMOVE_POWER_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_VOLUP_ALL") == 0) {
      clearIRGroup(IR_REMOVE_VOLUP_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_VOLDOWN_ALL") == 0) {
      clearIRGroup(IR_REMOVE_VOLDOWN_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_SPEAKERS_ALL") == 0) {
      clearIRGroup(IR_REMOVE_SPEAKERS_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_INPUT_ALL") == 0) {
      clearIRGroup(IR_REMOVE_INPUT_ALL);

    } else if (strcmp(cmd, "PULSE_VOLUP") == 0) {
      if (powerState) {
        shortPressVolUp();
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");
        }

    } else if (strcmp(cmd, "PULSE_VOLDOWN") == 0) {
      if (powerState) {
        shortPressVolDown();
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");
      }

    } else if (strncmp(cmd, "SET_MAX_VOL_INPUT_", 18) == 0) {
      int input = cmd[18] - '0';      // např. '3' → 3
      int percent = atoi(cmd + 20);  // např. SET_MAX_VOL_INPUT_3_60
        if (input >= 0 && input < numInputs) {
        saveMaxVolumeLimit(input, percent);
      }

    } else if (strcmp(cmd, "SET_CURRENT_MAX_VOLUME") == 0) {
        if (!powerState) {
      Serial.println(F("Cannot set max volume: system is OFF."));
      } else {
      currentADC = analogRead(potPin);
      int percent = map(currentADC, adcMin, adcMax, 0, 100);
      percent = constrain(percent, 0, 100);
      saveMaxVolumeLimit(activeInput, percent);
      Serial.print(F("Max volume limit for input "));
      Serial.print(activeInput);
      Serial.print(F(" set to current value: "));
      Serial.print(percent);
      Serial.println(F("%"));
      }
    } else if (strcmp(cmd, "IR_LEARN_PLAY") == 0) {
      currentIRLearnMode = IR_LEARN_PLAY;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: PLAY"));

    } else if (strcmp(cmd, "IR_LEARN_NEXT") == 0) {
      currentIRLearnMode = IR_LEARN_NEXT;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: NEXT"));

    } else if (strcmp(cmd, "IR_LEARN_PREV") == 0) {
      currentIRLearnMode = IR_LEARN_PREV;
      irLearnStartTime = millis();
      Serial.println(F("IR learning: PREV"));

    } else if (strcmp(cmd, "IR_REMOVE_PLAY") == 0) {
      currentIRLearnMode = IR_REMOVE_PLAY;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: PLAY"));

    } else if (strcmp(cmd, "IR_REMOVE_NEXT") == 0) {
      currentIRLearnMode = IR_REMOVE_NEXT;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: NEXT"));

    } else if (strcmp(cmd, "IR_REMOVE_PREV") == 0) {
      currentIRLearnMode = IR_REMOVE_PREV;
      irLearnStartTime = millis();
      Serial.println(F("IR remove mode: PREV"));

    } else if (strcmp(cmd, "IR_REMOVE_PLAY_ALL") == 0) {
      clearIRGroup(IR_REMOVE_PLAY_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_PREV_ALL") == 0) {
      clearIRGroup(IR_REMOVE_PREV_ALL);

    } else if (strcmp(cmd, "IR_REMOVE_NEXT_ALL") == 0) {
      clearIRGroup(IR_REMOVE_NEXT_ALL);

    } else if (strcmp(cmd, "MEDIA_PLAY") == 0) {
      Serial.println(F("SEND_MEDIA_PLAY"));
        lastActivity = millis();
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();

    } else if (strcmp(cmd, "MEDIA_START") == 0) {
      Serial.println(F("SEND_MEDIA_START"));
        lastActivity = millis();
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
        analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();

    } else if (strcmp(cmd, "MEDIA_PREV") == 0) {
      Serial.println(F("SEND_MEDIA_PREV"));
        lastActivity = millis();
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();

    } else if (strcmp(cmd, "MEDIA_NEXT") == 0) {
      Serial.println(F("SEND_MEDIA_NEXT"));
        lastActivity = millis();
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();

    } else if (strcmp(cmd, "WIPE_EEPROM") == 0) {
      wipeEEPROM();

    } else if (strncmp(cmd, "SET_AUTO_OFF_", 13) == 0) {
      // získáme číslo za prefixem
      long sec = atol(cmd + 13); // převede string na číslo (long)
      if (sec > 0) {
        setTimer(sec); // nastaví uživatelský časovač
        Serial.print("Auto-off set to ");
        Serial.print(sec);
        Serial.println(" seconds.");
       } else if (sec == 0) {
          setTimer(sec);
        
      } else {
        Serial.println("Invalid time for auto-off.");
      }
    }
    Serial.print(F("<"));
    Serial.print(powerState ? "ON;" : "OFF;");
    Serial.print(activeInput);
    Serial.print(F(";"));
    int volumePercent = getVolumePercent();
      Serial.print(volumePercent);
      Serial.print(F(";"));
    Serial.print(speakerState);
      if (timerActive) {
        timerRemaining = ((timerEnd - millis()) / 1000UL);
          }
      else {
        timerRemaining = 0;
      }
      Serial.print(F(";"));
      Serial.print(timerRemaining);
      Serial.println(F(";>"));
  }
}

#define EEPROM_MAGIC            0x42
#define EEPROM_VOLUME_MAGIC     0x99

#define EEPROM_ADDR_MAGIC       0
#define EEPROM_ADDR_IRDATA      1
#define EEPROM_ADDR_VOLUME_MAGIC 400
#define EEPROM_ADDR_VOLUME_LIMITS (EEPROM_ADDR_VOLUME_MAGIC + 1)


void saveIRCodesToEEPROM() {
  int addr = EEPROM_ADDR_IRDATA;
  EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);

  auto saveSet = [&](int count, unsigned long* codes) {
    EEPROM.update(addr++, count);
    for (int i = 0; i < count; i++) {
      EEPROM.put(addr, codes[i]);
      addr += sizeof(unsigned long);
    }
  };

  saveSet(irCountPower,    irCodesPower);
  saveSet(irCountVolUp,    irCodesVolUp);
  saveSet(irCountVolDown,  irCodesVolDown);
  saveSet(irCountSpeakers, irCodesSpeakers);
  saveSet(irCountInput,    irCodesInput);
  saveSet(irCountPlay,     irCodesPlay);
  saveSet(irCountNext,     irCodesNext);
  saveSet(irCountPrev,     irCodesPrev);
}

void loadIRCodesFromEEPROM() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    Serial.println(F("EEPROM není inicializováno platnými IR daty."));
    return;
  }

  int addr = EEPROM_ADDR_IRDATA;

  auto loadSet = [&](int& count, unsigned long* codes) {
    count = EEPROM.read(addr++);
    count = constrain(count, 0, maxIrCodes);
    for (int i = 0; i < count; i++) {
      EEPROM.get(addr, codes[i]);
      addr += sizeof(unsigned long);
    }
  };

  loadSet(irCountPower,    irCodesPower);
  loadSet(irCountVolUp,    irCodesVolUp);
  loadSet(irCountVolDown,  irCodesVolDown);
  loadSet(irCountSpeakers, irCodesSpeakers);
  loadSet(irCountInput,    irCodesInput);
  loadSet(irCountPlay,     irCodesPlay);
  loadSet(irCountNext,     irCodesNext);
  loadSet(irCountPrev,     irCodesPrev);

  Serial.println(F("IR kódy načteny z EEPROM."));
}

void saveMaxVolumeLimit(int input, int value) {
  if (input >= 0 && input < numInputs) {
    value = constrain(value, 0, 100);
    EEPROM.update(EEPROM_ADDR_VOLUME_LIMITS + input, value);
    maxInputVol[input] = value;
    EEPROM.update(EEPROM_ADDR_VOLUME_MAGIC, EEPROM_VOLUME_MAGIC);
    Serial.print(F("Uloženo maxVolumeLimit vstupu "));
    Serial.print(input);
    Serial.print(F(": "));
    Serial.println(value);
  }
}

void loadMaxVolumeLimits() {
  if (EEPROM.read(EEPROM_ADDR_VOLUME_MAGIC) != EEPROM_VOLUME_MAGIC) {
    Serial.println(F("Volume limity nejsou inicializované."));
    for (int i = 0; i < numInputs; i++) {
      maxInputVol[i] = 100;
    }
    return;
  }

  Serial.println(F("Načítám max volume limity:"));
  for (int i = 0; i < numInputs; i++) {
    int val = EEPROM.read(EEPROM_ADDR_VOLUME_LIMITS + i);
    maxInputVol[i] = constrain(val, 0, 100);
    Serial.print(F("Vstup "));
    Serial.print(i);
    Serial.print(F(": "));
    Serial.println(maxInputVol[i]);
  }
}


void clearIRGroup(IRLearnMode group) {
  switch (group) {
    case IR_REMOVE_POWER_ALL:
      irCountPower = 0;
      break;
    case IR_REMOVE_VOLUP_ALL:
      irCountVolUp = 0;
      break;
    case IR_REMOVE_VOLDOWN_ALL:
      irCountVolDown = 0;
      break;
    case IR_REMOVE_SPEAKERS_ALL:
      irCountSpeakers = 0;
      break;
    case IR_REMOVE_INPUT_ALL:
      irCountInput = 0;
      break;
    case IR_REMOVE_PLAY_ALL:
      irCountPlay = 0;
      break;
    case IR_REMOVE_NEXT_ALL:
      irCountNext = 0;
      break;
    case IR_REMOVE_PREV_ALL:
      irCountPrev = 0;
      break;
    default:
      return;
  }

  Serial.print(F("Cleared all IR codes for group: "));
  Serial.println(group);
  saveIRCodesToEEPROM();
}


void wipeEEPROM() {
  Serial.println("Wiping EEPROM...");
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);  // nebo 0x00 – ale 0xFF je výchozí stav nepopsané EEPROM
  }
  Serial.println("EEPROM wiped.");
  delay(500);
  resetARDUINO();
}

void resetARDUINO() {
  Serial.println(F("Soft reset in 5s..."));
  delay(5000); // aby se stihla odeslat zpráva přes Serial
  wdt_enable(WDTO_15MS);
  while (1);
}

// === Inicializace ===
void setup() {
  for (int i = 0; i < 6; i++) pinMode(relayInputPins[i], OUTPUT);
  pinMode(muteRelayPin, OUTPUT);
  pinMode(powerRelayPin, OUTPUT);
  pinMode(speakerRelayPin, OUTPUT);
  pinMode(irReceiverPin, INPUT);
  pinMode(pwmPin, OUTPUT);
  

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);


  Serial.begin(9600);
  Serial.println(F("Inicializace..."));

  //wipeEEPROM(); // EEPROM Wipe
  //delay(60000);

  // Výchozí hodnoty
  powerState = false;
  activeInput = 0;
  lastActivity = 0;
  // calibratePotentiometer();
  moveMotorToTarget(20);
  IrReceiver.begin(irReceiverPin, ENABLE_LED_FEEDBACK);

  togglePower(powerState);  // Nastaví vše na OFF
  //EEPROM.write(EEPROM_ADDR_MAGIC, 0); // odkomentuj pro reset, pak zase zakomentuj
  loadIRCodesFromEEPROM();
  
  if (EEPROM.read(EEPROM_ADDR_VOLUME_MAGIC) != EEPROM_VOLUME_MAGIC) {
  Serial.println(F("EEPROM max volume uninitialized. Resetting defaults."));
  for (int i = 0; i < numInputs; i++) {
    saveMaxVolumeLimit(i, 100); // výchozí maximum
  }
} else {
  loadMaxVolumeLimits();

}
}

unsigned long lastInputChange = 0;
const unsigned long brightDuration = 5000; // 3 sekundy plný jas

// === Nastavení vstupu ===
void setInput(int index) {
    if (activeInput != index) {
      digitalWrite(muteRelayPin, HIGH);  // Zapnout mute
    }
    activeInput = index;
    lastInputChange = millis();  // cas pro jas RGB
    muteEndTime = millis() + 2000;
    for (int i = 0; i < 6; i++) {
    digitalWrite(relayInputPins[i], (i == index));
  }
  updateRGB();
}

// === Mute časovač ===
void handleMute() {
  if (muteEndTime != 0 && millis() > muteEndTime) {
    digitalWrite(muteRelayPin, LOW);
    muteEndTime = 0;
  }
}

// === Zapnutí/vypnutí ===
void togglePower(bool state) {
  powerState = state;
  digitalWrite(powerRelayPin, state ? HIGH : LOW);

  if (!state) {
    poweringUp = false; // Vypnutí – deaktivace stavu
    for (int i = 0; i < 6; i++) digitalWrite(relayInputPins[i], LOW);
    speakerState = digitalRead(speakerRelayPin);  // Ulož stav reproduktoru
    digitalWrite(speakerRelayPin, LOW);
    digitalWrite(muteRelayPin, LOW);
    analogWrite(redPin, 255);
    analogWrite(greenPin, 255);
    analogWrite(bluePin, 255);
    setTimer(0);
    lastActivity = 0;
    Serial.println(F("POWER_OFF"));
  } else {
    poweringUp = true;
    powerOnTime = millis(); // Zapnutí – ulož čas
    lastActivity = millis();
    Serial.println(F("POWER_ON"));

    // Ochrana proti příliš hlasitému startu, nebo nulovemu volume
    int currentVolumePercent = getVolumePercent();
    if (currentVolumePercent > 45) {
      moveMotorToTarget(20);  // Plynulé ztišení na 20 %
    }
    if (currentVolumePercent < 5) {
      moveMotorToTarget(10);  // Zesílení na 10%
    }

    setInput(activeInput);
    digitalWrite(speakerRelayPin, speakerState);  // Obnov stav reproduktoru
  }
}



// === RGB indikace vstupu ===
void updateRGB() {
  if (!powerState) {
    analogWrite(redPin, 255);
    analogWrite(greenPin, 255);
    analogWrite(bluePin, 255);
    return;
  }

  int r = 255, g = 255, b = 255;
  switch (activeInput) {
    case 0: g = 0; break;               // zelená
    case 1: r = 60; g = 0; break;       // žlutá
    case 2: b = 0; break;               // modrá
    case 3: b = 0; g = 0; break;        // tyrkys
    case 4: r = 0; g = 200; break;      // oranžová
    case 5: r = 0; b = 0; break;        // fialová
  }

  // určení, jestli jsme v režimu "plného jasu" nebo "ztlumeného"
  bool bright = (millis() - lastInputChange < brightDuration);

  float factor = bright ? 0.7 : 0.08;   // 30 % nebo 8 % původní intenzity

  r = 255 - ((255 - r) * factor);
  g = 255 - ((255 - g) * factor);
  b = 255 - ((255 - b) * factor);

  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);
}

// === Tlačítková logika ===
const unsigned long debounceDelay = 50;
unsigned long lastButtonReadTime = 0;

void handleButtons() {
  static Button lastStableButton = BTN_NONE;
  static unsigned long lastDebounceTime = 0;
  static const unsigned long debounceDelay = 50;

  Button currentButton = getButtonFromADC(analogRead(buttonPanelPin));
  unsigned long now = millis();

  // Detekce změny tlačítka
  if (currentButton != lastButton) {
    lastDebounceTime = now;
    lastButton = currentButton;
  }

  // Po uplynutí debounce doby bereme změnu jako platnou
  if ((now - lastDebounceTime) > debounceDelay) {
    if (lastStableButton == BTN_NONE && currentButton != BTN_NONE) {
      // Tlačítko právě stisknuto
      lastActivity = millis();
      //Serial.println("RESET HLAVNIHO CASOVACE");
      buttonPressTime = now;
      buttonHeld = false;
      buttonActionTaken = false;
      lastStableButton = currentButton;

    } else if (lastStableButton != BTN_NONE && currentButton == BTN_NONE) {
      // Tlačítko právě uvolněno
      unsigned long pressDuration = now - buttonPressTime;

      if (!buttonActionTaken) {
        if (pressDuration >= longPressThreshold) {
          // Dlouhý stisk
          if (powerState) {
            if (lastStableButton == BTN_VOLUP || lastStableButton == BTN_VOLDOWN) {
              volumeButtonHeld = true;
              heldVolumeButton = lastStableButton;
              lastStepTime = now;
            } else {
              handleLongPress(lastStableButton);
            }
          }
        } else {
          // Krátký stisk
          if (powerState || lastStableButton == BTN_POWER) {
            handleShortPress(lastStableButton);
          }
        }
      }

      // Reset po uvolnění
      longPressVolUpEnd();
      longPressVolDownEnd();
      lastStableButton = BTN_NONE;
      volumeButtonHeld = false;
      heldVolumeButton = BTN_NONE;
      volumeCurrentDelay = 200;  // reset rychlosti
      updateRGB();               // obnov barvu vstupu
    }

    // Při držení tlačítka ručně spustíme dlouhý stisk
    if (!buttonActionTaken && currentButton == lastStableButton && (now - buttonPressTime) > longPressThreshold) {
      buttonActionTaken = true;

      if (powerState) {
        if (lastStableButton == BTN_VOLUP || lastStableButton == BTN_VOLDOWN) {
          if (lastStableButton == BTN_VOLUP) {
          longPressVolUpStart();
          } else longPressVolDownStart();
          
          volumeButtonHeld = true;
          heldVolumeButton = lastStableButton;
          lastStepTime = now;
        } else {
          handleLongPress(lastStableButton);
        }
      }
    }
  }
}


bool isDuplicate(unsigned long code, unsigned long* codeArray, int count) {
  for (int i = 0; i < count; i++) {
    if (codeArray[i] == code) return true;
  }
  return false;
}


// Uceni IR
void handleIRLearning() {
  if (currentIRLearnMode == IR_NONE) return;

  if (millis() - irLearnStartTime > irLearnTimeout) {
    Serial.println(F("IR learning timed out"));
    currentIRLearnMode = IR_NONE;
    return;
  }

  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      unsigned long code = IrReceiver.decodedIRData.decodedRawData;
      Serial.print(F("Received IR code: 0x"));
      Serial.println(code, HEX);

      bool changed = false;

      // Pomocná funkce pro detekci duplicit
      auto isDuplicate = [](unsigned long code, unsigned long* arr, int count) -> bool {
        for (int i = 0; i < count; i++) {
          if (arr[i] == code) return true;
        }
        return false;
      };

      // --- UČENÍ ---
      switch (currentIRLearnMode) {
        case IR_LEARN_POWER:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountPower < maxIrCodes) {
            irCodesPower[irCountPower++] = code;
            changed = true;
          } else {
            Serial.println(F("POWER IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_VOLUP:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountVolUp < maxIrCodes) {
            irCodesVolUp[irCountVolUp++] = code;
            changed = true;
          } else {
            Serial.println(F("VOLUP IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_VOLDOWN:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountVolDown < maxIrCodes) {
            irCodesVolDown[irCountVolDown++] = code;
            changed = true;
          } else {
            Serial.println(F("VOLDOWN IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_SPEAKERS:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountSpeakers < maxIrCodes) {
            irCodesSpeakers[irCountSpeakers++] = code;
            changed = true;
          } else {
            Serial.println(F("SPEAKERS IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_INPUT:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountInput < maxIrCodes) {
            irCodesInput[irCountInput++] = code;
            changed = true;
          } else {
            Serial.println(F("INPUT IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_PLAY:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountPlay < maxIrCodes) {
            irCodesPlay[irCountPlay++] = code;
            changed = true;
          } else {
            Serial.println(F("PLAY IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_NEXT:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountNext < maxIrCodes) {
            irCodesNext[irCountNext++] = code;
            changed = true;
          } else {
            Serial.println(F("NEXT IR codes full, cannot add more."));
          }
          break;

        case IR_LEARN_PREV:
          if (isDuplicate(code, irCodesPower, irCountPower) ||
              isDuplicate(code, irCodesVolUp, irCountVolUp) ||
              isDuplicate(code, irCodesVolDown, irCountVolDown) ||
              isDuplicate(code, irCodesSpeakers, irCountSpeakers) ||
              isDuplicate(code, irCodesInput, irCountInput) ||
              isDuplicate(code, irCodesPlay, irCountPlay) ||
              isDuplicate(code, irCodesNext, irCountNext) ||
              isDuplicate(code, irCodesPrev, irCountPrev)) {
            Serial.println(F("Code already exists in some group. Not added."));
          } else if (irCountPrev < maxIrCodes) {
            irCodesPrev[irCountPrev++] = code;
            changed = true;
          } else {
            Serial.println(F("PREV IR codes full, cannot add more."));
          }
          break;

        // --- ODEBRÁNÍ ---
        case IR_REMOVE_POWER:
          for (int i = 0; i < irCountPower; i++) {
            if (irCodesPower[i] == code) {
              for (int j = i; j < irCountPower - 1; j++) {
                irCodesPower[j] = irCodesPower[j + 1];
              }
              irCountPower--;
              changed = true;
              Serial.println(F("Code removed from POWER."));
              break;
            }
          }
          break;

        case IR_REMOVE_VOLUP:
          for (int i = 0; i < irCountVolUp; i++) {
            if (irCodesVolUp[i] == code) {
              for (int j = i; j < irCountVolUp - 1; j++) {
                irCodesVolUp[j] = irCodesVolUp[j + 1];
              }
              irCountVolUp--;
              changed = true;
              Serial.println(F("Code removed from VOLUP."));
              break;
            }
          }
          break;

        case IR_REMOVE_VOLDOWN:
          for (int i = 0; i < irCountVolDown; i++) {
            if (irCodesVolDown[i] == code) {
              for (int j = i; j < irCountVolDown - 1; j++) {
                irCodesVolDown[j] = irCodesVolDown[j + 1];
              }
              irCountVolDown--;
              changed = true;
              Serial.println(F("Code removed from VOLDOWN."));
              break;
            }
          }
          break;

        case IR_REMOVE_SPEAKERS:
          for (int i = 0; i < irCountSpeakers; i++) {
            if (irCodesSpeakers[i] == code) {
              for (int j = i; j < irCountSpeakers - 1; j++) {
                irCodesSpeakers[j] = irCodesSpeakers[j + 1];
              }
              irCountSpeakers--;
              changed = true;
              Serial.println(F("Code removed from SPEAKERS."));
              break;
            }
          }
          break;

        case IR_REMOVE_INPUT:
          for (int i = 0; i < irCountInput; i++) {
            if (irCodesInput[i] == code) {
              for (int j = i; j < irCountInput - 1; j++) {
                irCodesInput[j] = irCodesInput[j + 1];
              }
              irCountInput--;
              changed = true;
              Serial.println(F("Code removed from INPUT."));
              break;
            }
          }
          break;

        case IR_REMOVE_PLAY:
          for (int i = 0; i < irCountPlay; i++) {
            if (irCodesPlay[i] == code) {
              for (int j = i; j < irCountPlay - 1; j++) {
                irCodesPlay[j] = irCodesPlay[j + 1];
              }
              irCountPlay--;
              changed = true;
              Serial.println(F("Code removed from PLAY."));
              break;
            }
          }
          break;

        case IR_REMOVE_NEXT:
          for (int i = 0; i < irCountNext; i++) {
            if (irCodesNext[i] == code) {
              for (int j = i; j < irCountNext - 1; j++) {
                irCodesNext[j] = irCodesNext[j + 1];
              }
              irCountNext--;
              changed = true;
              Serial.println(F("Code removed from NEXT."));
              break;
            }
          }
          break;

        case IR_REMOVE_PREV:
          for (int i = 0; i < irCountPrev; i++) {
            if (irCodesPrev[i] == code) {
              for (int j = i; j < irCountPrev - 1; j++) {
                irCodesPrev[j] = irCodesPrev[j + 1];
              }
              irCountPrev--;
              changed = true;
              Serial.println(F("Code removed from PREV."));
              break;
            }
          }
          break;
      }

      if (changed) {
        saveIRCodesToEEPROM();
        Serial.println(F("IR code list updated. Learning/removal ended."));
        currentIRLearnMode = IR_NONE;
      } else {
        Serial.println(F("No change made. Try again or use different code."));
      }
    }

    IrReceiver.resume();
  }
}




// === Krátké stisky ===
void handleShortPress(Button b) {
  if (!powerState && b != BTN_POWER) return;  // Zablokuj všechna tlačítka kromě Power v OFF stavu

  switch (b) {
    case BTN_POWER:
      togglePower(!powerState);
      break;

    case BTN_INPUT:
      setInput((activeInput + 1) % 6);
      break;

    case BTN_SPEAKERS:
      speakerState = !speakerState;
      digitalWrite(speakerRelayPin, speakerState);
      break;

    case BTN_VOLUP:
      shortPressVolUp();
      // Spustit krátký pulz
      shortPulseActive = true;
      shortPulseStartTime = millis();
      shortPulseButton = BTN_VOLUP;
      break;

    case BTN_VOLDOWN:
      shortPressVolDown();
      // Spustit krátký pulz
      shortPulseActive = true;
      shortPulseStartTime = millis();
      shortPulseButton = BTN_VOLDOWN;
      break;

    case BTN_PLAY:
      Serial.println(F("SEND_MEDIA_PLAY"));
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();
      break;

    case BTN_NEXT:
      Serial.println(F("SEND_MEDIA_NEXT"));
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();
      break;

    case BTN_PREV:
      Serial.println(F("SEND_MEDIA_PREV"));
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();
      break;

    default:
      break;
  }
}

// === Dlouhé stisky ===
void handleLongPress(Button b) { // Sem můžeš dát jiné případy dlouhých stisků mimo volume
switch (b) {
case BTN_SPEAKERS: // Nastavit max volume pro aktualni vstup
      if (!powerState) {
      Serial.println(F("Cannot set max volume: system is OFF."));
  }     else {
      currentADC = analogRead(potPin);
      int percent = map(currentADC, adcMin, adcMax, 0, 100);
      percent = constrain(percent, 0, 100);
      saveMaxVolumeLimit(activeInput, percent);

      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(1000);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();

      Serial.print(F("Max volume limit for input "));
      Serial.print(activeInput);
      Serial.print(F(" set to current value: "));
      Serial.print(percent);
      Serial.println(F("%"));
      }
      break;

case BTN_PLAY:
    Serial.println(F("SEND_MEDIA_START"));
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
      analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(80);
        analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
        delay(120);
        analogWrite(redPin, 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
        delay(500);
      updateRGB();
      break;

    default:
      break;
  
}
}



int getVolumePercent() {
  int adc = analogRead(potPin);
  adc = constrain(adc, adcMin, adcMax);
  return map(adc, adcMin, adcMax, 0, 100);
}


void handleVolumeLedPulse() {
  if (!powerState) return;
  unsigned long now = millis();
  if (now - lastPulseTime >= pulseInterval) {
    lastPulseTime = now;
    pulseState = !pulseState;

    if (heldVolumeButton == BTN_VOLUP) {
      analogWrite(redPin, pulseState ? 0 : 255);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
    } else if (heldVolumeButton == BTN_VOLDOWN) {
      analogWrite(redPin, 255);
      analogWrite(greenPin, pulseState ? 200 : 255);
      analogWrite(bluePin, pulseState ? 0 : 255);
    }
  }
}

void handleShortVolumeLedPulse() {
  if (!powerState) return;
  if (!shortPulseActive) return;

  unsigned long now = millis();
  if (now - shortPulseStartTime < shortPulseDuration) {
    if (shortPulseButton == BTN_VOLUP) {
      analogWrite(redPin, 0);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 255);
    } else if (shortPulseButton == BTN_VOLDOWN) {
      analogWrite(redPin, 255);
      analogWrite(greenPin, 200);
      analogWrite(bluePin, 0);
    }
  } else {
    shortPulseActive = false;
    updateRGB();
  }
}


void printStoredIRCodes() {
  Serial.println(F("Stored POWER codes:"));
  for (int i = 0; i < irCountPower; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesPower[i], HEX);
  }

  Serial.println(F("Stored VOLUP codes:"));
  for (int i = 0; i < irCountVolUp; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesVolUp[i], HEX);
  }

  Serial.println(F("Stored VOLDOWN codes:"));
  for (int i = 0; i < irCountVolDown; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesVolDown[i], HEX);
  }

  Serial.println(F("Stored SPEAKERS codes:"));
  for (int i = 0; i < irCountSpeakers; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesSpeakers[i], HEX);
  }

  Serial.println(F("Stored INPUT codes:"));
  for (int i = 0; i < irCountInput; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesInput[i], HEX);
  }

  Serial.println(F("Stored PLAY codes:"));
  for (int i = 0; i < irCountPlay; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesPlay[i], HEX);
  }

  Serial.println(F("Stored NEXT codes:"));
  for (int i = 0; i < irCountNext; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesNext[i], HEX);
  }

  Serial.println(F("Stored PREV codes:"));
  for (int i = 0; i < irCountPrev; i++) {
    Serial.print(F("0x"));
    Serial.println(irCodesPrev[i], HEX);
  }
}


void handleIRButtons() {
  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    IrReceiver.resume();

    // Najdi, které tlačítko odpovídá
    Button b = BTN_NONE;
    for (int i = 0; i < irCountPower; i++) {
      if (irCodesPower[i] == code) b = BTN_POWER;
    }
    for (int i = 0; i < irCountVolUp; i++) {
      if (irCodesVolUp[i] == code) b = BTN_VOLUP;
    }
    for (int i = 0; i < irCountVolDown; i++) {
      if (irCodesVolDown[i] == code) b = BTN_VOLDOWN;
    }
    for (int i = 0; i < irCountSpeakers; i++) {
      if (irCodesSpeakers[i] == code) b = BTN_SPEAKERS;
    }
    for (int i = 0; i < irCountPlay; i++) {
      if (irCodesPlay[i] == code) b = BTN_PLAY;
    }
    for (int i = 0; i < irCountNext; i++) {
      if (irCodesNext[i] == code) b = BTN_NEXT;
    }
    for (int i = 0; i < irCountPrev; i++) {
      if (irCodesPrev[i] == code) b = BTN_PREV;
    }
    for (int i = 0; i < irCountInput; i++) {
      if (irCodesInput[i] == code) b = BTN_INPUT;
    }

    if (b != BTN_NONE) {
      unsigned long now = millis();
      if (b != lastIrButton || now - lastIrReceiveTime > irRepeatThreshold * 2) {
        // Nový stisk
        lastActivity = millis();
        //Serial.println("RESET HLAVNIHO CASOVACE");
        irPressStartTime = now;
        irButtonHeld = false;
        irActionTaken = false;
        lastIrButton = b;
      }

      lastIrReceiveTime = now;
    }
  }

  // Zpracuj dlouhý nebo krátký stisk
  if (lastIrButton != BTN_NONE) {
    unsigned long now = millis();

    if (!irActionTaken && now - irPressStartTime > irHoldThreshold) {
      // Dlouhý stisk
      irActionTaken = true;
      if (powerState) {
        if (lastIrButton == BTN_VOLUP || lastIrButton == BTN_VOLDOWN) {
          if (lastIrButton == BTN_VOLUP) {
          longPressVolUpStart();
          } else longPressVolDownStart();
          volumeButtonHeld = true;
          heldVolumeButton = lastIrButton;
          lastStepTime = now;
        } else {
          handleLongPress(lastIrButton);
        }
      }
    }

    // Pokud IR signál přestal chodit
    if (now - lastIrReceiveTime > irRepeatThreshold * 2) {
      if (!irActionTaken) {
        // Krátký stisk
        if (powerState || lastIrButton == BTN_POWER) {
          handleShortPress(lastIrButton);
        }
      }

      // Reset
      if (!motorImpulseActive) {
      longPressVolUpEnd();
      longPressVolDownEnd();
      }
      lastIrButton = BTN_NONE;
      irButtonHeld = false;
      volumeButtonHeld = false;
      heldVolumeButton = BTN_NONE;
      volumeCurrentDelay = 200;
      updateRGB();
    }
  }
}



// Vrátí index kódu v poli, nebo -1 pokud nenalezen
int findIrCodeInArray(unsigned long code, unsigned long* arr, int count) {
  for (int i = 0; i < count; i++) {
    if (arr[i] == code) return i;
  }
  return -1;
}

// Smaže kód na indexu z pole, posune zbytek a sníží počet
void removeIrCodeFromArray(int index, unsigned long* arr, int& count) {
  if (index < 0 || index >= count) return;
  for (int i = index; i < count - 1; i++) {
    arr[i] = arr[i + 1];
  }
  count--;
}

void stopMotor() {
  analogWrite(pwmPin, 0);
  digitalWrite(dirUpPin, LOW);
  digitalWrite(dirDownPin, LOW);
}

// Funkce pro řízení podle procent (WiFi příkaz)
void moveMotorToTarget(int percent) {
  percent = constrain(percent, 0, 100);
  targetADC = map(percent, 0, 100, adcMin, adcMax);
  moveMotorToTarget(); // zavolá verzi bez parametru
}

void moveMotorToTarget() {
  int adc = analogRead(potPin);
  currentADC = adc;
  
  int diff = targetADC - currentADC;
  if (abs(diff) < 6) { // dosaženo
    stopMotor();
    movingToTarget = false;
    return;
  }

  // PWM se zpomalením a dolní hranicí
  float fraction = abs(diff) / float(adcMax - adcMin); // 0.0 – 1.0
int pwm = minPWM + (maxPWM - minPWM) * pow(fraction, 3); // sqrt zpomalení
pwm = constrain(pwm, minPWM, maxPWM);

  if (diff > 0) {
    digitalWrite(dirDownPin, LOW);
    delay(50);  // případně zkrátit nebo zrušit
    digitalWrite(dirUpPin, HIGH);
  } else {
    digitalWrite(dirUpPin, LOW);
    delay(50);  // případně zkrátit nebo zrušit
    digitalWrite(dirDownPin, HIGH);
  }

  analogWrite(pwmPin, pwm);
  movingToTarget = true;
}


void shortPressVolUp() {
  motorImpulseDirectionUp = true;
  startMotorImpulse();
}

void shortPressVolDown() {
  motorImpulseDirectionUp = false;
  startMotorImpulse();
}

void longPressVolUpStart() {
  accelStartTime = millis();
  digitalWrite(dirDownPin, LOW);
  delay(50);
  digitalWrite(dirUpPin, HIGH);
}

void longPressVolUpLoop() {
  int currentPercent = map(analogRead(potPin), adcMin, adcMax, 0, 100);
    if (currentPercent >= maxInputVol[activeInput]) {
      longPressVolUpEnd();
      return;
}
  int elapsed = millis() - accelStartTime;
  int pwm = map(elapsed, 0, accelDuration, 120, maxPWM);
  pwm = constrain(pwm, 120, maxPWM);
  analogWrite(pwmPin, pwm);
}

void longPressVolUpEnd() {
  if (!motorImpulseActive) {stopMotor();}
  currentADC = analogRead(potPin); // nová výchozí pozice
}

void longPressVolDownStart() {
  accelStartTime = millis();
  digitalWrite(dirUpPin, LOW);
  delay(50);
  digitalWrite(dirDownPin, HIGH);
}

void longPressVolDownLoop() {
  int elapsed = millis() - accelStartTime;
  int pwm = map(elapsed, 0, accelDuration, 120, maxPWM);
  pwm = constrain(pwm, 120, maxPWM);
  analogWrite(pwmPin, pwm);
}

void longPressVolDownEnd() {
  if (!motorImpulseActive) {stopMotor();}
  currentADC = analogRead(potPin); // nová výchozí pozice
}

void checkMaxVolLimit() {
  int currentPercent = map(analogRead(potPin), adcMin, adcMax, 0, 100);
  int allowedMax = maxInputVol[activeInput];
  
  if (currentPercent > allowedMax) {
    if (currentPercent > allowedMax + 5) {
          digitalWrite(muteRelayPin, HIGH);
          muteActive = true;
              }
    moveMotorToTarget(allowedMax);  // snížíme hlasitost
  } else if (muteActive && currentPercent <= allowedMax) {
    digitalWrite(muteRelayPin, LOW);
    muteActive = false;
    stopMotor();
    movingToTarget = false;
  }
}


void startMotorImpulse() {
  int pwm = (minPWM + maxPWM) / 2;  // konstantní rychlost

  if (motorImpulseDirectionUp) {
    digitalWrite(dirDownPin, LOW);
    delay(50);
    digitalWrite(dirUpPin, HIGH);
  } else {
    digitalWrite(dirUpPin, LOW);
    delay(50);
    digitalWrite(dirDownPin, HIGH);
  }
  motorImpulseEndTime = millis() + 100;
  motorImpulseActive = true;
  analogWrite(pwmPin, pwm);
}

// --- Funkce pro nastavení časovače ---
void setTimer(unsigned long sekundy) {
  if (sekundy > 0) {
    timerEnd = millis() + (sekundy * 1000UL);
    timerActive = true;
    Serial.print("Časovač nastaven na ");
    Serial.print(sekundy);
    Serial.println(" sekund.");
  } else {
    timerActive = false;
    Serial.println("Časovač vypnut.");
  }
}


// --- Funkce pro kontrolu časovače, volat v loop() ---
void checkTimer() {
  if (timerActive && millis() >= timerEnd) {
    Serial.println("Časovač vypršel, vypínám zesilovač...");
    if (powerState) {
      togglePower(!powerState);   // vypne zesilovač
    }
    timerActive = false; // deaktivace časovače po použití
  }
}

// === Hlavní smyčka ===
void loop() {

  handleButtons();
  handleMute();
  checkWifiSerial();
  handleShortVolumeLedPulse();
  handleIRLearning();
  handleIRButtons();
  if(powerState && !poweringUp) {
    checkMaxVolLimit();
    }

  if (movingToTarget) {
  moveMotorToTarget();
}

if (poweringUp && millis() - powerOnTime >= 4000) {
  poweringUp = false;
}



if (volumeButtonHeld) {
    handleVolumeLedPulse();  // <- přidáno zde

    if (millis() - lastStepTime >= volumeCurrentDelay) {
      lastStepTime = millis();
      if (heldVolumeButton == BTN_VOLUP) {
        longPressVolUpLoop();
      } else if (heldVolumeButton == BTN_VOLDOWN) {
        longPressVolDownLoop();
      }
    }
  }



if (motorImpulseActive && millis() >= motorImpulseEndTime) {
  stopMotor();
  motorImpulseActive = false;
}

if (millis() - lastInputChange > brightDuration && lastInputChange != 0){
    lastInputChange = 0;
    updateRGB();
}

if (powerState && millis() - lastActivity >= autoOffTime) {
    Serial.println(F("3 hodiny neaktivity, vypínám zesilovač..."));
    togglePower(!powerState);
  }

//lastActivity = millis();
//Serial.println("RESET HLAVNIHO CASOVACE");

checkTimer();

  // Button btn = getButtonFromADC(analogRead(A4));
  // Serial.print(F("Button: "));
  // Serial.println(btn);
}
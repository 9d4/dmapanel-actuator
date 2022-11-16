#include <EEPROM.h>

const uint8_t SERIAL_COMM_TERMINATOR = 255;
const uint8_t PIN_OUTPUT[] = {
  2,
  3,
  4,
  5,
  // 6, LED INDICATOR
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15,
  16,
  17,
  18,
  19,
  20
};


struct SerialCommand {
  char cmd;
  char arg1;
  char arg2;
};

void flushSerial() {
  while (Serial.available() > 0) Serial.read();
}

void saveSerialCmd(SerialCommand sc) {
  static unsigned int pinsLength = sizeof(PIN_OUTPUT);
  uint8_t eeAddr = getPinIndex((uint8_t)sc.arg1) * sizeof(SerialCommand) + 1;  // 1 is gap

  EEPROM.put(eeAddr, sc);
}

SerialCommand getSerialCmd(int pin) {
  SerialCommand sc;
  EEPROM.get((getPinIndex(pin) * sizeof(SerialCommand) + 1), sc);

  return sc;
}

// Return true if it find '\n'
bool scanSerialCommand(SerialCommand *psc) {
  static SerialCommand sc;
  static uint8_t counter = 0;

  while (Serial.available() > 0) {
    counter++;

    char c = Serial.read();
    if (c == (char)SERIAL_COMM_TERMINATOR) {
      flushSerial();

      *psc = sc;

      counter = 0;
      sc = SerialCommand{};

      return true;
    }

    switch (counter) {
      case 1:
        sc.cmd = c;
        psc->cmd = c;
        break;
      case 2:
        sc.arg1 = c;
        psc->arg1 = c;
        break;
      case 3:
        sc.arg2 = c;
        psc->arg2 = c;
        break;
    }
  }

  return false;
}

void executeSerialCommand(SerialCommand sc) {
  switch (sc.cmd) {
    case 'A':
      // A stands for analog.
      // arg1 is the pin to be controlled
      // arg2 is the pin state 0x00 to 0xff
      analogWrite((uint8_t)sc.arg1, sc.arg2);
      saveSerialCmd(sc);
      Serial.write('A');
      break;
    case 'C':
      // C stands for check. Check if actuator is ready
      Serial.write('C');
      break;
    case 'S':
      // S stands for switch
      // arg1 is the pin to be controlled
      // arg2 is the pin state 0x0 or 0x1
      digitalWrite((uint8_t)sc.arg1, sc.arg2);
      saveSerialCmd(sc);
      Serial.write('S');
      break;
    case 'P':
      // P stand for pin mode
      // arg1 is the pin to be controlled
      // arg2 is the pin mode
      pinMode((uint8_t)sc.arg1, sc.arg2);
      Serial.write('P');
      break;
  }
}

void restoreAllPinState() {
  for (uint16_t i = 0; i < sizeof(PIN_OUTPUT); i++) {
    SerialCommand sc = getSerialCmd(PIN_OUTPUT[i]);
    executeSerialCommand(sc);
  }
  Serial.println("Pins state restored.");
}

void setupPins() {
  for (uint16_t i = 0; i < sizeof(PIN_OUTPUT); i++) {
    pinMode((uint8_t)PIN_OUTPUT[i], OUTPUT);
  }
}

// get pin index in PIN_OUTPUT[]
int8_t getPinIndex(uint8_t pin) {
  volatile int8_t index = -1;

  for (uint16_t i; i < sizeof(PIN_OUTPUT); i++) {
    if (PIN_OUTPUT[i] == pin) {
      index = i;
      break;
    }
  }

  return index;
}

void waveIndicator() {
  static uint8_t state = 1;
  static uint8_t direction = 1;
  static uint32_t lastTime = 0;
  static uint32_t lastCycle = 0;
  static uint16_t delay = 15;
  static uint16_t cycleDelay = 2000;

  if (millis() - lastTime > delay) {
    lastTime = millis();

    if (millis() - lastCycle < cycleDelay) {
      return;
    }

    state += direction;
    analogWrite(6, state);

    if (state > 128) {
      direction = -1;
    }

    if (state < 1) {
      direction = 1;
      lastCycle = millis();
    }
  }
}

void eraseEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }  
}

void eraseEEPROMButton() {
  static uint32_t lastTimePress = 0;
  static uint16_t lastValue = 0;
  static uint16_t resetVal = 1023;
  static uint16_t del = 3000;

  uint16_t nowVal = analogRead(21);
  
  if (nowVal == 1023) {
    if (lastValue != nowVal) {
      lastTimePress = millis();
      lastValue = nowVal;
      Serial.println("Press 3 seconds to erase EEPROM");
    }

    if (millis() - lastTimePress > del) {
      eraseEEPROM();
      Serial.println("reset");
      delay(1000);
      return;
    }

    lastValue = nowVal;
    return;
  }

  if (nowVal < resetVal) {
    lastValue = nowVal;
  }
}

void setup() {
  EEPROM.begin();
  Serial.begin(9600);
  delay(100);
  Serial.println("Initializing...");
  setupPins();
  restoreAllPinState();
  Serial.println("Initialization Done.");
}

void loop() {
  SerialCommand sc;
  bool done = scanSerialCommand(&sc);
  if (done) {
    executeSerialCommand(sc);
  }
  waveIndicator();
  eraseEEPROMButton();
}